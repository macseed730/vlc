/*****************************************************************************
 * i420_yuy2.c : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damien@videolan.org>
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

#if defined (PLUGIN_ALTIVEC) && defined(HAVE_ALTIVEC_H)
#   include <altivec.h>
#endif

#include "i420_yuy2.h"

#define SRC_FOURCC  "I420,IYUV,YV12"

#if defined (PLUGIN_PLAIN)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,Y211"
#    define VLC_TARGET
#elif defined (PLUGIN_SSE2)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422"
#    define VLC_TARGET VLC_SSE
#elif defined (PLUGIN_ALTIVEC)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422"
#    define VLC_TARGET VLC_ALTIVEC
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( filter_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
#if defined (PLUGIN_PLAIN)
    set_description( N_("Conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_callback_video_converter( Activate, 80 )
# define vlc_CPU_capable() (true)
#elif defined (PLUGIN_SSE2)
    set_description( N_("SSE2 conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_callback_video_converter( Activate, 250 )
# define vlc_CPU_capable() vlc_CPU_SSE2()
#elif defined (PLUGIN_ALTIVEC)
    set_description( N_("AltiVec conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_callback_video_converter( Activate, 250 )
# define vlc_CPU_capable() vlc_CPU_ALTIVEC()
#endif
vlc_module_end ()

VIDEO_FILTER_WRAPPER( I420_YUY2 )
VIDEO_FILTER_WRAPPER( I420_YVYU )
VIDEO_FILTER_WRAPPER( I420_UYVY )
#if defined (PLUGIN_PLAIN)
VIDEO_FILTER_WRAPPER( I420_Y211 )
#endif

static const struct vlc_filter_operations *
GetFilterOperations( filter_t *p_filter )
{
    switch( p_filter->fmt_out.video.i_chroma )
    {
        case VLC_CODEC_YUYV:
            return &I420_YUY2_ops;

        case VLC_CODEC_YVYU:
            return &I420_YVYU_ops;

        case VLC_CODEC_UYVY:
            return &I420_UYVY_ops;

#if defined (PLUGIN_PLAIN)
        case VLC_CODEC_Y211:
            return &I420_Y211_ops;
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

    if( p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width
       || p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
       || p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
        return VLC_EGENERIC;

    // VLC_CODEC_YV12: FIXME invert U and V in the filters :)
    if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_I420)
        return VLC_EGENERIC;

    /* Find the adequate filter function depending on the output format. */
    p_filter->ops = GetFilterOperations( p_filter );
    if( p_filter->ops == NULL )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

#if 0
static inline unsigned long long read_cycles(void)
{
    unsigned long long v;
    __asm__ __volatile__("rdtsc" : "=A" (v): );

    return v;
}
#endif

/* Following functions are local */

/*****************************************************************************
 * I420_YUY2: planar YUV 4:2:0 to packed YUYV 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I420_YUY2( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line1, *p_line2 = p_dest->p->p_pixels;
    uint8_t *p_y1, *p_y2 = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

#if defined (PLUGIN_ALTIVEC)
#define VEC_NEXT_LINES( ) \
    p_line1  = p_line2; \
    p_line2 += p_dest->p->i_pitch; \
    p_y1     = p_y2; \
    p_y2    += p_source->p[Y_PLANE].i_pitch;

#define VEC_LOAD_UV( ) \
    u_vec = vec_ld( 0, p_u ); p_u += 16; \
    v_vec = vec_ld( 0, p_v ); p_v += 16;

#define VEC_MERGE( a ) \
    uv_vec = a( u_vec, v_vec ); \
    y_vec = vec_ld( 0, p_y1 ); p_y1 += 16; \
    vec_st( vec_mergeh( y_vec, uv_vec ), 0, p_line1 ); p_line1 += 16; \
    vec_st( vec_mergel( y_vec, uv_vec ), 0, p_line1 ); p_line1 += 16; \
    y_vec = vec_ld( 0, p_y2 ); p_y2 += 16; \
    vec_st( vec_mergeh( y_vec, uv_vec ), 0, p_line2 ); p_line2 += 16; \
    vec_st( vec_mergel( y_vec, uv_vec ), 0, p_line2 ); p_line2 += 16;

    vector unsigned char u_vec;
    vector unsigned char v_vec;
    vector unsigned char uv_vec;
    vector unsigned char y_vec;

    if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 32 ) |
           ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 2 ) ) )
    {
        /* Width is a multiple of 32, we take 2 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
#warning FIXME: converting widths % 16 but !widths % 32 is broken on altivec
#if 0
    else if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) |
                ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 4 ) ) )
    {
        /* Width is only a multiple of 16, we take 4 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 4 ; i_y-- ; )
        {
            /* Line 1 and 2, pixels 0 to ( width - 16 ) */
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }

            /* Line 1 and 2, pixels ( width - 16 ) to ( width ) */
            VEC_LOAD_UV( );
            VEC_MERGE( vec_mergeh );

            /* Line 3 and 4, pixels 0 to 16 */
            VEC_NEXT_LINES( );
            VEC_MERGE( vec_mergel );

            /* Line 3 and 4, pixels 16 to ( width ) */
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
#endif
    else
    {
        /* Crap, use the C version */
#undef VEC_NEXT_LINES
#undef VEC_LOAD_UV
#undef VEC_MERGE
#endif

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset / 2 );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if !defined(PLUGIN_SSE2)
    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8; i_x-- ; )
        {
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x-- ; )
        {
            C_YUV420_YUYV( );
        }

        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line2 += i_dest_margin;
    }

