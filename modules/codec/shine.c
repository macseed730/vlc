/*****************************************************************************
 * shine_mod.c: MP3 encoder using Shine, a fixed point implementation
 *****************************************************************************
 * Copyright (C) 2008-2009 M2X
 *
 * Authors: Rafaël Carré <rcarre@m2x.nl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>
#include <vlc_aout.h>

#include <assert.h>
#include <inttypes.h>

#include <shine/layer3.h>

typedef struct
{
    shine_t s;
    unsigned int samples_per_frame;
    block_t *first, **lastp;

    unsigned int i_buffer;
    uint8_t *p_buffer;
} encoder_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenEncoder   ( vlc_object_t * );
static void CloseEncoder  ( encoder_t * );

static block_t *EncodeFrame  ( encoder_t *, block_t * );

vlc_module_begin();
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_description( N_("MP3 fixed point audio encoder") );
    set_capability( "audio encoder", 50 );
    set_callback( OpenEncoder );
vlc_module_end();

static atomic_bool busy = ATOMIC_VAR_INIT(false);

static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t*)p_this;
    encoder_sys_t *p_sys;

    /* shine is an 'MP3' encoder */
    if( (p_enc->fmt_out.i_codec != VLC_CODEC_MP3 && p_enc->fmt_out.i_codec != VLC_CODEC_MPGA) ||
        p_enc->fmt_out.audio.i_channels > 2 )
        return VLC_EGENERIC;

    /* Shine is strict on its input */
    if( p_enc->fmt_in.audio.i_channels != 2 )
    {
        msg_Err( p_enc, "Only stereo input is accepted, rejecting %d channels",
            p_enc->fmt_in.audio.i_channels );
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_out.i_bitrate <= 0 )
    {
        msg_Err( p_enc, "unknown bitrate" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_enc, "bitrate %d, samplerate %d, channels %d",
             p_enc->fmt_out.i_bitrate, p_enc->fmt_out.audio.i_rate,
             p_enc->fmt_out.audio.i_channels );

    if( atomic_exchange(&busy, true) )
    {
        msg_Err( p_enc, "encoder already in progress" );
        return VLC_EGENERIC;
    }

    p_enc->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        goto enomem;

    p_sys->first = NULL;
    p_sys->lastp = &p_sys->first;

    shine_config_t cfg = {
        .wave = {
            .channels = p_enc->fmt_out.audio.i_channels,
            .samplerate = p_enc->fmt_out.audio.i_rate,
        },
    };

    shine_set_config_mpeg_defaults(&cfg.mpeg);
    cfg.mpeg.bitr = p_enc->fmt_out.i_bitrate / 1000;

    if (shine_check_config(cfg.wave.samplerate, cfg.mpeg.bitr) == -1) {
        atomic_store(&busy, false);
        msg_Err(p_enc, "Invalid bitrate %d\n", cfg.mpeg.bitr);
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_sys->s = shine_initialise(&cfg);
    p_sys->samples_per_frame = shine_samples_per_pass(p_sys->s);

    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;

    static const struct vlc_encoder_operations ops =
    {
        .close = CloseEncoder,
        .encode_audio = EncodeFrame,
    };
    p_enc->ops = &ops;

    return VLC_SUCCESS;

enomem:
    atomic_store(&busy, false);
    return VLC_ENOMEM;
}

/* We split/pack PCM blocks to a fixed size: p_sys->samples_per_frame * 4 bytes */
static block_t *GetPCM( encoder_t *p_enc, block_t *p_block )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_pcm_block;

    if( !p_block ) goto buffered; /* just return a block if we can */

    /* Put the PCM samples sent by VLC in the Fifo */
    while( p_sys->i_buffer + p_block->i_buffer >= p_sys->samples_per_frame * 4 )
    {
        unsigned int i_buffer = 0;
        p_pcm_block = block_Alloc( p_sys->samples_per_frame * 4 );
        if( !p_pcm_block )
            break;

        if( p_sys->i_buffer )
        {
            memcpy( p_pcm_block->p_buffer, p_sys->p_buffer, p_sys->i_buffer );

            i_buffer = p_sys->i_buffer;
            p_sys->i_buffer = 0;
            free( p_sys->p_buffer );
            p_sys->p_buffer = NULL;
        }

        memcpy( p_pcm_block->p_buffer + i_buffer,
                    p_block->p_buffer, p_sys->samples_per_frame * 4 - i_buffer );
        p_block->p_buffer += p_sys->samples_per_frame * 4 - i_buffer;

        p_block->i_buffer -= p_sys->samples_per_frame * 4 - i_buffer;

        *(p_sys->lastp) = p_pcm_block;
        p_sys->lastp = &p_pcm_block->p_next;
    }

    /* We hadn't enough data to make a block, put it in standby */
    if( p_block->i_buffer )
    {
        uint8_t *p_tmp;

        if( p_sys->i_buffer > 0 )
            p_tmp = realloc( p_sys->p_buffer, p_block->i_buffer + p_sys->i_buffer );
        else
            p_tmp = malloc( p_block->i_buffer );

        if( !p_tmp )
        {
            p_sys->i_buffer = 0;
            free( p_sys->p_buffer );
            p_sys->p_buffer = NULL;
            return NULL;
        }
        p_sys->p_buffer = p_tmp;
        memcpy( p_sys->p_buffer + p_sys->i_buffer,
                    p_block->p_buffer, p_block->i_buffer );

        p_sys->i_buffer += p_block->i_buffer;
        p_block->i_buffer = 0;
    }

buffered:
    /* and finally get a block back */
    p_pcm_block = p_sys->first;

    if( p_pcm_block != NULL ) {
        p_sys->first = p_pcm_block->p_next;
        if( p_pcm_block->p_next == NULL )
            p_sys->lastp = &p_sys->first;
    }

    return p_pcm_block;
}

