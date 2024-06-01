/*****************************************************************************
 * gstdecode.c: Decoder module making use of gstreamer
 *****************************************************************************
 * Copyright (C) 2014-2016 VLC authors and VideoLAN
 *
 * Author: Vikram Fugro <vikram.fugro@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>

#include <gst/app/gstappsrc.h>
#include <gst/gstatomicqueue.h>

#include "gstvlcpictureplaneallocator.h"
#include "gstvlcvideosink.h"
#include "gstcopypicture.h"
#include "gst_mem.h"

typedef struct
{
    GstElement *p_decoder;
    GstElement *p_decode_src;
    GstElement *p_decode_in;
    GstElement *p_decode_out;

    GstVlcPicturePlaneAllocator *p_allocator;

    GstBus *p_bus;

    GstVideoInfo vinfo;
    GstAtomicQueue *p_que;
    bool b_prerolled;
    bool b_running;

    vlc_video_context *vctx;
} decoder_sys_t;

typedef struct
{
    GstCaps *p_sinkcaps;
    GstCaps *p_srccaps;
} sink_src_caps_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t* );
static void CloseDecoder( vlc_object_t* );
static int  DecodeBlock( decoder_t*, block_t* );
static void Flush( decoder_t * );
static int OpenDecoderDevice( vlc_decoder_device*, vlc_window_t* );

#define MODULE_DESCRIPTION N_( "Uses GStreamer framework's plugins " \
        "to decode the media codecs" )

#define USEDECODEBIN_TEXT N_( "Use DecodeBin" )
#define USEDECODEBIN_LONGTEXT N_( \
    "DecodeBin is a container element, that can add and " \
    "manage multiple elements. Apart from adding the decoders, " \
    "decodebin also adds elementary stream parsers which can provide " \
    "more info such as codec profile, level and other attributes, " \
    "in the form of GstCaps (Stream Capabilities) to decoder." )

#define USEVLCPOOL_TEXT "Use VLCPool"
#define USEVLCPOOL_LONGTEXT \
    "Allow the gstreamer decoders to directly decode (direct render) " \
    "into the buffers provided and managed by the (downstream)VLC modules " \
    "that follow. Note: Currently this feature is unstable, enable it at " \
    "your own risk."

vlc_module_begin( )
    set_shortname( "GstDecode" )
    add_shortcut( "gstdecode" )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    /* decoder main module */
    set_description( N_( "GStreamer Based Decoder" ) )
    set_help( MODULE_DESCRIPTION )
    set_capability( "video decoder", 50 )
    set_section( N_( "Decoding" ) , NULL )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_bool( "use-decodebin", true, USEDECODEBIN_TEXT,
        USEDECODEBIN_LONGTEXT )
    add_bool( "use-vlcpool", false, USEVLCPOOL_TEXT,
        USEVLCPOOL_LONGTEXT )
    add_submodule( )
        set_callback_dec_device( OpenDecoderDevice, 100 )
        add_shortcut( "gstdecode" )
vlc_module_end( )

static void gst_mem_pic_context_Destroy( struct picture_context_t *ctx )
{
    struct gst_mem_pic_context *gst_mem_ctx = container_of( ctx,
            struct gst_mem_pic_context, s );

    gst_buffer_unref( gst_mem_ctx->p_buf );
    free( gst_mem_ctx );
}

static picture_context_t *gst_mem_pic_context_Copy(
        struct picture_context_t *ctx )
{
    struct gst_mem_pic_context *gst_mem_ctx = container_of( ctx,
            struct gst_mem_pic_context, s );
    struct gst_mem_pic_context *gst_mem_ctx_copy = calloc( 1,
            sizeof( *gst_mem_ctx_copy ) );
    if( unlikely( gst_mem_ctx_copy == NULL ) )
        return NULL;

    *gst_mem_ctx_copy = *gst_mem_ctx;
    vlc_video_context_Hold( gst_mem_ctx_copy->s.vctx );
    gst_buffer_ref( gst_mem_ctx_copy->p_buf );

    return &gst_mem_ctx_copy->s;
}

void gst_vlc_dec_ensure_empty_queue( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_count = 0;

    msg_Dbg( p_dec, "Ensuring the decoder queue is empty");

    /* Busy wait with sleep; As this is rare case and the
     * wait might at max go for 3-4 iterations, preferred to not
     * to throw in a cond/lock here. */
    while( p_sys->b_running && i_count < 60 &&
            gst_atomic_queue_length( p_sys->p_que ))
    {
        vlc_tick_sleep ( VLC_TICK_FROM_MS(15) );
        i_count++;
    }

    if( p_sys->b_running )
    {
        if( !gst_atomic_queue_length( p_sys->p_que ))
            msg_Dbg( p_dec, "Ensured the decoder queue is empty" );
        else
            msg_Warn( p_dec, "Timed out when ensuring an empty queue" );
    }
    else
        msg_Dbg( p_dec, "Ensuring empty decoder queue not required; decoder \
                not running" );
}

