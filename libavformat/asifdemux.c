/*
 * Audio Processing Technology demuxer for NON-Audiophiles 
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


#include "internal.h"
#include "avformat.h"
#include "rawdec.h"

#define ASIF_SAMPLE_SIZE   1
#define ASIF_BLOCK_SIZE  (1*ASIF_SAMPLE_SIZE)

#define SAMPLE_OFFSET 14
#define ASIF_TAG MKTAG('a', 's', 'i', 'f')
#define MAX_SAMPLES 1024

typedef struct ASIFDemuxerContext {
    int32_t current;
    int32_t samples;
    int32_t rate; 
    int16_t channels;
} ASIFDemuxerContext;

static int asif_read_header(AVFormatContext *s)
{

    ASIFDemuxerContext *asif_c = s->priv_data;
    AVStream *st = NULL;
    AVIOContext *pb = s->pb;
    
    asif_c->samples = 0; 
    asif_c->current = 0; 
    asif_c->rate = 0; 
    asif_c->channels = 0;

    /*Check for valid asif tag at beginning of file*/
    if (avio_rl32(pb) != ASIF_TAG) {
        av_log(s, AV_LOG_ERROR, "Required format of 'aisf' for demuxing.\n");
        return AVERROR_INVALIDDATA;
    }

    /* Reading Header Info, assuming all goes well :) */
    asif_c->rate = avio_rl32(pb);

    asif_c->channels = avio_rl16(pb);
    
    asif_c->samples = avio_rl32(pb);

    /* In the case of an error, return that we got invalid data */
    if(!asif_c->rate || !asif_c->channels || !asif_c->samples) 
        return AVERROR_INVALIDDATA;

    st = avformat_new_stream(s, NULL);
    if (!st)
        return AVERROR(ENOMEM);
 
    /*Supply relevant information to the Stream. */
    st->codecpar->codec_type     = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id       = AV_CODEC_ID_ASIF;
    st->codecpar->sample_rate    = asif_c->rate;
    st->codecpar->channels       = asif_c->channels;

    return 0;
}

static int asif_read_packet(AVFormatContext *s, AVPacket *pkt)
{

    ASIFDemuxerContext *ctx = s->priv_data;
    AVCodecParameters *par = s->streams[0]->codecpar;
    int ret, size, t; 

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
	t = av_append_packet(s->pb, pkt, size);
	ret += t;
	if(t < 0)
	    return t;
    }

    ///< Increment the number of samples that we've read.
    ctx->current += size;

    pkt->flags &= ~AV_PKT_FLAG_CORRUPT;
    pkt->stream_index = 0;

    return ret;
}

AVInputFormat ff_asif_demuxer = {
    .name           = "asif",
    .long_name      = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 2020)"),
    .priv_data_size = sizeof(ASIFDemuxerContext),
    .read_header    = asif_read_header,
    .read_packet    = asif_read_packet,
    .extensions     = "asif",
};
