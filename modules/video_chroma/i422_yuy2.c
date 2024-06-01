/*****************************************************************************
 * i422_yuy2.c : Planar YUV 4:2:2 to Packed YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
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
#include <vlc_cpu.h>

#include "i422_yuy2.h"

#define SRC_FOURCC  "I422"
#if defined (PLUGIN_PLAIN)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,Y211"
#else
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( filter_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
#if defined (PLUGIN_PLAIN)
    set_description( N_("Conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_callback_video_converter( Activate, 80 )
# define vlc_CPU_capable() (true)
# define VLC_TARGET
#elif defined (PLUGIN_SSE2)
    set_description( N_("SSE2 conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_callback_video_converter( Activate, 120 )
# define vlc_CPU_capable() vlc_CPU_SSE2()
# define VLC_TARGET VLC_SSE
#endif
vlc_module_end ()


VIDEO_FILTER_WRAPPER( I422_YUY2 )
VIDEO_FILTER_WRAPPER( I422_YVYU )
VIDEO_FILTER_WRAPPER( I422_UYVY )
#if defined (PLUGIN_PLAIN)
VIDEO_FILTER_WRAPPER( I422_Y211 )
#endif


static const struct vlc_filter_operations*
GetFilterOperations(filter_t *filter)
{
    switch( filter->fmt_out.video.i_chroma )
    {
        case VLC_CODEC_YUYV:
            return &I422_YUY2_ops;

        case VLC_CODEC_YVYU:
            return &I422_YVYU_ops;

        case VLC_CODEC_UYVY:
            return &I422_UYVY_ops;

#if defined (PLUGIN_PLAIN)
        case VLC_CODEC_Y211:
            return &I422_Y211_ops;
#endif

        default:
            return NULL;
    }

}

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( filter_t *p_filter )
{
    if( !vlc_CPU_capable() )
        return VLC_EGENERIC;
    if( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) & 1
     || (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) & 1 )
    {
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
    {
        return VLC_EGENERIC;
    }

    /* This is a i422 -> * converter. */
    if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_I422 )
        return VLC_EGENERIC;


    p_filter->ops = GetFilterOperations( p_filter );
    if( p_filter->ops == NULL)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/* Following functions are local */

/*****************************************************************************
 * I422_YUY2: planar YUV 4:2:2 to packed YUY2 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I422_YUY2( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if defined (PLUGIN_SSE2)

    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line|(intptr_t)p_y))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_YUYV_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    else {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_YUYV_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    SSE2_END;

#else

    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
    {
        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x-- ; )
        {
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
        }
        p_y += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line += i_dest_margin;
    }

#endif
}

/*****************************************************************************
 * I422_YVYU: planar YUV 4:2:2 to packed YVYU 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I422_YVYU( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if defined (PLUGIN_SSE2)

    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line|(intptr_t)p_y))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_YVYU_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    else {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_YVYU_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    SSE2_END;

#else

    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
    {
        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x-- ; )
        {
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
        }
        p_y += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line += i_dest_margin;
    }

#endif
}

/*****************************************************************************
 * I422_UYVY: planar YUV 4:2:2 to packed UYVY 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I422_UYVY( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if defined (PLUGIN_SSE2)

    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line|(intptr_t)p_y))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_UYVY_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    else {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
        {
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV422_UYVY_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            }
            p_y += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line += i_dest_margin;
        }
    }
    SSE2_END;

#else

    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
    {
        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x-- ; )
        {
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
        }
        p_y += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line += i_dest_margin;
    }

#endif
}

/*****************************************************************************
 * I422_Y211: planar YUV 4:2:2 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (PLUGIN_PLAIN)
static void I422_Y211( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels + p_dest->p->i_visible_lines * p_dest->p->i_pitch;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) ; i_y-- ; )
    {
        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV422_Y211( p_line, p_y, p_u, p_v );
            C_YUV422_Y211( p_line, p_y, p_u, p_v );
        }
    }
}
#endif