#if defined (PLUGIN_ALTIVEC)
    }
#endif

#elif defined(PLUGIN_SSE2)
    /*
    ** SSE2 128 bits fetch/store instructions are faster
    ** if memory access is 16 bytes aligned
    */

    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line2|(intptr_t)p_y2))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_YUYV_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    else
    {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_YUYV_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    /* make sure all SSE2 stores are visible thereafter */
    SSE2_END;

#endif // defined(PLUGIN_SSE2)
}

/*****************************************************************************
 * I420_YVYU: planar YUV 4:2:0 to packed YVYU 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I420_YVYU( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line1, *p_line2 = p_dest->p->p_pixels;
    uint8_t *p_y1, *p_y2 = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

#if defined (PLUGIN_ALTIVEC)
#define VEC_NEXT_LINES( ) \
    p_line1  = p_line2; \
    p_line2 += p_dest->p->i_pitch; \
    p_y1     = p_y2; \
    p_y2    += p_source->p[Y_PLANE].i_pitch;

#define VEC_LOAD_UV( ) \
    u_vec = vec_ld( 0, p_u ); p_u += 16; \
    v_vec = vec_ld( 0, p_v ); p_v += 16;

#define VEC_MERGE( a ) \
    vu_vec = a( v_vec, u_vec ); \
    y_vec = vec_ld( 0, p_y1 ); p_y1 += 16; \
    vec_st( vec_mergeh( y_vec, vu_vec ), 0, p_line1 ); p_line1 += 16; \
    vec_st( vec_mergel( y_vec, vu_vec ), 0, p_line1 ); p_line1 += 16; \
    y_vec = vec_ld( 0, p_y2 ); p_y2 += 16; \
    vec_st( vec_mergeh( y_vec, vu_vec ), 0, p_line2 ); p_line2 += 16; \
    vec_st( vec_mergel( y_vec, vu_vec ), 0, p_line2 ); p_line2 += 16;

    vector unsigned char u_vec;
    vector unsigned char v_vec;
    vector unsigned char vu_vec;
    vector unsigned char y_vec;

    if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 32 ) |
           ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 2 ) ) )
    {
        /* Width is a multiple of 32, we take 2 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) |
                ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 4 ) ) )
    {
        /* Width is only a multiple of 16, we take 4 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 4 ; i_y-- ; )
        {
            /* Line 1 and 2, pixels 0 to ( width - 16 ) */
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }

            /* Line 1 and 2, pixels ( width - 16 ) to ( width ) */
            VEC_LOAD_UV( );
            VEC_MERGE( vec_mergeh );

            /* Line 3 and 4, pixels 0 to 16 */
            VEC_NEXT_LINES( );
            VEC_MERGE( vec_mergel );

            /* Line 3 and 4, pixels 16 to ( width ) */
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else
    {
        /* Crap, use the C version */
#undef VEC_NEXT_LINES
#undef VEC_LOAD_UV
#undef VEC_MERGE
#endif

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset / 2 );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if !defined(PLUGIN_SSE2)
    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x-- ; )
        {
            C_YUV420_YVYU( );
        }

        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line2 += i_dest_margin;
    }

#if defined (PLUGIN_ALTIVEC)
    }
#endif

