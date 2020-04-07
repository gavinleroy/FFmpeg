/*
 * Audio Processing Technology codec for NON-Audiophiles 
 *
 * Copyright (C) 2020  Gavin Gray <gavinleroy6@gmail.com>
 *
 * asifdec.c:
 * The asif decoder receives packets of data from the asif demuxer.
 * It reconstructs the complete unsigned 8-bit samples from the deltas
 * and writes them to an AVFrame so that the given asif file can be
 * played or converted via FFMPEG commands.
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


/*Simple struct that keeps track of the previous delta; used for reconstructing
 *samples from the deltas stored in the .asif file.*/
typedef struct{
    uint8_t *prev;
} ASIFDecodeContext;

/*Set the sample format and initialize the prev array in our ASIFDecodeContext*/
static int asif_decode_init(AVCodecContext *avctx)
{   
    avctx->sample_fmt = AV_SAMPLE_FMT_U8P;

    ASIFDecodeContext *ctx = avctx->priv_data;

    ctx->prev = (uint8_t *) malloc(avctx->channels * sizeof(uint8_t));
    
    for(int i = 0; i < avctx->channels; i++)
        ctx->prev[i] = 0;

    return 0;
}

/*Decode the data sent from the demuxer in the AVPacket.  Write it into the AVFrame,
 *rebuilding the samples from the delta values as needed.*/
static int asif_decode_frame(AVCodecContext *avctx, void *data,
                             int *got_frame_ptr, AVPacket *avpkt)
{
    ASIFDecodeContext *ctx = avctx->priv_data;

    uint8_t *src = avpkt->data;
    uint8_t *samples;
    AVFrame *frame = data;
    int sample_size, n, ret;

    sample_size = 1;

    ///<Size of the pkt data in each channel
    n = avpkt->size / avctx->channels / sample_size;

    ///<Number of samples we're writing per frame.
    frame->nb_samples = n;
    frame->linesize[0] = n * avctx->channels;
    frame->format = avctx->sample_fmt;

    ///<Basic error checking
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
    for(int c = 0; c < avctx->channels; c++){
        samples = frame->extended_data[c];
	for (int j = 0; j < n; j++){
	    samples[j] = *src + ctx->prev[c];
	    ctx->prev[c] = samples[j];
	    src++;
	}
    }
    
    *got_frame_ptr = 1;

    ///<Return the size of the data we decoded this frame
    return n * avctx->channels * sample_size;
}

/*Free up the memory used by our ASIFDecodeContext*/
static int asif_decode_close(AVCodecContext *avctx) {
    ASIFDecodeContext *ctx = avctx->priv_data;
    free(ctx->prev);
    return 0;
}

/*Tell FFMPEG relevant information about our decoder and its functions*/
AVCodec ff_asif_decoder = {
    .name                  = "asif",
    .long_name             = NULL_IF_CONFIG_SMALL("ASIF audio file"),
    .type                  = AVMEDIA_TYPE_AUDIO,
    .id                    = AV_CODEC_ID_ASIF,
    .priv_data_size        = sizeof(ASIFDecodeContext),
    .init                  = asif_decode_init,
    .decode                = asif_decode_frame,
    .close                 = asif_decode_close,
    .capabilities   	   = AV_CODEC_CAP_DR1,
    .sample_fmts    	   = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_U8P,
                                                            AV_SAMPLE_FMT_NONE }, 
};