/* Emitted by appsrc when serving a seek request.
 * Seek over here is only used for flushing the buffers.
 * Returns TRUE always, as the 'real' seek will be
 * done by VLC framework */
static gboolean seek_data_cb( GstAppSrc *p_src, guint64 l_offset,
        gpointer p_data )
{
    VLC_UNUSED( p_src );
    decoder_t *p_dec = p_data;
    msg_Dbg( p_dec, "appsrc seeking to %"G_GUINT64_FORMAT, l_offset );
    return TRUE;
}

/* Emitted by decodebin when an autoplugged element not yet
 * downstream-linked does a query.
 * Used here for format and allocator negotiation. */
static gboolean autoplug_query_cb( GstElement *p_bin, GstPad *p_pad,
                                   GstElement *p_element, GstQuery *p_query,
                                   gpointer p_data )
{
    VLC_UNUSED( p_bin );
    decoder_t *p_dec = p_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( ( p_pad->direction == GST_PAD_SRC ) &&
        GST_IS_VIDEO_DECODER( p_element ) )
    {
        switch( GST_QUERY_TYPE ( p_query ) ){
        case GST_QUERY_CAPS:
            return gst_vlc_video_sink_query_caps( p_query );
        case GST_QUERY_ALLOCATION:
            GstBaseSink *p_bsink = GST_BASE_SINK_CAST( p_sys->p_decode_out );
            GstBaseSinkClass *p_bclass = GST_BASE_SINK_GET_CLASS( p_bsink );
            return p_bclass->propose_allocation( p_bsink, p_query );
        default:
            return FALSE;
        }
    }

    return FALSE;
}

/* Emitted by decodebin and links decodebin to vlcvideosink.
 * Since only one elementary codec stream is fed to decodebin,
 * this signal cannot be emitted more than once. */
static void pad_added_cb( GstElement *p_ele, GstPad *p_pad, gpointer p_data )
{
    VLC_UNUSED( p_ele );
    decoder_t *p_dec = p_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( likely( gst_pad_has_current_caps( p_pad ) ) )
    {
        GstPadLinkReturn ret;
        GstPad *p_sinkpad;

        msg_Dbg( p_dec, "linking the decoder with the vsink");

        p_sinkpad = gst_element_get_static_pad(
                p_sys->p_decode_out, "sink" );
        ret = gst_pad_link( p_pad, p_sinkpad );
        if( ret != GST_PAD_LINK_OK )
            msg_Err( p_dec, "failed to link decoder with vsink");

        gst_object_unref( p_sinkpad );
    }
    else
    {
        msg_Err( p_dec, "decodebin src pad has no caps" );
        GST_ELEMENT_ERROR( p_sys->p_decoder, STREAM, FAILED,
                ( "vlc stream error" ), NULL );
    }
}

static gboolean caps_handoff_cb( GstElement* p_ele, GstCaps *p_caps,
        gpointer p_data )
{
    VLC_UNUSED( p_ele );
    decoder_t *p_dec = p_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    GstVideoAlignment align;

    msg_Info( p_dec, "got new caps %s", gst_caps_to_string( p_caps ));

    if( !gst_video_info_from_caps( &p_sys->vinfo, p_caps ))
    {
        msg_Err( p_dec, "failed to negotiate" );
        return FALSE;
    }

    gst_vlc_dec_ensure_empty_queue( p_dec );
    gst_video_alignment_reset( &align );

    return gst_vlc_set_vout_fmt( &p_sys->vinfo, &align, p_caps, p_dec );
}

/* Emitted by vlcvideosink for every buffer,
 * Adds the buffer to the queue */
static void frame_handoff_cb( GstElement *p_ele, GstBuffer *p_buf,
        gpointer p_data )
{
    VLC_UNUSED( p_ele );
    decoder_t *p_dec = p_data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Push the buffer to the queue */
    gst_atomic_queue_push( p_sys->p_que, gst_buffer_ref( p_buf ) );
}

