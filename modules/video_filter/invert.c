/*****************************************************************************
 * invert.c : Invert video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( filter_t * );

VIDEO_FILTER_WRAPPER(Filter)

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Invert video filter") )
    set_shortname( N_("Color inversion" ))
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    add_shortcut( "invert" )
    set_callback_video_filter( Create )
vlc_module_end ()

/*****************************************************************************
 * Create: allocates Invert video thread output method
 *****************************************************************************
 * This function allocates and initializes a Invert vout method.
 *****************************************************************************/
static int Create( filter_t *p_filter )
{
    vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;

    if( fourcc == VLC_CODEC_YUVP || fourcc == VLC_CODEC_RGBP
     || fourcc == VLC_CODEC_RGBA || fourcc == VLC_CODEC_ARGB
     || fourcc == VLC_CODEC_BGRA || fourcc == VLC_CODEC_ABGR )
        return VLC_EGENERIC;

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( fourcc );
    if( p_chroma == NULL || p_chroma->plane_count == 0
     || p_chroma->pixel_size * 8 != p_chroma->pixel_bits )
        return VLC_EGENERIC;

    p_filter->ops = &Filter_ops;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Filter( filter_t *p_filter, picture_t *p_pic, picture_t *p_outpic )
{
    VLC_UNUSED(p_filter);
    int i_planes;

    if( p_pic->format.i_chroma == VLC_CODEC_YUVA )
    {
        /* We don't want to invert the alpha plane */
        i_planes = p_pic->i_planes - 1;
        memcpy(
            p_outpic->p[A_PLANE].p_pixels, p_pic->p[A_PLANE].p_pixels,
            p_pic->p[A_PLANE].i_pitch *  p_pic->p[A_PLANE].i_lines );
    }
    else
    {
        i_planes = p_pic->i_planes;
    }

    for( int i_index = 0 ; i_index < i_planes ; i_index++ )
    {
        uint8_t *p_in, *p_in_end, *p_line_end, *p_out;

        p_in = p_pic->p[i_index].p_pixels;
        p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                           * p_pic->p[i_index].i_pitch;

        p_out = p_outpic->p[i_index].p_pixels;

        while( p_in < p_in_end )
        {
            uint64_t *p_in64, *p_out64;

            p_line_end = p_in + p_pic->p[i_index].i_visible_pitch - 64;

            p_in64 = (uint64_t*)p_in;
            p_out64 = (uint64_t*)p_out;

            while( p_in64 < (uint64_t *)p_line_end )
            {
                /* Do 64 pixels at a time */
                *p_out64++ = ~*p_in64++; *p_out64++ = ~*p_in64++;
                *p_out64++ = ~*p_in64++; *p_out64++ = ~*p_in64++;
                *p_out64++ = ~*p_in64++; *p_out64++ = ~*p_in64++;
                *p_out64++ = ~*p_in64++; *p_out64++ = ~*p_in64++;
            }

            p_in = (uint8_t*)p_in64;
            p_out = (uint8_t*)p_out64;
            p_line_end += 64;

            while( p_in < p_line_end )
            {
                *p_out++ = ~( *p_in++ );
            }

            p_in += p_pic->p[i_index].i_pitch
                     - p_pic->p[i_index].i_visible_pitch;
            p_out += p_outpic->p[i_index].i_pitch
                     - p_outpic->p[i_index].i_visible_pitch;
        }
    }
}