static block_t *EncodeFrame( encoder_t *p_enc, block_t *p_block )
{
    if (!p_block) /* TODO: flush */
        return NULL;

    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_pcm_block;
    block_t *p_chain = NULL;
    unsigned int i_samples = p_block->i_buffer >> 2 /* s16l stereo */;
    vlc_tick_t start_date = p_block->i_pts;
    start_date -= vlc_tick_from_samples(i_samples, p_enc->fmt_out.audio.i_rate);

    VLC_UNUSED(p_enc);

    do {
        p_pcm_block = GetPCM( p_enc, p_block );
        if( !p_pcm_block )
            break;

        p_block = NULL; /* we don't need it anymore */
        int16_t pcm_planar_buf[SHINE_MAX_SAMPLES * 2];
        int16_t *pcm_planar_buf_chans[2] = {
            &pcm_planar_buf[0],
            &pcm_planar_buf[p_sys->samples_per_frame],
        };
        aout_Deinterleave( pcm_planar_buf, p_pcm_block->p_buffer,
                p_sys->samples_per_frame, p_enc->fmt_in.audio.i_channels, p_enc->fmt_in.i_codec);

        int written;
        unsigned char *buf = shine_encode_buffer(p_sys->s, pcm_planar_buf_chans, &written);
        block_Release( p_pcm_block );

        if (written <= 0)
            break;

        block_t *p_mp3_block = block_Alloc( written );
        if( !p_mp3_block )
            break;

        memcpy( p_mp3_block->p_buffer, buf, written );

        /* date management */
        p_mp3_block->i_length = vlc_tick_from_samples(p_sys->samples_per_frame,
            p_enc->fmt_out.audio.i_rate);

        start_date += p_mp3_block->i_length;
        p_mp3_block->i_dts = p_mp3_block->i_pts = start_date;

        p_mp3_block->i_nb_samples = p_sys->samples_per_frame;

        block_ChainAppend( &p_chain, p_mp3_block );

    } while( p_pcm_block );

    return p_chain;
}

static void CloseEncoder( encoder_t *p_enc )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    /* TODO: we should send the last PCM block padded with 0
     * But we don't know if other blocks will come before it's too late */
    if( p_sys->i_buffer )
        free( p_sys->p_buffer );

    shine_close(p_sys->s);
    atomic_store(&busy, false);

    block_ChainRelease(p_sys->first);
    free( p_sys );
}
