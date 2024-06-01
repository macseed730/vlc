/*****************************************************************************
 * vpx.c: libvpx decoder (VP8/VP9) module
 *****************************************************************************
 * Copyright (C) 2013 Rafaël Carré
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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
#include <vlc_codec.h>

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_image.h>

#ifdef ENABLE_SOUT
# include <vpx/vpx_encoder.h>
# include <vpx/vp8cx.h>
#endif

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
#ifdef ENABLE_SOUT
static const char *const ppsz_sout_options[] = { "quality-mode", NULL };
static int OpenEncoder(vlc_object_t *);
static void CloseEncoder(encoder_t *);
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict);

#define QUALITY_MODE_TEXT N_("Quality mode")
#define QUALITY_MODE_LONGTEXT N_("Quality setting which will determine max encoding time.")

static const int quality_values[] = {
    VPX_DL_GOOD_QUALITY, VPX_DL_REALTIME, VPX_DL_BEST_QUALITY
};
static const char* const quality_desc[] = {
    N_("Good"), N_("Realtime"), N_("Best"),
};
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname("vpx")
    set_description(N_("WebM video decoder"))
    set_capability("video decoder", 60)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_subcategory(SUBCAT_INPUT_VCODEC)
#ifdef ENABLE_SOUT
    add_submodule()
    set_shortname("vpx")
    set_capability("video encoder", 60)
    set_description(N_("WebM video encoder"))
    set_callback(OpenEncoder)
#   define ENC_CFG_PREFIX "sout-vpx-"
    add_integer( ENC_CFG_PREFIX "quality-mode", VPX_DL_BEST_QUALITY, QUALITY_MODE_TEXT,
                 QUALITY_MODE_LONGTEXT )
        change_integer_list( quality_values, quality_desc );
#endif
vlc_module_end ()

static void vpx_err_msg(vlc_object_t *this, struct vpx_codec_ctx *ctx,
                        const char *msg)
{
    const char *error  = vpx_codec_error(ctx);
    const char *detail = vpx_codec_error_detail(ctx);
    if (!detail)
        detail = "no specific information";
    msg_Err(this, msg, error, detail);
}

#define VPX_ERR(this, ctx, msg) vpx_err_msg(VLC_OBJECT(this), ctx, msg ": %s (%s)")

/*****************************************************************************
 * decoder_sys_t: libvpx decoder descriptor
 *****************************************************************************/
typedef struct
{
    struct vpx_codec_ctx ctx;
} decoder_sys_t;

static const struct
{
    vlc_fourcc_t     i_chroma;
    enum vpx_img_fmt i_chroma_id;
    uint8_t          i_bitdepth;
    enum vpx_color_space cs;
} chroma_table[] =
{
    /* Transfer characteristic-dependent mappings must come first */
    { VLC_CODEC_GBR_PLANAR, VPX_IMG_FMT_I444, 8, VPX_CS_SRGB },
    { VLC_CODEC_GBR_PLANAR_10L, VPX_IMG_FMT_I44416, 10, VPX_CS_SRGB },

    { VLC_CODEC_I420, VPX_IMG_FMT_I420, 8, VPX_CS_UNKNOWN },
    { VLC_CODEC_I422, VPX_IMG_FMT_I422, 8, VPX_CS_UNKNOWN },
    { VLC_CODEC_I444, VPX_IMG_FMT_I444, 8, VPX_CS_UNKNOWN },
    { VLC_CODEC_I440, VPX_IMG_FMT_I440, 8, VPX_CS_UNKNOWN },

    { VLC_CODEC_YV12, VPX_IMG_FMT_YV12, 8, VPX_CS_UNKNOWN },

    { VLC_CODEC_I420_10L, VPX_IMG_FMT_I42016, 10, VPX_CS_UNKNOWN },
    { VLC_CODEC_I422_10L, VPX_IMG_FMT_I42216, 10, VPX_CS_UNKNOWN },
    { VLC_CODEC_I444_10L, VPX_IMG_FMT_I44416, 10, VPX_CS_UNKNOWN },

    { VLC_CODEC_I420_12L, VPX_IMG_FMT_I42016, 12, VPX_CS_UNKNOWN },
    { VLC_CODEC_I422_12L, VPX_IMG_FMT_I42216, 12, VPX_CS_UNKNOWN },
    { VLC_CODEC_I444_12L, VPX_IMG_FMT_I44416, 12, VPX_CS_UNKNOWN },

    { VLC_CODEC_I444_16L, VPX_IMG_FMT_I44416, 16, VPX_CS_UNKNOWN },
};