#elif defined(PLUGIN_SSE2)
    /*
    ** SSE2 128 bits fetch/store instructions are faster
    ** if memory access is 16 bytes aligned
    */
    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line2|(intptr_t)p_y2))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_YVYU_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_YVYU( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    else
    {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_YVYU_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_YVYU( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    /* make sure all SSE2 stores are visible thereafter */
    SSE2_END;
#endif // defined(PLUGIN_SSE2)
}

/*****************************************************************************
 * I420_UYVY: planar YUV 4:2:0 to packed UYVY 4:2:2
 *****************************************************************************/
VLC_TARGET
static void I420_UYVY( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line1, *p_line2 = p_dest->p->p_pixels;
    uint8_t *p_y1, *p_y2 = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

#if defined (PLUGIN_ALTIVEC)
#define VEC_NEXT_LINES( ) \
    p_line1  = p_line2; \
    p_line2 += p_dest->p->i_pitch; \
    p_y1     = p_y2; \
    p_y2    += p_source->p[Y_PLANE].i_pitch;

#define VEC_LOAD_UV( ) \
    u_vec = vec_ld( 0, p_u ); p_u += 16; \
    v_vec = vec_ld( 0, p_v ); p_v += 16;

#define VEC_MERGE( a ) \
    uv_vec = a( u_vec, v_vec ); \
    y_vec = vec_ld( 0, p_y1 ); p_y1 += 16; \
    vec_st( vec_mergeh( uv_vec, y_vec ), 0, p_line1 ); p_line1 += 16; \
    vec_st( vec_mergel( uv_vec, y_vec ), 0, p_line1 ); p_line1 += 16; \
    y_vec = vec_ld( 0, p_y2 ); p_y2 += 16; \
    vec_st( vec_mergeh( uv_vec, y_vec ), 0, p_line2 ); p_line2 += 16; \
    vec_st( vec_mergel( uv_vec, y_vec ), 0, p_line2 ); p_line2 += 16;

    vector unsigned char u_vec;
    vector unsigned char v_vec;
    vector unsigned char uv_vec;
    vector unsigned char y_vec;

    if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 32 ) |
           ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 2 ) ) )
    {
        /* Width is a multiple of 32, we take 2 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else if( !( ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) |
                ( (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) % 4 ) ) )
    {
        /* Width is only a multiple of 16, we take 4 lines at a time */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 4 ; i_y-- ; )
        {
            /* Line 1 and 2, pixels 0 to ( width - 16 ) */
            VEC_NEXT_LINES( );
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }

            /* Line 1 and 2, pixels ( width - 16 ) to ( width ) */
            VEC_LOAD_UV( );
            VEC_MERGE( vec_mergeh );

            /* Line 3 and 4, pixels 0 to 16 */
            VEC_NEXT_LINES( );
            VEC_MERGE( vec_mergel );

            /* Line 3 and 4, pixels 16 to ( width ) */
            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 32 ; i_x-- ; )
            {
                VEC_LOAD_UV( );
                VEC_MERGE( vec_mergeh );
                VEC_MERGE( vec_mergel );
            }
        }
    }
    else
    {
        /* Crap, use the C version */
#undef VEC_NEXT_LINES
#undef VEC_LOAD_UV
#undef VEC_MERGE
#endif

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset / 2 );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

#if !defined(PLUGIN_SSE2)
    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
        }
        for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 8 ) / 2; i_x--; )
        {
            C_YUV420_UYVY( );
        }

        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line2 += i_dest_margin;
    }

#if defined (PLUGIN_ALTIVEC)
    }
#endif

#elif defined(PLUGIN_SSE2)
    /*
    ** SSE2 128 bits fetch/store instructions are faster
    ** if memory access is 16 bytes aligned
    */
    if( 0 == (15 & (p_source->p[Y_PLANE].i_pitch|p_dest->p->i_pitch|
        ((intptr_t)p_line2|(intptr_t)p_y2))) )
    {
        /* use faster SSE2 aligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_UYVY_ALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_UYVY( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    else
    {
        /* use slower SSE2 unaligned fetch and store */
        for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += p_dest->p->i_pitch;

            p_y1 = p_y2;
            p_y2 += p_source->p[Y_PLANE].i_pitch;

            for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 16 ; i_x-- ; )
            {
                SSE2_CALL( SSE2_YUV420_UYVY_UNALIGNED );
            }
            for( i_x = ( (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) % 16 ) / 2; i_x-- ; )
            {
                C_YUV420_UYVY( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_margin_c;
            p_v += i_source_margin_c;
            p_line2 += i_dest_margin;
        }
    }
    /* make sure all SSE2 stores are visible thereafter */
    SSE2_END;
#endif // defined(PLUGIN_SSE2)
}

/*****************************************************************************
 * I420_Y211: planar YUV 4:2:0 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (PLUGIN_PLAIN)
static void I420_Y211( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint8_t *p_line1, *p_line2 = p_dest->p->p_pixels;
    uint8_t *p_y1, *p_y2 = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p[0].i_pitch
                                 - p_source->p[0].i_visible_pitch
                                 - p_filter->fmt_in.video.i_x_offset;
    const int i_source_margin_c = p_source->p[1].i_pitch
                                 - p_source->p[1].i_visible_pitch
                                 - ( p_filter->fmt_in.video.i_x_offset / 2 );
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch
                               - ( p_filter->fmt_out.video.i_x_offset * 2 );

    for( i_y = (p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height) / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = (p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width) / 8 ; i_x-- ; )
        {
            C_YUV420_Y211( );
            C_YUV420_Y211( );
        }

        p_y2 += i_source_margin;
        p_u += i_source_margin_c;
        p_v += i_source_margin_c;
        p_line2 += i_dest_margin;
    }
}
#endif
