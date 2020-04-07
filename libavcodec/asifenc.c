/*
 * Audio Processing Technology codec for NON-Audiophiles 
 *
 * Copyright (C) 2020  Gavin Gray <gavinleroy6@gmail.com>
 *
 * asifenc.c:
 * The asif encoder receives audio frames and copies them into packets
 * which are then sent to the asif muxer for writing into a .asif file.
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

/*Checks the Codec ID, sets bits/sample and sample format*/
static int asif_encode_init(AVCodecContext *avctx) {

    if(avctx->codec->id != AV_CODEC_ID_ASIF)
        return AVERROR(EINVAL); ///< the id of the codec is incorrect

    avctx->bits_per_coded_sample = 8;
    avctx->sample_fmt		 = AV_SAMPLE_FMT_U8P;

    return 0;
}

/*Encodes a frame of audio samples taken from the AVFrame into the AVPacket*/
static int asif_encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                             const AVFrame *frame, int *got_packet_ptr)
{
    int sample_size, n, i, c, ret; 
    unsigned char *dst;
    const uint8_t *samples;
    
    sample_size = 1; ///< Samples are uint8_t so size is 1 byte.
    n = frame->nb_samples * avctx->channels;

    ///<Allocate a packet using API call
    ret = ff_alloc_packet2(avctx, avpkt, n*sample_size, n*sample_size);
    if (ret < 0)
	return ret;

    dst = avpkt->data;

    /* Note that actual encoding into the delta form will be done 
       in the muxer, this is because how the data is stored before
       actually writing to the file is in a convenient format for 
       quickly taking the deltas without the need of an additional
       data strucutre. */

    ///<Write the samples in each channel into a corresponding index of the frame's extended_data
    n /= avctx->channels;
    for (c = 0; c < avctx->channels; c++){
        samples  = frame->extended_data[c]; 
        for (i = 0; i < n; i++)
	    *dst++ = *samples++;
    }

    ///<Set some parameters in the packet
    avpkt->size = n * avctx->channels;
    avpkt->pts = frame->pts;
    avpkt->duration = frame->nb_samples;

    *got_packet_ptr = 1;

    return 0;
}

/*Supply FFMPEG with information about our encoder: format, private functions, etc.*/
AVCodec ff_asif_encoder = {
    .name                  = "asif",
    .long_name             = NULL_IF_CONFIG_SMALL("ASIF audio file"),
    .type                  = AVMEDIA_TYPE_AUDIO,
    .id                    = AV_CODEC_ID_ASIF,
    .init                  = asif_encode_init,
    .encode2               = asif_encode_frame,
    .capabilities          = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_DR1,
    .sample_fmts           = (const enum AVSampleFormat[]) { AV_SAMPLE_FMT_U8P,
								AV_SAMPLE_FMT_NONE },
};
