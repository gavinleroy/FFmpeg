/*
 * Audio Processing Technology codec for NON-Audiophiles 
 *
 * Copyright (C) 2020  Gavin Gray <gavinleroy6@gmail.com>
 *
 * asifmux.c:
 * The asif muxer receives samples from the asif encoder and writes them
 * to an .asif file per the sepcifications of the .asif file format.
 *
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "avio_internal.h"
#include "internal.h"

#define FREQUENCY_OFFSET 4
#define CHANNELS_OFFSET 8
#define SAMPLES_OFFSET 10
#define START_OFFSET 14

#define INITIAL_SIZE 1
#define MAX_CHANNELS 200

/*Struct that stores sample data and information about the file*/
typedef struct{
    int32_t samples;
    int32_t channels;
    int *max;
    int *ind;
    uint8_t **data;
} ASIFMuxContext;

/*Add new sample data to our internal buffers and reallocate memory as needed*/
static void add_data(ASIFMuxContext *ctx, int channel, int size, uint8_t *src){

    if(size + ctx->ind[channel] >= ctx->max[channel]){ ///< Resize and copy over data
	while(size + ctx->ind[channel] >= ctx->max[channel]) ///< Make sure that new size is large enough
            ctx->max[channel] *= 2; ///< Increase our max size
        uint8_t *new_data = (uint8_t *) malloc(ctx->max[channel] * sizeof(uint8_t));
        for(int i = 0; i < ctx->ind[channel]; i++) ///< Copy all old data over
            new_data[i] = ctx->data[channel][i]; 

        free(ctx->data[channel]); ///< Free old data
        ctx->data[channel] = new_data; ///< Swap pointers
    }

    ///<Copy new data into buffer
    int i;
    for(i = ctx->ind[channel]; i-ctx->ind[channel] < size; i++)
        ctx->data[channel][i] = *src++;

    ctx->ind[channel] += i - ctx->ind[channel];
}

/*Allocate memory for the internal data we are using to store samples.  Also initialize default values.*/
static void init_data(ASIFMuxContext *ctx){

    ctx->data = (uint8_t **) malloc(MAX_CHANNELS * sizeof(uint8_t *)); ///<Stores the actual sample data
    ctx->max = (int *) malloc(MAX_CHANNELS * sizeof(int));             ///<Max size - for dynamic reallocation
    ctx->ind = (int *) malloc(MAX_CHANNELS * sizeof(int));             ///<Index - current index of last sample in dynamic array

    for(int i = 0; i < MAX_CHANNELS; i++){
        ctx->data[i] = (uint8_t *) malloc(INITIAL_SIZE * sizeof(uint8_t));
	ctx->max[i] = INITIAL_SIZE;
	ctx->ind[i] = 0;
    }
}

/* Write header data for the .asif file. */
static int asif_write_header(AVFormatContext *s){

    ASIFMuxContext *ctx = s->priv_data;
    AVCodecParameters *par = s->streams[0]->codecpar;

    ///<Write the 'asif' header
    avio_seek(s->pb, 0, SEEK_SET);
    ffio_wfourcc(s->pb, "asif");
    
    ///<Write the sample frequency
    avio_seek(s->pb, FREQUENCY_OFFSET, SEEK_SET);
    avio_wl32(s->pb, par->sample_rate);
    
    ///<Write the number of channels
    avio_seek(s->pb, CHANNELS_OFFSET, SEEK_SET);
    avio_wl16(s->pb, par->channels);

    ///<Write the samples per channel
    avio_seek(s->pb, SAMPLES_OFFSET, SEEK_SET);
    avio_skip(s->pb, START_OFFSET - SAMPLES_OFFSET);

    avio_seek(s->pb, START_OFFSET, SEEK_SET);
    ///< Set the Information in our context
    ctx->samples = 0;
    ctx->channels = par->channels;

    init_data(ctx); ///< initialize data arrays.
    
    return 0;
}