/* Check if the element can use this caps */
static gint find_decoder_func( gconstpointer p_p1, gconstpointer p_p2 )
{
    GstElementFactory *p_factory;
    sink_src_caps_t *p_caps;

    p_factory = ( GstElementFactory* )p_p1;
    p_caps = ( sink_src_caps_t* )p_p2;

    return !( gst_element_factory_can_sink_any_caps( p_factory,
                p_caps->p_sinkcaps ) &&
            gst_element_factory_can_src_any_caps( p_factory,
                p_caps->p_srccaps ));
}

static bool default_msg_handler( decoder_t *p_dec, GstMessage *p_msg )
{
    bool err = false;

    switch( GST_MESSAGE_TYPE( p_msg ) ){
    case GST_MESSAGE_ERROR:
        {
            gchar  *psz_debug;
            GError *p_error;

            gst_message_parse_error( p_msg, &p_error, &psz_debug );
            g_free( psz_debug );

            msg_Err( p_dec, "Error from %s: %s",
                    GST_ELEMENT_NAME( GST_MESSAGE_SRC( p_msg ) ),
                    p_error->message );
            g_error_free( p_error );
            err = true;
        }
        break;
    case GST_MESSAGE_WARNING:
        {
            gchar  *psz_debug;
            GError *p_error;

            gst_message_parse_warning( p_msg, &p_error, &psz_debug );
            g_free( psz_debug );

            msg_Warn( p_dec, "Warning from %s: %s",
                    GST_ELEMENT_NAME( GST_MESSAGE_SRC( p_msg ) ),
                    p_error->message );
            g_error_free( p_error );
        }
        break;
    case GST_MESSAGE_INFO:
        {
            gchar  *psz_debug;
            GError *p_error;

            gst_message_parse_info( p_msg, &p_error, &psz_debug );
            g_free( psz_debug );

            msg_Info( p_dec, "Info from %s: %s",
                    GST_ELEMENT_NAME( GST_MESSAGE_SRC( p_msg ) ),
                    p_error->message );
            g_error_free( p_error );
        }
        break;
    default:
        break;
    }

    return err;
}

static gboolean vlc_gst_plugin_init( GstPlugin *p_plugin )
{
    if( !gst_element_register( p_plugin, "vlcvideosink", GST_RANK_NONE,
                GST_TYPE_VLC_VIDEO_SINK ))
        return FALSE;

    return TRUE;
}


static void vlc_gst_init_once(void *data)
{
    bool *registered = data;

    gst_init( NULL, NULL );
    *registered = gst_plugin_register_static( 1, 0, "videolan",
                "VLC Gstreamer plugins", vlc_gst_plugin_init,
                "1.0.0", "LGPL", "NA", "vlc", "NA" );
}

/* gst_init( ) is not thread-safe, hence a thread-safe wrapper */
static bool vlc_gst_init( void )
{
    static vlc_once_t once = VLC_STATIC_ONCE;
    static bool vlc_gst_registered;

    vlc_once(&once, vlc_gst_init_once, &vlc_gst_registered);
    return vlc_gst_registered;
}

