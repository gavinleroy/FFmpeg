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

#include "libavutil/attributes.h"
#include "libavutil/float_dsp.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "mathops.h"

static int asif_decode_init(AVCodecContext *avctx)
{

    avctx->sample_fmt = AV_SAMPLE_FMT_U8P;

    return 0;
}

static int asif_decode_frame(AVCodecContext *avctx, void *data,
                             int *got_frame_ptr, AVPacket *avpkt)
{

    uint8_t *src = avpkt->data;
    uint8_t *samples;
    AVFrame *frame = data;
    int sample_size, n, ret;

    sample_size = 1;

    ///<Size of the pkt data in each channel
    n = avpkt->size / avctx->channels;

    ///<Number of samples we're writing per frame.
    frame->nb_samples = n;
    frame->linesize[0] = n;
    frame->format = avctx->sample_fmt;

    if (n < 0) {
        av_log(avctx, AV_LOG_ERROR, "Invalid sample size\n");
        return AVERROR(EINVAL);
    }
    if (avctx->channels <= 0) {
        av_log(avctx, AV_LOG_ERROR, "Invalid number of channels\n");
        return AVERROR(EINVAL);
    }
    if (avctx->codec_id != avctx->codec->id) {
        av_log(avctx, AV_LOG_ERROR, "Codec id mismatch\n");
        return AVERROR(EINVAL);
    }

    ///< Use ffmpeg api to allocate buffer for the frame.
    ret = ff_get_buffer(avctx, frame, 0);
    if (ret < 0)
        return ret;

    ///< Decode data into AVFrame exteneded_data buffer
    for(int i = 0; i < avctx->channels; i++){
        samples = frame->extended_data[i];
	for (int j = 0; j < n; j++)
	    samples[j] = *src++;
    }
///<	    samples[j] = src[i*n + j];
    
    *got_frame_ptr = 1;

    return n * avctx->channels;
}

AVCodec ff_asif_decoder = {
    .name                  = "asif",
    .long_name             = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 2020)"),
    .type                  = AVMEDIA_TYPE_AUDIO,
    .id                    = AV_CODEC_ID_ASIF,
    .init                  = asif_decode_init,
    .decode                = asif_decode_frame,
    .capabilities   	   = AV_CODEC_CAP_DR1,
    .sample_fmts    	   = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_U8P,
                                                            AV_SAMPLE_FMT_NONE }, 
};

/*
    .priv_data_size        = sizeof(ASIFContext),
    .caps_internal         = FF_CODEC_CAP_INIT_THREADSAFE,
    .channel_layouts       = (const uint64_t[]) { AV_CH_LAYOUT_STEREO, 0},
                                                             AV_SAMPLE_FMT_NONE },
*/
