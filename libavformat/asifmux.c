/*
 * Audio Processing Technology codec for NON-Audiophiles 
 *
 * Copyright (C) 2020  Gavin Gray <gavinleroy6@gmail.com>
 * Copyright (C) 2020  Dan Ruley  <drslc14@gmail.com>
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
 */

#include <stdio.h>

#include "libavutil/intreadwrite.h"

#include "avformat.h"
#include "internal.h"



#include <stdint.h>
#include <string.h>

#include "libavutil/avstring.h"
#include "libavutil/dict.h"
#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/time_internal.h"

#include "avformat.h"
#include "avio.h"
#include "avio_internal.h"
#include "internal.h"
#include "riff.h"

#define FREQUENCY_OFFSET 4
#define CHANNELS_OFFSET 8
#define SAMPLES_OFFSET 10
#define START_OFFSET 14

#define INITIAL_SIZE 1024
#define MAX_CHANNELS 200

typedef struct{
    int32_t samples;
    int32_t channels;
    int *max;
    int *ind;
    uint8_t **data;
} ASIFContext;

static void add_data(ASIFContext *ctx, int channel, int size, uint8_t *src){

    if(size + ctx->ind[channel] >= ctx->max[channel]){ ///< Resize and copy over data
        ctx->max[channel] *= 2; ///< Increase our max size
        uint8_t *new_data = (uint8_t *) malloc(ctx->max[channel] * sizeof(uint8_t));
        for(int i = 0; i < ctx->ind[channel]; i++) ///< Copy all old data over
            new_data[i] = ctx->data[channel][i]; 

        free(ctx->data[channel]);
        ctx->data[channel] = new_data;
    }

    ///<Copy new data into buffer
    int i;
    for(i = ctx->ind[channel]; i-ctx->ind[channel] < size; i++)
        ctx->data[channel][i] = *src++;

    ctx->ind[channel] += i - ctx->ind[channel];
}

static void init_data(ASIFContext *ctx){

    ctx->data = (uint8_t **) malloc(MAX_CHANNELS * sizeof(uint8_t*));
    ctx->max = (int *) malloc(MAX_CHANNELS * sizeof(int));
    ctx->ind = (int *) malloc(MAX_CHANNELS * sizeof(int));

    for(int i = 1; i < MAX_CHANNELS; i++){ ///< Start at 1 because channel(0) is already written
        ctx->data[i] = (uint8_t *) malloc(INITIAL_SIZE * sizeof(uint8_t));
	ctx->max[i] = INITIAL_SIZE;
	ctx->ind[i] = 0;
    }
}

/* Write header data for the .asif file. */
static int asif_write_header(AVFormatContext *s){

    ASIFContext *ctx = s->priv_data;
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

/* Writes the frame of audio data contained inside the AVPacket */
static int asif_write_packet(AVFormatContext *s, AVPacket *pkt){

    ASIFContext *ctx = s->priv_data;

    ///< The number of sampels per channel in this packet
    int packet_samples = pkt->size / ctx->channels;
    
    uint8_t *src = pkt->data;

    for(int i = 0; i < ctx->channels; i++) {
	if(i > 0){ ///< Append to the right vector for later
	    add_data(ctx, i, packet_samples, src); 
	} else { ///< Write the first channel to the file
            avio_seek(s->pb, START_OFFSET + ctx->samples, SEEK_SET);    
            avio_write(s->pb, src, packet_samples);
        }
	src += packet_samples;
    }
    ctx->samples += packet_samples;

    return 0;
}

static void write_remaining_channels(ASIFContext *ctx, AVIOContext *pb){
    ///< Set stream after the first channel written.
    for(int c = 1; c < ctx->channels; c++){
        avio_seek(pb, START_OFFSET + (c * ctx->samples), SEEK_SET);
        avio_write(pb, ctx->data[c], ctx->samples);    
    }
}

static int asif_write_trailer(AVFormatContext *s){
    ASIFContext *ctx = s->priv_data;

    /* update total number of samples in the first block */
    if ((s->pb->seekable & AVIO_SEEKABLE_NORMAL) && ctx->samples){
        int64_t pos = avio_tell(s->pb);
        avio_seek(s->pb, SAMPLES_OFFSET, SEEK_SET);
        avio_wl32(s->pb, ctx->samples / ctx->channels);
        avio_seek(s->pb, pos, SEEK_SET);
    }

    write_remaining_channels(ctx, s->pb);

    ///< Free up all data used by ASIFContext
    for(int i = 1; i < MAX_CHANNELS; i++){
	free(ctx->data[i]);
	ctx->max[i] = -1;
	ctx->ind[i] = -1;
    }
    free(ctx->max);
    free(ctx->ind);
    free(ctx->data);
    return 0;
}

AVOutputFormat ff_asif_muxer = {
    .name              = "asif",
    .long_name         = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 2020)"),
    .extensions        = "asif",
    .priv_data_size    = sizeof(ASIFContext),
    .audio_codec       = AV_CODEC_ID_ASIF,
    .video_codec       = AV_CODEC_ID_NONE,
    .write_header      = asif_write_header,
    .write_packet      = asif_write_packet,
    .write_trailer     = asif_write_trailer,
    .flags             = AVFMT_NOTIMESTAMPS,
};