static GstStructure* vlc_to_gst_fmt( const es_format_t *p_fmt )
{
    const video_format_t *p_vfmt = &p_fmt->video;
    GstStructure *p_str = NULL;

    switch( p_fmt->i_codec ){
    case VLC_CODEC_H264:
        p_str = gst_structure_new_empty( "video/x-h264" );
        gst_structure_set( p_str, "alignment", G_TYPE_STRING, "au", NULL );
        if( p_fmt->i_extra )
            gst_structure_set( p_str, "stream-format", G_TYPE_STRING, "avc",
                    NULL );
        else
            gst_structure_set( p_str, "stream-format", G_TYPE_STRING,
                    "byte-stream", NULL );
        break;
    case VLC_CODEC_HEVC:
        p_str = gst_structure_new_empty( "video/x-h265" );
        gst_structure_set( p_str, "alignment", G_TYPE_STRING, "au",
                "stream-format", G_TYPE_STRING, "hvc1", NULL );
        break;
    case VLC_CODEC_MP4V:
        p_str = gst_structure_new_empty( "video/mpeg" );
        gst_structure_set( p_str, "mpegversion", G_TYPE_INT, 4,
                "systemstream", G_TYPE_BOOLEAN, FALSE, NULL );
        break;
    case VLC_CODEC_VP8:
        p_str = gst_structure_new_empty( "video/x-vp8" );
        break;
    case VLC_CODEC_VP9:
        p_str = gst_structure_new_empty( "video/x-vp9" );
        break;
    case VLC_CODEC_AV1:
        p_str = gst_structure_new_empty( "video/x-av1" );
        break;
    case VLC_CODEC_MPGV:
        p_str = gst_structure_new_empty( "video/mpeg" );
        gst_structure_set( p_str, "mpegversion", G_TYPE_INT, 2,
                "systemstream", G_TYPE_BOOLEAN, FALSE, NULL );
        break;
    case VLC_CODEC_FLV1:
        p_str = gst_structure_new_empty( "video/x-flash-video" );
        gst_structure_set( p_str, "flvversion", G_TYPE_INT, 1, NULL );
        break;
    case VLC_CODEC_WMV1:
        p_str = gst_structure_new_empty( "video/x-wmv" );
        gst_structure_set( p_str, "wmvversion", G_TYPE_INT, 1,
                "format", G_TYPE_STRING, "WMV1", NULL );
        break;
    case VLC_CODEC_WMV2:
        p_str = gst_structure_new_empty( "video/x-wmv" );
        gst_structure_set( p_str, "wmvversion", G_TYPE_INT, 2,
                "format", G_TYPE_STRING, "WMV2", NULL );
        break;
    case VLC_CODEC_WMV3:
        p_str = gst_structure_new_empty( "video/x-wmv" );
        gst_structure_set( p_str, "wmvversion", G_TYPE_INT, 3,
                "format", G_TYPE_STRING, "WMV3", NULL );
        break;
    case VLC_CODEC_VC1:
        p_str = gst_structure_new_empty( "video/x-wmv" );
        gst_structure_set( p_str, "wmvversion", G_TYPE_INT, 3,
                "format", G_TYPE_STRING, "WVC1", NULL );
        break;
    default:
        /* unsupported codec */
        return NULL;
    }

    if( p_vfmt->i_width && p_vfmt->i_height )
        gst_structure_set( p_str,
                "width", G_TYPE_INT, p_vfmt->i_width,
                "height", G_TYPE_INT, p_vfmt->i_height, NULL );

    if( p_vfmt->i_frame_rate && p_vfmt->i_frame_rate_base )
        gst_structure_set( p_str, "framerate", GST_TYPE_FRACTION,
                p_vfmt->i_frame_rate,
                p_vfmt->i_frame_rate_base, NULL );

    if( p_vfmt->i_sar_num && p_vfmt->i_sar_den )
        gst_structure_set( p_str, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                p_vfmt->i_sar_num,
                p_vfmt->i_sar_den, NULL );

    if( p_fmt->i_extra )
    {
        GstBuffer *p_buf;

        p_buf = gst_buffer_new_wrapped_full( GST_MEMORY_FLAG_READONLY,
                p_fmt->p_extra, p_fmt->i_extra, 0,
                p_fmt->i_extra, NULL, NULL );
        if( p_buf == NULL )
        {
            gst_structure_free( p_str );
            return NULL;
        }

        gst_structure_set( p_str, "codec_data", GST_TYPE_BUFFER, p_buf, NULL );
        gst_buffer_unref( p_buf );
    }

    return p_str;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = ( decoder_t* )p_this;
    decoder_sys_t *p_sys;
    GstStateChangeReturn i_ret;
    gboolean b_ret;
    sink_src_caps_t caps = { NULL, NULL };
    GstStructure *p_str;
    GstAppSrcCallbacks cb;
    int i_rval = VLC_SUCCESS;
    GList *p_list;
    bool dbin, vlc_pool;

#define VLC_GST_CHECK( r, v, s, t ) \
    { if( r == v ){ msg_Err( p_dec, s ); i_rval = t; goto fail; } }

    if( !vlc_gst_init( ))
    {
        msg_Err( p_dec, "failed to register vlcvideosink" );
        return VLC_EGENERIC;
    }

    p_str = vlc_to_gst_fmt( p_dec->fmt_in );
    if( !p_str )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = p_dec->p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
    {
        gst_structure_free( p_str );
        return VLC_ENOMEM;
    }

    dbin = var_CreateGetBool( p_dec, "use-decodebin" );
    msg_Dbg( p_dec, "Using decodebin? %s", dbin ? "yes ":"no" );

    caps.p_sinkcaps = gst_caps_new_empty( );
    gst_caps_append_structure( caps.p_sinkcaps, p_str );
    /* Currently supports only system memory raw output format */
    caps.p_srccaps = gst_caps_new_empty_simple( "video/x-raw" );

    /* Get the list of all the available gstreamer decoders */
    p_list = gst_element_factory_list_get_elements(
            GST_ELEMENT_FACTORY_TYPE_DECODER, GST_RANK_MARGINAL );
    if( !dbin )
    {
        GList *p_l;
        /* Sort them as per ranks */
        p_list = g_list_sort( p_list, gst_plugin_feature_rank_compare_func );
        p_l = g_list_find_custom( p_list, &caps, find_decoder_func );
        VLC_GST_CHECK( p_l, NULL, "no suitable decoder found", VLC_ENOTSUP );
        /* create the decoder with highest rank */
        p_sys->p_decode_in = gst_element_factory_create(
                ( GstElementFactory* )p_l->data, NULL );
        VLC_GST_CHECK( p_sys->p_decode_in, NULL,
                "failed to create decoder", VLC_ENOMEM );
    }
    else
    {
        GList *p_l;
        /* Just check if any suitable decoder exists, rest will be
         * handled by decodebin */
        p_l = g_list_find_custom( p_list, &caps, find_decoder_func );
        VLC_GST_CHECK( p_l, NULL, "no suitable decoder found", VLC_ENOTSUP );
    }
    gst_plugin_feature_list_free( p_list );
    p_list = NULL;
    gst_caps_unref( caps.p_srccaps );
    caps.p_srccaps = NULL;

    p_sys->b_prerolled = false;
    p_sys->b_running = false;

    /* Queue: GStreamer thread will dump buffers into this queue,
     * DecodeBlock() will pop out the buffers from the queue */
    p_sys->p_que = gst_atomic_queue_new( 0 );

    p_sys->p_decode_src = gst_element_factory_make( "appsrc", NULL );
    VLC_GST_CHECK( p_sys->p_decode_src, NULL, "appsrc not found", VLC_ENOMEM );
    g_object_set( G_OBJECT( p_sys->p_decode_src ), "caps", caps.p_sinkcaps,
            "emit-signals", TRUE, "format", GST_FORMAT_BYTES,
            "stream-type", GST_APP_STREAM_TYPE_SEEKABLE,
            /* Making DecodeBlock() to block on appsrc with max queue size of 1 byte.
             * This will make the push_buffer() tightly coupled with the buffer
             * flow from appsrc -> decoder. push_buffer() will only return when
             * the same buffer it just fed to appsrc has also been fed to the
             * decoder element as well */
            "block", TRUE, "max-bytes", ( guint64 )1, NULL );
    gst_caps_unref( caps.p_sinkcaps );
    caps.p_sinkcaps = NULL;
    cb.enough_data = NULL;
    cb.need_data = NULL;
    cb.seek_data = seek_data_cb;
    gst_app_src_set_callbacks( GST_APP_SRC( p_sys->p_decode_src ),
            &cb, p_dec, NULL );

    if( dbin )
    {
        p_sys->p_decode_in = gst_element_factory_make( "decodebin", NULL );
        VLC_GST_CHECK( p_sys->p_decode_in, NULL, "decodebin not found",
                       VLC_ENOMEM );
        //g_object_set( G_OBJECT( p_sys->p_decode_in ),
        //"max-size-buffers", 2, NULL );
        //g_signal_connect( G_OBJECT( p_sys->p_decode_in ), "no-more-pads",
                //G_CALLBACK( no_more_pads_cb ), p_dec );
        g_signal_connect( G_OBJECT( p_sys->p_decode_in ), "pad-added",
                G_CALLBACK( pad_added_cb ), p_dec );

        g_signal_connect( G_OBJECT( p_sys->p_decode_in ), "autoplug-query",
                G_CALLBACK( autoplug_query_cb ), p_dec );
    }

    /* videosink: will emit signal for every available buffer */
    p_sys->p_decode_out = gst_element_factory_make( "vlcvideosink", NULL );
    VLC_GST_CHECK( p_sys->p_decode_out, NULL, "vlcvideosink not found",
                   VLC_ENOMEM );

    vlc_pool = var_CreateGetBool( p_dec, "use-vlcpool" );
    msg_Dbg( p_dec, "Using vlc pool? %s", vlc_pool ? "yes ":"no" );

    p_sys->p_allocator = gst_vlc_picture_plane_allocator_new(
            (gpointer) p_dec );
    g_object_set( G_OBJECT( p_sys->p_decode_out ), "sync", FALSE, "allocator",
            p_sys->p_allocator, "id", (gpointer) p_dec, "use-pool", vlc_pool, NULL );
    g_signal_connect( G_OBJECT( p_sys->p_decode_out ), "new-buffer",
            G_CALLBACK( frame_handoff_cb ), p_dec );

    //FIXME: caps_signal
#if 0
    g_signal_connect( G_OBJECT( p_sys->p_decode_out ), "new-caps",
            G_CALLBACK( caps_handoff_cb ), p_dec );
#else
    GST_VLC_VIDEO_SINK( p_sys->p_decode_out )->new_caps = caps_handoff_cb;
#endif

    p_sys->p_decoder = GST_ELEMENT( gst_bin_new( "decoder" ) );
    VLC_GST_CHECK( p_sys->p_decoder, NULL, "bin not found", VLC_ENOMEM );
    p_sys->p_bus = gst_bus_new( );
    VLC_GST_CHECK( p_sys->p_bus, NULL, "failed to create bus", VLC_ENOMEM );
    gst_element_set_bus( p_sys->p_decoder, p_sys->p_bus );

    gst_bin_add_many( GST_BIN( p_sys->p_decoder ),
            p_sys->p_decode_src, p_sys->p_decode_in,
            p_sys->p_decode_out, NULL );
    gst_object_ref( p_sys->p_decode_src );
    gst_object_ref( p_sys->p_decode_in );
    gst_object_ref( p_sys->p_decode_out );

    b_ret = gst_element_link( p_sys->p_decode_src, p_sys->p_decode_in );
    VLC_GST_CHECK( b_ret, FALSE, "failed to link src <-> in",
            VLC_EGENERIC );

    if( !dbin )
    {
        b_ret = gst_element_link( p_sys->p_decode_in, p_sys->p_decode_out );
        VLC_GST_CHECK( b_ret, FALSE, "failed to link in <-> out",
                VLC_EGENERIC );
    }

    /* set the pipeline to playing */
    i_ret = gst_element_set_state( p_sys->p_decoder, GST_STATE_PLAYING );
    VLC_GST_CHECK( i_ret, GST_STATE_CHANGE_FAILURE,
            "set state failure", VLC_EGENERIC );
    p_sys->b_running = true;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

    vlc_decoder_device *dec_device = decoder_GetDecoderDevice( p_dec );
    if( dec_device == NULL )
    {
        msg_Err( p_dec, "failed to get a decoder device" );
        goto fail;
    }
    p_sys->vctx = vlc_video_context_Create( dec_device, VLC_VIDEO_CONTEXT_GSTDECODE, 0, NULL );
    vlc_decoder_device_Release( dec_device );
    if( unlikely( p_sys->vctx == NULL ) )
    {
        msg_Err( p_dec, "failed to create a video context" );
        goto fail;
    }

    return VLC_SUCCESS;

fail:
    if( caps.p_sinkcaps )
        gst_caps_unref( caps.p_sinkcaps );
    if( caps.p_srccaps )
        gst_caps_unref( caps.p_srccaps );
    if( p_list )
        gst_plugin_feature_list_free( p_list );
    CloseDecoder( ( vlc_object_t* )p_dec );
    return i_rval;
}