struct video_color
{
    video_color_primaries_t primaries;
    video_transfer_func_t transfer;
    video_color_space_t space;
};

const struct video_color vpx_color_mapping_table[] =
{
    [VPX_CS_UNKNOWN]   =  { COLOR_PRIMARIES_UNDEF,
                            TRANSFER_FUNC_UNDEF,
                            COLOR_SPACE_UNDEF },
    [VPX_CS_BT_601]    =  { COLOR_PRIMARIES_BT601_525,
                            TRANSFER_FUNC_BT709,
                            COLOR_SPACE_BT601 },
    [VPX_CS_BT_709]    =  { COLOR_PRIMARIES_BT709,
                            TRANSFER_FUNC_BT709,
                            COLOR_SPACE_BT709 },
    [VPX_CS_SMPTE_170] =  { COLOR_PRIMARIES_SMTPE_170,
                            TRANSFER_FUNC_BT709,
                            COLOR_SPACE_BT601 },
    [VPX_CS_SMPTE_240] =  { COLOR_PRIMARIES_SMTPE_240,
                            TRANSFER_FUNC_SMPTE_240,
                            COLOR_SPACE_UNDEF },
    [VPX_CS_BT_2020]   =  { COLOR_PRIMARIES_BT2020,
                            TRANSFER_FUNC_BT2020,
                            COLOR_SPACE_BT2020 },
    [VPX_CS_RESERVED]  =  { COLOR_PRIMARIES_UNDEF,
                            TRANSFER_FUNC_UNDEF,
                            COLOR_SPACE_UNDEF },
    [VPX_CS_SRGB]      =  { COLOR_PRIMARIES_SRGB,
                            TRANSFER_FUNC_SRGB,
                            COLOR_SPACE_UNDEF },
};