/*Receives packets from the encoder and stores them in an extended buffer.  This may not be ideal,
  but this way we know the exact samples/channel value to write into the file header.*/
static int asif_write_packet(AVFormatContext *s, AVPacket *pkt){

    ASIFMuxContext *ctx = s->priv_data;

    ///< The number of sampels per channel in this packet
    int packet_samples = pkt->size / ctx->channels;
    
    uint8_t *src = pkt->data;

    for(int i = 0; i < ctx->channels; i++) {
        /* All samples are stored in the data buffers,
	 * after all samples are received the deltas are computed
	 * and written to the file.  
	 */
	add_data(ctx, i, packet_samples, src); 

	src += packet_samples;
    }
    ctx->samples += packet_samples;

    return 0;
}


/*Computes the delta values for the n-1 samples preceding the initial sample for each channel*/
static void compute_deltas(ASIFMuxContext *ctx, int8_t *deltas, int c) {

    ///<Diff is the difference between ith and i-1th sample.
    int diff = 0;
    ///<Cary is any "catch up" from prior deltas that were outside [-128,127]
    int carry = 0;

    ///<If the diff + carry is outside of the valid range, we clamp it and put the remaining
    ///<delta in carry for next time.
     for(int i = 1; i < ctx->ind[c]; i++) {
         diff = ctx->data[c][i] - ctx->data[c][i-1] + carry;
         if (diff < -128 || diff > 127) {
             carry = diff < -128 ? diff + 128 : diff - 127;
             diff = diff < -128 ? -128 : 127;
         }
         deltas[i - 1] = diff;
         diff = 0;
    }
}

/*Helper function that writes data contained in our buffer of samples into the .asif file*/
static void write_channel_data(ASIFMuxContext *ctx, AVIOContext *pb){

    int8_t *deltas = (int8_t *) malloc((ctx->samples-1) * sizeof(int8_t));

    ///<Scan to the right position
    avio_seek(pb, START_OFFSET, SEEK_SET); 

    ///<Write the first sample as unsigned for each channel, then the deltas
    for(int c = 0; c < ctx->channels; c++){
        compute_deltas(ctx, deltas, c);
        avio_w8(pb, ctx->data[c][0]);   
        avio_write(pb, deltas, ctx->samples - 1);    
    }
    free(deltas);
}

/*Writes all of the buffered samples to the file, then free any memory we had allocated.*/
static int asif_write_trailer(AVFormatContext *s){
  
    ASIFMuxContext *ctx = s->priv_data;

    /* update total number of samples in the first block */
    if ((s->pb->seekable & AVIO_SEEKABLE_NORMAL) && ctx->samples){
        int64_t pos = avio_tell(s->pb);
        avio_seek(s->pb, SAMPLES_OFFSET, SEEK_SET);
        avio_wl32(s->pb, ctx->samples);
        avio_seek(s->pb, pos, SEEK_SET);
    }
    
    ///< Write all the channels to the file.
    write_channel_data(ctx, s->pb);

    ///< Free up all data used by ASIFMuxContext
    for(int i = 0; i < MAX_CHANNELS; i++){
	free(ctx->data[i]);
	ctx->max[i] = -1;
	ctx->ind[i] = -1;
    }

    free(ctx->max);
    free(ctx->ind);
    free(ctx->data);
    return 0;
}

/*Supply information to FFMPEG about our muxer: functions, private data size, etc.*/
AVOutputFormat ff_asif_muxer = {
    .name              = "asif",
    .long_name         = NULL_IF_CONFIG_SMALL("ASIF audio file"),
    .extensions        = "asif",
    .priv_data_size    = sizeof(ASIFMuxContext),
    .audio_codec       = AV_CODEC_ID_ASIF,
    .video_codec       = AV_CODEC_ID_NONE,
    .write_header      = asif_write_header,
    .write_packet      = asif_write_packet,
    .write_trailer     = asif_write_trailer,
    .flags             = AVFMT_NOTIMESTAMPS,
};