/* Flush */
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    GstBuffer *p_buffer;
    gboolean b_ret;

    /* Send a new segment event. Seeking position is
     * irrelevant in this case, as the main motive for a
     * seek here, is to tell the elements to start flushing
     * and start accepting buffers from a new time segment */
    b_ret = gst_element_seek_simple( p_sys->p_decoder,
            GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, 0 );
    msg_Dbg( p_dec, "new segment event : %d", b_ret );

    /* flush the output buffers from the queue */
    while( ( p_buffer = gst_atomic_queue_pop( p_sys->p_que ) ) )
        gst_buffer_unref( p_buffer );

    p_sys->b_prerolled = false;
}

/* Decode */
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    picture_t *p_pic = NULL;
    decoder_sys_t *p_sys = p_dec->p_sys;
    GstMessage *p_msg;

    if( !p_block ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( unlikely( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY |
                    BLOCK_FLAG_CORRUPTED ) ) )
    {
        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
            Flush( p_dec );

        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            goto done;
        }
    }

    if( likely( p_block->i_buffer ) )
    {
        GstBuffer *p_buf;

        p_buf = gst_buffer_new_wrapped_full( GST_MEMORY_FLAG_READONLY,
                p_block->p_start, p_block->i_size,
                p_block->p_buffer - p_block->p_start, p_block->i_buffer,
                p_block, ( GDestroyNotify )block_Release );
        if( unlikely( p_buf == NULL ) )
        {
            msg_Err( p_dec, "failed to create input gstbuffer" );
            block_Release( p_block );
            return VLCDEC_ECRITICAL;
        }

        if( p_block->i_dts != VLC_TICK_INVALID )
            GST_BUFFER_DTS( p_buf ) = gst_util_uint64_scale( p_block->i_dts,
                    GST_SECOND, GST_MSECOND );

        if( p_block->i_pts == VLC_TICK_INVALID )
            GST_BUFFER_PTS( p_buf ) = GST_BUFFER_DTS( p_buf );
        else
            GST_BUFFER_PTS( p_buf ) = gst_util_uint64_scale( p_block->i_pts,
                    GST_SECOND, GST_MSECOND );

        if( p_block->i_length != VLC_TICK_INVALID )
            GST_BUFFER_DURATION( p_buf ) = gst_util_uint64_scale(
                    p_block->i_length, GST_SECOND, GST_MSECOND );

        if( p_dec->fmt_in->video.i_frame_rate  &&
                p_dec->fmt_in->video.i_frame_rate_base )
            GST_BUFFER_DURATION( p_buf ) = gst_util_uint64_scale( GST_SECOND,
                    p_dec->fmt_in->video.i_frame_rate_base,
                    p_dec->fmt_in->video.i_frame_rate );

        /* Give the input buffer to GStreamer Bin.
         *
         *  libvlc                      libvlc
         *    \ (i/p)              (o/p) ^
         *     \                        /
         *   ___v____GSTREAMER BIN_____/____
         *  |                               |
         *  |   appsrc-->decode-->vlcsink   |
         *  |_______________________________|
         *
         * * * * * * * * * * * * * * * * * * * * */
        if( unlikely( gst_app_src_push_buffer(
                        GST_APP_SRC_CAST( p_sys->p_decode_src ), p_buf )
                    != GST_FLOW_OK ) )
        {
            /* block will be released internally,
             * when gst_buffer_unref() is called */
            msg_Err( p_dec, "failed to push buffer" );
            return VLCDEC_ECRITICAL;
        }
    }
    else
        block_Release( p_block );

    /* Poll for any messages, errors */
    p_msg = gst_bus_pop_filtered( p_sys->p_bus,
            GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR |
            GST_MESSAGE_EOS | GST_MESSAGE_WARNING |
            GST_MESSAGE_INFO );
    if( p_msg )
    {
        switch( GST_MESSAGE_TYPE( p_msg ) ){
        case GST_MESSAGE_EOS:
            /* for debugging purpose */
            msg_Warn( p_dec, "got unexpected eos" );
            break;
        /* First buffer received */
        case GST_MESSAGE_ASYNC_DONE:
            /* for debugging purpose */
            p_sys->b_prerolled = true;
            msg_Dbg( p_dec, "Pipeline is prerolled" );
            break;
        default:
            if( default_msg_handler( p_dec, p_msg ) )
            {
                gst_message_unref( p_msg );
                return VLCDEC_ECRITICAL;
            }
            break;
        }
        gst_message_unref( p_msg );
    }

    /* Look for any output buffers in the queue */
    if( gst_atomic_queue_peek( p_sys->p_que ) )
    {
        GstBuffer *p_buf = GST_BUFFER_CAST(
                gst_atomic_queue_pop( p_sys->p_que ));
        GstMemory *p_mem;
        p_mem = gst_buffer_peek_memory( p_buf, 0 );

        bool b_copy_picture = true;

        if( p_mem &&
            GST_IS_VLC_PICTURE_PLANE_ALLOCATOR( p_mem->allocator ))
        {
            b_copy_picture = false;
            p_pic = picture_Hold(( (GstVlcPicturePlane*) p_mem )->p_pic );
        }
        else if( p_mem && gst_is_dmabuf_memory(p_mem) )
        {
            b_copy_picture = false;

            switch( p_dec->fmt_out.video.i_chroma ) {
            case VLC_CODEC_NV12:
                p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec =
                    VLC_CODEC_GST_MEM_OPAQUE;
                break;
            case VLC_CODEC_GST_MEM_OPAQUE:
                break;
            /* fallback */
            default:
                b_copy_picture = true;
            }

            if( !b_copy_picture )
            {
                /* Get a new picture */
                if( decoder_UpdateVideoOutput( p_dec, p_sys->vctx ) )
                {
                    gst_buffer_unref( p_buf );
                    goto done;
                }
                p_pic = decoder_NewPicture( p_dec );
                if( !p_pic )
                {
                    gst_buffer_unref( p_buf );
                    goto done;
                }

                struct gst_mem_pic_context *pctx = calloc( 1, sizeof( *pctx ) );
                if( unlikely( pctx == NULL ) )
                {
                    gst_buffer_unref( p_buf );
                    return VLCDEC_ECRITICAL;
                }

                pctx->s = ( picture_context_t ) {
                    gst_mem_pic_context_Destroy, gst_mem_pic_context_Copy,
                    p_sys->vctx,
                };
                vlc_video_context_Hold( pctx->s.vctx );

                pctx->p_buf = p_buf;
                gst_buffer_ref( p_buf );

                pctx->p_vinfo = &p_sys->vinfo;
                p_pic->context = &pctx->s;
            }
        }

        if( b_copy_picture )
        {
            GstVideoFrame frame;

            /* Get a new picture */
            if( decoder_UpdateVideoFormat( p_dec ) )
            {
                gst_buffer_unref( p_buf );
                goto done;
            }
            p_pic = decoder_NewPicture( p_dec );
            if( !p_pic )
            {
                gst_buffer_unref( p_buf );
                goto done;
            }

            if( unlikely( !gst_video_frame_map( &frame,
                            &p_sys->vinfo, p_buf, GST_MAP_READ ) ) )
            {
                msg_Err( p_dec, "failed to map gst video frame" );
                gst_buffer_unref( p_buf );
                return VLCDEC_ECRITICAL;
            }

            gst_CopyPicture( p_pic, &frame );
            gst_video_frame_unmap( &frame );
        }

        if( p_pic != NULL )
            p_pic->b_progressive = ( p_sys->vinfo.interlace_mode ==
                                     GST_VIDEO_INTERLACE_MODE_PROGRESSIVE );

        if( likely( GST_BUFFER_PTS_IS_VALID( p_buf ) ) )
            p_pic->date = gst_util_uint64_scale(
                GST_BUFFER_PTS( p_buf ), GST_MSECOND, GST_SECOND );
        else
            msg_Warn( p_dec, "Gst Buffer has no timestamp" );

        gst_buffer_unref( p_buf );
    }