static vlc_fourcc_t FindVlcChroma( struct vpx_image *img )
{
    for( unsigned int i = 0; i < ARRAY_SIZE(chroma_table); i++ )
        if( chroma_table[i].i_chroma_id == img->fmt &&
            chroma_table[i].i_bitdepth == img->bit_depth &&
            ( chroma_table[i].cs == VPX_CS_UNKNOWN ||
              chroma_table[i].cs == img->cs ) )
            return chroma_table[i].i_chroma;

    return 0;
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************/
static int Decode(decoder_t *dec, block_t *block)
{
    decoder_sys_t *p_sys = dec->p_sys;
    struct vpx_codec_ctx *ctx = &p_sys->ctx;

    if (block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (block->i_flags & (BLOCK_FLAG_CORRUPTED)) {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    /* Associate packet PTS with decoded frame */
    vlc_tick_t *pkt_pts = malloc(sizeof(*pkt_pts));
    if (!pkt_pts) {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    *pkt_pts = (block->i_pts != VLC_TICK_INVALID) ? block->i_pts : block->i_dts;

    vpx_codec_err_t err;
    err = vpx_codec_decode(ctx, block->p_buffer, block->i_buffer, pkt_pts, 0);

    block_Release(block);

    if (err != VPX_CODEC_OK) {
        free(pkt_pts);
        VPX_ERR(dec, ctx, "Failed to decode frame");
        if (err == VPX_CODEC_UNSUP_BITSTREAM)
            return VLCDEC_ECRITICAL;
        else
            return VLCDEC_SUCCESS;
    }

    const void *iter = NULL;
    struct vpx_image *img = vpx_codec_get_frame(ctx, &iter);
    if (!img) {
        free(pkt_pts);
        return VLCDEC_SUCCESS;
    }

    /* fetches back the PTS */
    pkt_pts = img->user_priv;
    vlc_tick_t pts = *pkt_pts;
    free(pkt_pts);

    dec->fmt_out.i_codec = FindVlcChroma(img);

    if( dec->fmt_out.i_codec == 0 ) {
        msg_Err(dec, "Unsupported output colorspace %d", img->fmt);
        return VLCDEC_SUCCESS;
    }

    video_format_t *v = &dec->fmt_out.video;

    if (img->d_w != v->i_visible_width || img->d_h != v->i_visible_height) {
        v->i_visible_width = dec->fmt_out.video.i_width = img->d_w;
        v->i_visible_height = dec->fmt_out.video.i_height = img->d_h;
    }

    if( !dec->fmt_out.video.i_sar_num || !dec->fmt_out.video.i_sar_den )
    {
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    }

    if(dec->fmt_in->video.primaries == COLOR_PRIMARIES_UNDEF &&
       img->cs >= 0 && img->cs < ARRAY_SIZE(vpx_color_mapping_table))
    {
        v->primaries = vpx_color_mapping_table[img->cs].primaries;
        v->transfer = vpx_color_mapping_table[img->cs].transfer;
        v->space = vpx_color_mapping_table[img->cs].space;
        v->color_range = img->range == VPX_CR_FULL_RANGE ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    }

    dec->fmt_out.video.projection_mode = dec->fmt_in->video.projection_mode;
    dec->fmt_out.video.multiview_mode = dec->fmt_in->video.multiview_mode;
    dec->fmt_out.video.pose = dec->fmt_in->video.pose;

    if (decoder_UpdateVideoFormat(dec))
        return VLCDEC_SUCCESS;
    picture_t *pic = decoder_NewPicture(dec);
    if (!pic)
        return VLCDEC_SUCCESS;

    for (int plane = 0; plane < pic->i_planes; plane++ ) {
        plane_t src_plane = pic->p[plane];
        src_plane.p_pixels = img->planes[plane];
        src_plane.i_pitch = img->stride[plane];
        plane_CopyPixels(&pic->p[plane], &src_plane);
    }

    pic->b_progressive = true; /* codec does not support interlacing */
    pic->date = pts;

    decoder_QueueVideo(dec, pic);
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    const struct vpx_codec_iface *iface;
    int vp_version;

    switch (dec->fmt_in->i_codec)
    {
#ifdef ENABLE_VP8_DECODER
    case VLC_CODEC_WEBP:
    case VLC_CODEC_VP8:
        iface = &vpx_codec_vp8_dx_algo;
        vp_version = 8;
        break;
#endif
#ifdef ENABLE_VP9_DECODER
    case VLC_CODEC_VP9:
        iface = &vpx_codec_vp9_dx_algo;
        vp_version = 9;
        break;
#endif
    default:
        return VLC_EGENERIC;
    }

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    dec->p_sys = sys;

    struct vpx_codec_dec_cfg deccfg = {
        .threads = __MIN(vlc_GetCPUCount(), 16)
    };

    msg_Dbg(p_this, "VP%d: using libvpx version %s (build options %s)",
        vp_version, vpx_codec_version_str(), vpx_codec_build_config());

    if (vpx_codec_dec_init(&sys->ctx, iface, &deccfg, 0) != VPX_CODEC_OK) {
        VPX_ERR(p_this, &sys->ctx, "Failed to initialize decoder");
        free(sys);
        return VLC_EGENERIC;;
    }

    dec->pf_decode = Decode;

    dec->fmt_out.video.i_width = dec->fmt_in->video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in->video.i_height;

    if (dec->fmt_in->video.i_sar_num > 0 && dec->fmt_in->video.i_sar_den > 0) {
        dec->fmt_out.video.i_sar_num = dec->fmt_in->video.i_sar_num;
        dec->fmt_out.video.i_sar_den = dec->fmt_in->video.i_sar_den;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *sys = dec->p_sys;

    /* Free our PTS */
    const void *iter = NULL;
    for (;;) {
        struct vpx_image *img = vpx_codec_get_frame(&sys->ctx, &iter);
        if (!img)
            break;
        free(img->user_priv);
    }

    vpx_codec_destroy(&sys->ctx);

    free(sys);
}

#ifdef ENABLE_SOUT

/*****************************************************************************
 * encoder_sys_t: libvpx encoder descriptor
 *****************************************************************************/
typedef struct
{
    struct vpx_codec_ctx ctx;
    unsigned long quality;
} encoder_sys_t;

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    const struct vpx_codec_iface *iface;
    int vp_version;

    switch (p_enc->fmt_out.i_codec)
    {
#ifdef ENABLE_VP8_ENCODER
    case VLC_CODEC_WEBP:
    case VLC_CODEC_VP8:
        iface = &vpx_codec_vp8_cx_algo;
        vp_version = 8;
        break;
#endif
#ifdef ENABLE_VP9_ENCODER
    case VLC_CODEC_VP9:
        iface = &vpx_codec_vp9_cx_algo;
        vp_version = 9;
        break;
#endif
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the encoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    struct vpx_codec_enc_cfg enccfg = {0};
    vpx_codec_enc_config_default(iface, &enccfg, 0);
    enccfg.g_threads = __MIN(vlc_GetCPUCount(), 4);
    enccfg.g_w = p_enc->fmt_in.video.i_visible_width;
    enccfg.g_h = p_enc->fmt_in.video.i_visible_height;

    msg_Dbg(p_this, "VP%d: using libvpx version %s (build options %s)",
        vp_version, vpx_codec_version_str(), vpx_codec_build_config());

    struct vpx_codec_ctx *ctx = &p_sys->ctx;
    if (vpx_codec_enc_init(ctx, iface, &enccfg, 0) != VPX_CODEC_OK) {
        VPX_ERR(p_this, ctx, "Failed to initialize encoder");
        goto error;
    }

    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
    config_ChainParse(p_enc, ENC_CFG_PREFIX, ppsz_sout_options, p_enc->p_cfg);

    /* Deadline (in ms) to spend in encoder */
    const unsigned long quality = var_GetInteger(p_enc, ENC_CFG_PREFIX "quality-mode");
    switch (quality) {
        case VPX_DL_REALTIME:
        case VPX_DL_BEST_QUALITY:
        case VPX_DL_GOOD_QUALITY:
            p_sys->quality = quality;
            break;
        default:
            msg_Warn(p_this, "Unexpected quality %lu, forcing %d", quality, VPX_DL_BEST_QUALITY);
            p_sys->quality = VPX_DL_BEST_QUALITY;
            break;
    }

    static const struct vlc_encoder_operations ops =
    {
        .close = CloseEncoder,
        .encode_video = Encode,
    };
    p_enc->ops = &ops;
    p_enc->p_sys = p_sys;

    return VLC_SUCCESS;
error:
    free(p_sys);
    return VLC_EGENERIC;
}

static const uint32_t webp_simple_lossy_header[5] = {
    VLC_FOURCC('R', 'I', 'F', 'F'),
    0, /* TBD: total size of VP8 data plus 12 bytes for WEBP fourcc + VP8 ChunkHeader */
    VLC_FOURCC('W', 'E', 'B', 'P'),
    VLC_FOURCC('V', 'P', '8', ' '),
    0, /* TBD: total size of VP8 data */
};

static void webp_write_header(uint8_t *p_header, uint32_t i_size, size_t i_header_size)
{
    assert(i_header_size == sizeof(webp_simple_lossy_header));

    memcpy(p_header, webp_simple_lossy_header, i_header_size);
    SetDWLE(p_header + 1*sizeof(uint32_t), i_size + 4 + 8);
    SetDWLE(p_header + 4*sizeof(uint32_t), i_size);
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    struct vpx_codec_ctx *ctx = &p_sys->ctx;

    if (!p_pict) return NULL;

    vpx_image_t img = {0};
    unsigned i_w = p_enc->fmt_in.video.i_visible_width;
    unsigned i_h = p_enc->fmt_in.video.i_visible_height;

    /* Create and initialize the vpx_image (use 1 and correct later to avoid getting
       rejected for non-power of 2 pitch) */
    if (!vpx_img_wrap(&img, VPX_IMG_FMT_I420, i_w, i_h, 1, p_pict->p[0].p_pixels)) {
        VPX_ERR(p_enc, ctx, "Failed to wrap image");
        return NULL;
    }

    /* Fill in real plane/stride values. */
    for (int plane = 0; plane < p_pict->i_planes; plane++) {
        img.planes[plane] = p_pict->p[plane].p_pixels;
        img.stride[plane] = p_pict->p[plane].i_pitch;
    }

    int flags = 0;

    vpx_codec_err_t res = vpx_codec_encode(ctx, &img, p_pict->date, 1,
     flags, p_sys->quality);
    if (res != VPX_CODEC_OK) {
        VPX_ERR(p_enc, ctx, "Failed to encode frame");
        vpx_img_free(&img);
        return NULL;
    }

    const vpx_codec_cx_pkt_t *pkt = NULL;
    vpx_codec_iter_t iter = NULL;
    block_t *p_out = NULL;

    /* WebP container specific context */
    uint32_t i_vp8_data_size = 0;
    uint8_t *p_header = NULL;
    const bool b_is_webp = p_enc->fmt_out.i_codec == VLC_CODEC_WEBP;
    static const size_t i_webp_header_size = sizeof(webp_simple_lossy_header);

    while ((pkt = vpx_codec_get_cx_data(ctx, &iter)) != NULL)
    {
        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            size_t i_block_sz = pkt->data.frame.sz;
            const bool b_needs_padding_byte = b_is_webp && (pkt->data.frame.sz & 1);
            int keyframe = pkt->data.frame.flags & VPX_FRAME_IS_KEY;

            if (b_is_webp && p_header == NULL) {
                i_block_sz += i_webp_header_size;
                i_block_sz += b_needs_padding_byte;
            }

            block_t *p_block = block_Alloc(i_block_sz);
            if (unlikely(p_block == NULL))
            {
                block_ChainRelease(p_out);
                p_out = NULL;
                break;
            }

            uint8_t *p_buffer = p_block->p_buffer;

            /* Leave room at the beginning for the WebP header data. */
            if (b_is_webp && p_header == NULL) {
                p_header = p_buffer;
                p_buffer += i_webp_header_size;
                i_vp8_data_size += pkt->data.frame.sz;
            }

            memcpy(p_buffer, pkt->data.frame.buf, pkt->data.frame.sz);
            p_block->i_dts = p_block->i_pts = pkt->data.frame.pts;
            if (keyframe)
                p_block->i_flags |= BLOCK_FLAG_TYPE_I;

            /* If Chunk Size is odd, a single padding byte -- that MUST be 0 to
               conform with RIFF -- is added. */
            if (b_needs_padding_byte)
                p_block->p_buffer[i_block_sz - 1] = 0;

            block_ChainAppend(&p_out, p_block);
        }
    }

    /* For WebP, now that we have the total size, write the RIFF header. */
    if (b_is_webp && p_header)
        webp_write_header(p_header, i_vp8_data_size, i_webp_header_size);

    vpx_img_free(&img);
    return p_out;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder(encoder_t *p_enc)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    if (vpx_codec_destroy(&p_sys->ctx))
        VPX_ERR(&p_enc->obj, &p_sys->ctx, "Failed to destroy codec");
    free(p_sys);
}

#endif  /* ENABLE_SOUT */
