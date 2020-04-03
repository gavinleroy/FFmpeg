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
#include "libavutil/opt.h"
#include "libavutil/attributes.h"
#include "libavutil/float_dsp.h"
#include "avcodec.h"
#include "put_bits.h"
#include "bytestream.h"
#include "internal.h"
#include "mathops.h"

///<#include "asif.h"


static int asif_encode_init(AVCodecContext *avctx) {

    if(avctx->codec->id != AV_CODEC_ID_ASIF){
        return AVERROR(EINVAL); ///< the id of the codec is incorrect
    }

    avctx->bits_per_coded_sample = 8;
///<    avctx->block_align           = 0;
///<    avctx->bit_rate              = avctx->block_align * 8LL * avctx->sample_rate;
    avctx->sample_fmt		 = AV_SAMPLE_FMT_U8P;

    return 0;
}

static int asif_encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                             const AVFrame *frame, int *got_packet_ptr)
{
    
    int sample_size, n, i, c, ret; 
    unsigned char *dst;
    const uint8_t *samples;
    
    sample_size = 1; ///< Samples are uint8_t
    n = frame->nb_samples * avctx->channels;

    ret = ff_alloc_packet2(avctx, avpkt, n*sample_size, n*sample_size);
    if (ret < 0)
	return ret;

    dst = avpkt->data;

    /* Note that actual encoding into the delta form will be done 
       in the muxer, this is because how the data is stored before
       actually writing to the file is in a convenient format for 
       quickly taking the deltas without the need of an additional
       data strucutre. */

    n /= avctx->channels; ///< ECODE THE DATA IN A PLANAR FORM
    for (c = 0; c < avctx->channels; c++){
        samples  = frame->extended_data[c]; 
        for (i = 0; i < n; i++)
	    *dst++ = *samples++;
    }

    avpkt->size = n * avctx->channels;
    avpkt->pts = frame->pts;
    avpkt->duration = frame->nb_samples;
    *got_packet_ptr = 1;

    return 0;
}

///<static av_cold int asif_encode_close(AVCodecContext *avctx)
///<{
///<
///<    ///< We have no memory to free
///<
///<    return 0;
///<}

AVCodec ff_asif_encoder = {
    .name                  = "asif",
    .long_name             = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 2020)"),
    .type                  = AVMEDIA_TYPE_AUDIO,
    .id                    = AV_CODEC_ID_ASIF,
    .init                  = asif_encode_init,
    .encode2               = asif_encode_frame,
///<    .close                 = asif_encode_close,
    .capabilities          = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_DR1,
    .sample_fmts           = (const enum AVSampleFormat[]) { AV_SAMPLE_FMT_U8P,
								AV_SAMPLE_FMT_NONE },
};
