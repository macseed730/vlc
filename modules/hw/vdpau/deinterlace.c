/*****************************************************************************
 * deinterlace.c: VDPAU deinterlacing filter
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "vlc_vdpau.h"

typedef struct
{
    vlc_tick_t last_pts;
} filter_sys_t;

static picture_t *Deinterlace(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_tick_t last_pts = sys->last_pts;

    sys->last_pts = src->date;

    vlc_vdp_video_field_t *f1 = VDPAU_FIELD_FROM_PICCTX(src->context);
    if (unlikely(f1 == NULL))
        return src;
    if (f1->structure != VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME)
        return src; /* cannot deinterlace twice */

#ifdef VOUT_CORE_GETS_A_CLUE
    picture_t *dst = filter_NewPicture(filter);
#else
    picture_t *dst = picture_NewFromFormat(&src->format);
#endif
    if (dst == NULL)
        return src; /* cannot deinterlace without copying fields */

    vlc_vdp_video_field_t *f2 = vlc_vdp_video_copy(f1); // shallow copy
    if (unlikely(f2 == NULL))
    {
        picture_Release(dst);
        return src;
    }

    picture_CopyProperties(dst, src);
    dst->context = &f2->context;

    if (last_pts != VLC_TICK_INVALID)
        dst->date = (3 * src->date - last_pts) / 2;
    else
    if (filter->fmt_in.video.i_frame_rate != 0)
        dst->date = src->date + vlc_tick_from_samples(filter->fmt_in.video.i_frame_rate_base
                            ,filter->fmt_in.video.i_frame_rate);
    dst->b_top_field_first = !src->b_top_field_first;
    dst->i_nb_fields = 1;
    src->i_nb_fields = 1;

    assert(!picture_HasChainedPics(src));
    vlc_picture_chain_AppendChain( src, dst );

    if (src->b_progressive || src->b_top_field_first)
    {
        f1->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
        f2->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }
    else
    {
        f1->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
        f2->structure = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }

    src->b_progressive = true;
    dst->b_progressive = true;
    return src;
}

static void Flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    sys->last_pts = VLC_TICK_INVALID;
}

static void Close(filter_t *filter)
{
    vlc_video_context_Release(filter->vctx_out);
}

static const struct vlc_filter_operations filter_ops = {
    .filter_video = Deinterlace, .close = Close,
    .flush = Flush,
};

static int Open(filter_t *filter)
{
    if ( filter->vctx_in == NULL ||
         vlc_video_context_GetType(filter->vctx_in) != VLC_VIDEO_CONTEXT_VDPAU )
        return VLC_EGENERIC;
    if (filter->fmt_in.video.i_chroma != VLC_CODEC_VDPAU_VIDEO)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    filter_sys_t *sys = vlc_obj_malloc(VLC_OBJECT(filter), sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* NOTE: Only weave and bob are mandatory for the hardware to implement.
     * The other modes and IVTC should be checked. */

    sys->last_pts = VLC_TICK_INVALID;

    filter->ops = &filter_ops;
    filter->p_sys = sys;
    filter->fmt_out.video.i_frame_rate *= 2;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description(N_("VDPAU deinterlacing filter"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_deinterlace_callback(Open)
vlc_module_end()
