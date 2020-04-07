/*
 * Audio Processing Technology demuxer for NON-Audiophiles 
 *
 * Copyright (C) 2020  Gavin Gray <gavinleroy6@gmail.com>
 * Copyright (C) 2020  Dan Ruley  <drslc14@gmail.com>
 *
 * asifdemux.c:
 * The asif demuxer reads data from .asif files and stores them in
 * packets for use by the asif decoder.
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
 */


#include "internal.h"

#define SAMPLE_OFFSET 14
#define ASIF_TAG MKTAG('a', 's', 'i', 'f')
#define MAX_SAMPLES 1024

/*Simple struct that keeps track of some relevant values*/
typedef struct ASIFDemuxerContext {
    int32_t current;
    int32_t samples;
    int32_t rate; 
    int16_t channels;
} ASIFDemuxerContext;

/*Reads the header information out of an .asif file and performs error checking.*/
static int asif_read_header(AVFormatContext *s)
{

    ASIFDemuxerContext *ctx = s->priv_data;
    AVStream *st = NULL;
    AVIOContext *pb = s->pb;
    
    ctx->samples = 0; 
    ctx->current = 0; 
    ctx->rate = 0; 
    ctx->channels = 0;

    /*Check for valid asif tag at beginning of file*/
    if (avio_rl32(pb) != ASIF_TAG) {
        av_log(s, AV_LOG_ERROR, "Required format of 'aisf' for demuxing.\n");
        return AVERROR_INVALIDDATA;
    }

    /* Reading Header Info, assuming all goes well :) */
    ctx->rate = avio_rl32(pb);

    ctx->channels = avio_rl16(pb);
    
    ctx->samples = avio_rl32(pb);

    /* In the case of an error, return that we got invalid data */
    if(!ctx->rate || !ctx->channels || !ctx->samples) 
        return AVERROR_INVALIDDATA;

    st = avformat_new_stream(s, NULL);
    if (!st)
        return AVERROR(ENOMEM);
 
    /*Supply relevant information to the Stream. */
    st->codecpar->codec_type     = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id       = AV_CODEC_ID_ASIF;
    st->codecpar->sample_rate    = ctx->rate;
    st->codecpar->channels       = ctx->channels;

    return 0;
}

/*Reads sample data from the .asif file and builds an AVPacket which will then
 *be sent to the asif decoder.*/
static int asif_read_packet(AVFormatContext *s, AVPacket *pkt)
{

    ASIFDemuxerContext *ctx = s->priv_data;
    int ret, size, append; 

    if(ctx->current >= ctx->samples)
        return AVERROR_EOF;

    ///< Compute ideal read size of every 62ms.
    size = FFMAX(ctx->rate / 25, 1);
    ///< Clamp that to our max number of samples
    size = FFMIN(size, MAX_SAMPLES);
    ///< Clamp to total number of samples that we have
    size = (size + ctx->current > ctx->samples) ? ctx->samples - ctx->current : size;

    avio_seek(s->pb, SAMPLE_OFFSET + ctx->current, SEEK_SET); 
    ret = av_get_packet(s->pb, pkt, size);
    if (ret < 0)
        return ret;
    
    ///< For each *additional* channel in channels:
    for (int i = 1; i < ctx->channels; i++) {
        ///< Seek to next channel at where we left off (ctx->current)
        avio_seek(s->pb, SAMPLE_OFFSET + (i * ctx->samples) + ctx->current, SEEK_SET); 
        ///< Append channel samples to packet
	append = av_append_packet(s->pb, pkt, size);
	ret += append;
	if(append < 0)
	    return append;
    }

    ///< Increment the number of samples that we've read.
    ctx->current += size;

    pkt->flags &= ~AV_PKT_FLAG_CORRUPT;
    pkt->stream_index = 0;

    return ret;
}

/* Supply FFMPEG with information about our demuxer: function calls, format name, etc.*/
AVInputFormat ff_asif_demuxer = {
    .name           = "asif",
    .long_name      = NULL_IF_CONFIG_SMALL("ASIF audio file"),
    .priv_data_size = sizeof(ASIFDemuxerContext),
    .read_header    = asif_read_header,
    .read_packet    = asif_read_packet,
    .extensions     = "asif",
};