done:
    if( p_pic != NULL )
        decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;
}

/* Close the decoder instance */
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = ( decoder_t* )p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    gboolean b_running = p_sys->b_running;

    if( b_running )
    {
        GstMessage *p_msg;
        GstFlowReturn i_ret;

        p_sys->b_running = false;

        /* Send EOS to the pipeline */
        i_ret = gst_app_src_end_of_stream(
                GST_APP_SRC_CAST( p_sys->p_decode_src ));
        msg_Dbg( p_dec, "app src eos: %s", gst_flow_get_name( i_ret ) );

        /* and catch it on the bus with a timeout */
        p_msg = gst_bus_timed_pop_filtered( p_sys->p_bus,
                2000000000ULL, GST_MESSAGE_EOS | GST_MESSAGE_ERROR );

        if( p_msg )
        {
            switch( GST_MESSAGE_TYPE( p_msg ) ){
            case GST_MESSAGE_EOS:
                msg_Dbg( p_dec, "got eos" );
                break;
            default:
                if( default_msg_handler( p_dec, p_msg ) )
                {
                    msg_Err( p_dec, "pipeline may not close gracefully" );
                    return;
                }
                break;
            }

            gst_message_unref( p_msg );
        }
        else
            msg_Warn( p_dec,
                    "no message, pipeline may not close gracefully" );
    }

    /* Remove any left-over buffers from the queue */
    if( p_sys->p_que )
    {
        GstBuffer *p_buf;
        while( ( p_buf = gst_atomic_queue_pop( p_sys->p_que ) ) )
            gst_buffer_unref( p_buf );
        gst_atomic_queue_unref( p_sys->p_que );
    }

    if( b_running && gst_element_set_state( p_sys->p_decoder, GST_STATE_NULL )
            != GST_STATE_CHANGE_SUCCESS )
        msg_Err( p_dec,
                "failed to change the state to NULL," \
                "pipeline may not close gracefully" );

    if( p_sys->p_allocator )
        gst_object_unref( p_sys->p_allocator );
    if( p_sys->p_bus )
        gst_object_unref( p_sys->p_bus );
    if( p_sys->p_decode_src )
        gst_object_unref( p_sys->p_decode_src );
    if( p_sys->p_decode_in )
        gst_object_unref( p_sys->p_decode_in );
    if( p_sys->p_decode_out )
        gst_object_unref( p_sys->p_decode_out );
    if( p_sys->p_decoder )
        gst_object_unref( p_sys->p_decoder );

    if( p_sys->vctx )
        vlc_video_context_Release( p_sys->vctx );

    free( p_sys );
}

static const struct vlc_decoder_device_operations gstdecode_device_ops = {
    .close = NULL,
};

static int OpenDecoderDevice(vlc_decoder_device *device, vlc_window_t *window)
{
    VLC_UNUSED(window);

    device->ops = &gstdecode_device_ops;
    device->type = VLC_DECODER_DEVICE_GSTDECODE;

    return VLC_SUCCESS;
}
