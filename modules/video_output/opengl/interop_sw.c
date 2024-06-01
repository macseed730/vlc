/*****************************************************************************
 * converter_sw.c: OpenGL converters for software video formats
 *****************************************************************************
 * Copyright (C) 2016,2017 VLC authors and VideoLAN
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

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_opengl_interop.h>

#include "gl_util.h"
#include "interop.h"

#define PBO_DISPLAY_COUNT 2 /* Double buffering */
typedef struct
{
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    GLuint      buffers[PICTURE_PLANE_MAX];
    size_t      bytes[PICTURE_PLANE_MAX];
} picture_sys_t;

struct priv
{
    bool   has_gl_3;
    bool   has_texture_rg;
    bool   has_unpack_subimage;
    void * texture_temp_buf;
    size_t texture_temp_buf_size;
    struct {
        picture_t *display_pics[PBO_DISPLAY_COUNT];
        size_t display_idx;
    } pbo;

#define OPENGL_VTABLE_F(X) \
        X(PFNGLGETERRORPROC,        GetError) \
        X(PFNGLGETINTEGERVPROC,     GetIntegerv) \
        X(PFNGLGETSTRINGPROC,       GetString) \
        \
        X(PFNGLACTIVETEXTUREPROC,   ActiveTexture) \
        X(PFNGLBINDTEXTUREPROC,     BindTexture) \
        X(PFNGLTEXIMAGE2DPROC,      TexImage2D) \
        X(PFNGLTEXSUBIMAGE2DPROC,   TexSubImage2D) \
        \
        X(PFNGLBINDBUFFERPROC,      BindBuffer) \
        X(PFNGLBUFFERDATAPROC,      BufferData) \
        X(PFNGLBUFFERSUBDATAPROC,   BufferSubData) \
        X(PFNGLDELETEBUFFERSPROC,   DeleteBuffers) \
        X(PFNGLGENBUFFERSPROC,      GenBuffers) \
        X(PFNGLPIXELSTOREIPROC,     PixelStorei)
    struct {
#define DECLARE_SYMBOL(type, name) type name;
        OPENGL_VTABLE_F(DECLARE_SYMBOL)
    } gl;
};

static void
pbo_picture_destroy(picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    picsys->DeleteBuffers(pic->i_planes, picsys->buffers);

    free(picsys);
}

static picture_t *
pbo_picture_create(const struct vlc_gl_interop *interop)
{
    const struct priv *priv = interop->priv;
    picture_sys_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL))
        return NULL;

    picture_resource_t rsc = {
        .p_sys = picsys,
        .pf_destroy = pbo_picture_destroy,
    };
    picture_t *pic = picture_NewFromResource(&interop->fmt_out, &rsc);
    if (pic == NULL)
    {
        free(picsys);
        return NULL;
    }

    priv->gl.GenBuffers(pic->i_planes, picsys->buffers);
    picsys->DeleteBuffers = priv->gl.DeleteBuffers;

    /* XXX: needed since picture_NewFromResource override pic planes */
    if (picture_Setup(pic, &interop->fmt_out))
    {
        picture_Release(pic);
        return NULL;
    }

    assert(pic->i_planes > 0
        && (unsigned) pic->i_planes <= interop->tex_count);

    for (int i = 0; i < pic->i_planes; ++i)
    {
        const plane_t *p = &pic->p[i];

        if( p->i_pitch < 0 || p->i_lines <= 0 ||
            (size_t)p->i_pitch > SIZE_MAX/p->i_lines )
        {
            picture_Release(pic);
            return NULL;
        }
        picsys->bytes[i] = p->i_pitch * p->i_lines;
    }
    return pic;
}

static int
pbo_data_alloc(const struct vlc_gl_interop *interop, picture_t *pic)
{
    const struct priv *priv = interop->priv;
    picture_sys_t *picsys = pic->p_sys;

    priv->gl.GetError();

    for (int i = 0; i < pic->i_planes; ++i)
    {
        priv->gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        priv->gl.BufferData(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                                GL_DYNAMIC_DRAW);

        if (priv->gl.GetError() != GL_NO_ERROR)
        {
            msg_Err(interop->gl, "could not alloc PBO buffers");
            priv->gl.DeleteBuffers(i, picsys->buffers);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static int
pbo_pics_alloc(const struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT; ++i)
    {
        picture_t *pic = priv->pbo.display_pics[i] =
            pbo_picture_create(interop);
        if (pic == NULL)
            goto error;

        if (pbo_data_alloc(interop, pic) != VLC_SUCCESS)
            goto error;
    }

    /* turn off pbo */
    priv->gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
error:
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    return VLC_EGENERIC;
}

static int
tc_pbo_update(const struct vlc_gl_interop *interop, uint32_t textures[],
              const int32_t tex_width[], const int32_t tex_height[],
              picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset; assert(plane_offset == NULL);
    struct priv *priv = interop->priv;

    picture_t *display_pic = priv->pbo.display_pics[priv->pbo.display_idx];
    picture_sys_t *p_sys = display_pic->p_sys;
    priv->pbo.display_idx = (priv->pbo.display_idx + 1) % PBO_DISPLAY_COUNT;

    for (int i = 0; i < pic->i_planes; i++)
    {
        GLsizeiptr size = pic->p[i].i_lines * pic->p[i].i_pitch;
        const GLvoid *data = pic->p[i].p_pixels;
        priv->gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                           p_sys->buffers[i]);
        priv->gl.BufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size, data);

        priv->gl.ActiveTexture(GL_TEXTURE0 + i);
        priv->gl.BindTexture(interop->tex_target, textures[i]);

        //for YUV with interleaved UV, pixel pitch is reported as 1
        if (pic->i_planes == 2 && interop->tex_count == 2 && i == 1)
            priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pic->p[i].i_pitch / (pic->p[i].i_pixel_pitch * 2));
        else
            priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        priv->gl.TexSubImage2D(interop->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                                   interop->texs[i].format, interop->texs[i].type, NULL);
        priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    if (pic->i_planes == 1 && interop->tex_count == 2)
    {
        /* For YUV 4:2:2 formats, a single plane is uploaded into 2 textures */
        priv->gl.ActiveTexture(GL_TEXTURE1);
        priv->gl.BindTexture(interop->tex_target, textures[1]);
        priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, (pic->p[0].i_pitch / pic->p[0].i_pixel_pitch) >> 1); /* yuv[Y] */
        priv->gl.TexSubImage2D(interop->tex_target, 0, 0, 0, tex_width[1], tex_height[1],
                               interop->texs[1].format, interop->texs[1].type, NULL);
        priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    GL_ASSERT_NOERROR(&priv->gl);

    /* turn off pbo */
    priv->gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static int
tc_common_allocate_textures(const struct vlc_gl_interop *interop, uint32_t textures[],
                            const int32_t tex_width[], const int32_t tex_height[])
{
    const struct priv *priv = interop->priv;
    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        priv->gl.BindTexture(interop->tex_target, textures[i]);
        priv->gl.TexImage2D(interop->tex_target, 0, interop->texs[i].internal,
                                tex_width[i], tex_height[i], 0, interop->texs[i].format,
                                interop->texs[i].type, NULL);
        GL_ASSERT_NOERROR(&priv->gl);
    }
    return VLC_SUCCESS;
}

static int
upload_plane(const struct vlc_gl_interop *interop, unsigned tex_idx,
             int32_t width, int32_t height, size_t pitch, size_t pixel_size,
             const void *pixels, unsigned pixel_pack)
{
    struct priv *priv = interop->priv;
    GLenum tex_format = interop->texs[tex_idx].format;
    GLenum tex_type = interop->texs[tex_idx].type;

    /* This unpack alignment is the default, but setting it just in case. */
    priv->gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);

    assert(height > 0);
    assert(width > 0);
    assert(pixel_size);
    assert(pitch % pixel_size == 0);
    assert((size_t) width * pixel_size <= pitch);

    size_t width_bytes = width * pixel_size * pixel_pack;

    if (!priv->has_unpack_subimage)
    {
        if (pitch != width_bytes)
        {
            size_t aligned_row_len = vlc_align(width_bytes, 4);
            size_t buf_size = aligned_row_len * height;
            const uint8_t *source = pixels;
            uint8_t *destination;
            if (priv->texture_temp_buf_size < buf_size)
            {
                priv->texture_temp_buf =
                    realloc_or_free(priv->texture_temp_buf, buf_size);
                if (priv->texture_temp_buf == NULL)
                {
                    priv->texture_temp_buf_size = 0;
                    return VLC_ENOMEM;
                }
                priv->texture_temp_buf_size = buf_size;
            }
            destination = priv->texture_temp_buf;

            for (GLsizei h = 0; h < height ; h++)
            {
                memcpy(destination, source, width_bytes);
                source += pitch;
                destination += aligned_row_len;
            }
            priv->gl.TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                                   tex_format, tex_type, priv->texture_temp_buf);
        }
        else
        {
            priv->gl.TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                                   tex_format, tex_type, pixels);
        }
    }
    else
    {
        priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch / (pixel_size * pixel_pack ));

        priv->gl.TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                               tex_format, tex_type, pixels);
        priv->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    GL_ASSERT_NOERROR(&priv->gl);

    return VLC_SUCCESS;
}

static int
tc_common_update(const struct vlc_gl_interop *interop, uint32_t textures[],
                 const int32_t tex_width[], const int32_t tex_height[],
                 picture_t *pic, const size_t *plane_offset)
{
    const struct priv *priv = interop->priv;
    int ret = VLC_SUCCESS;
    for (int i = 0; i < pic->i_planes && ret == VLC_SUCCESS; i++)
    {
        assert(textures[i] != 0);
        priv->gl.ActiveTexture(GL_TEXTURE0 + i);
        priv->gl.BindTexture(interop->tex_target, textures[i]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[i].p_pixels[plane_offset[i]] :
                             pic->p[i].p_pixels;

        //are we uploading packed UV plane
        unsigned pixel_pack = (pic->i_planes == 2 && i == 1) ? 2 : 1;
        ret = upload_plane(interop, i, tex_width[i], tex_height[i],
                           pic->p[i].i_pitch, pic->p[i].i_pixel_pitch, pixels, pixel_pack);
    }

    if (pic->i_planes == 1 && interop->tex_count == 2)
    {
        /* For YUV 4:2:2 formats, a single plane is uploaded into 2 textures */
        assert(textures[1] != 0);
        priv->gl.ActiveTexture(GL_TEXTURE1);
        priv->gl.BindTexture(interop->tex_target, textures[1]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[0].p_pixels[plane_offset[0]] :
                             pic->p[0].p_pixels;

        ret = upload_plane(interop, 1, tex_width[1], tex_height[1],
                           pic->p[0].i_pitch, pic->p[0].i_pixel_pitch, pixels, 2);
    }

    return ret;
}

static inline void
DivideRationalByTwo(vlc_rational_t *r) {
    if (r->num % 2 == 0)
        r->num /= 2;
    else
        r->den *= 2;
}

static bool fixGLFormat(struct vlc_gl_interop *interop, GLint* intfmt, GLint* fmt)
{
    struct priv *priv = interop->priv;
    if (*intfmt == 0)
        return true;

    //GLES 3.0, OpenGL 3.0 and OpenGL with GL_ARB_texture_rg
    //don't need transformations
    if (priv->has_gl_3
        || (priv->has_texture_rg && interop->gl->api_type == VLC_OPENGL))
        return true;

    //for GLES2 GL_EXT_texture_rg we need to use GL_RED/GL_RG as internal format
    if (priv->has_texture_rg)
    {
        switch (*intfmt) {
        case GL_R8:
            *intfmt = GL_RED;
            *fmt = GL_RED;
            break;
        case GL_RG8:
            *intfmt = GL_RG;
            *fmt = GL_RG;
            break;
        case GL_R16UI:
        case GL_RG16UI:
            return false;
        default:
            vlc_assert_unreachable();
        }

        return true;
    }

    //fallback to GL_LUMINANCE / GL_LUMINANCE_ALPHA
    switch (*intfmt) {
    case GL_R8:
        *intfmt = GL_LUMINANCE;
        *fmt = GL_LUMINANCE;
        break;
    case GL_R16UI:
        if (interop->gl->api_type == VLC_OPENGL_ES2)
            return false;

        *intfmt = GL_LUMINANCE16;
        *fmt = GL_LUMINANCE;
        break;
    case GL_RG8:
        *intfmt = GL_LUMINANCE_ALPHA;
        *fmt = GL_LUMINANCE_ALPHA;
        break;
    case GL_RG16UI:
        if (interop->gl->api_type == VLC_OPENGL_ES2)
            return false;

        *intfmt = GL_LUMINANCE16_ALPHA16;
        *fmt = GL_LUMINANCE_ALPHA;
        break;
    default:
        vlc_assert_unreachable();
    }
    return true;
}

static int
interop_yuv_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma, const vlc_chroma_description_t *desc)
{
    struct priv *priv = interop->priv;

    (void) chroma;

    struct interop_formats
    {
        GLint intfmt;
        GLint fmt;
        GLint plane2_intfmt;
        GLint plane2_fmt;
        GLint type;
        GLint plane2_type;
    } formats[] = {
        // 3 and 4 planes
            // 8 bits pixels
            { GL_R8, GL_RED, 0, 0, GL_UNSIGNED_BYTE, 0 },
            // 16 bits pixels
            { GL_R16UI, GL_RED, 0, 0, GL_UNSIGNED_SHORT, 0 },
        // 2 planes
            // 8 bits pixels
            { GL_R8, GL_RED, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE },
            // 16 bits pixels
            { GL_R16UI, GL_RED, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, GL_UNSIGNED_SHORT },
        // 1 plane is a special case that will be handled explicitly
    };

    if (desc->plane_count == 1)
    {
        if (chroma == VLC_CODEC_VUYA)
        {
            interop->tex_count = 2;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 },
                GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
            };
            return VLC_SUCCESS;
        }
        else if (desc->pixel_size != 2)
        {
            msg_Warn(interop->gl, "unsupported chroma %.4s", (char*)&chroma);
            return VLC_EGENERIC;
        }

        /* Only YUV 4:2:2 formats */
        /* The pictures have only 1 plane, but it is uploaded twice, once to
         * access the Y components, once to access the UV components. See
         * #26712. */
        GLint intfmt = GL_RG8;
        GLint fmt = GL_RG;
        if (!fixGLFormat(interop, &intfmt, &fmt))
            return VLC_EGENERIC;

        interop->tex_count = 2;
        interop->texs[0] = (struct vlc_gl_tex_cfg) {
            { 1, 1 }, { 1, 1 },
            intfmt, fmt, GL_UNSIGNED_BYTE
        };
        interop->texs[1] = (struct vlc_gl_tex_cfg) {
            { 1, 2 }, { 1, 1 },
            GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
        };
        return VLC_SUCCESS;
    }

    uint8_t format_index = 0;
    switch (desc->plane_count)
    {
    case 4:
    case 3:
        break;
    case 2:
        format_index += 2;
        break;
    default:
        vlc_assert_unreachable();
        break;
    }

    switch (desc->pixel_size)
    {
    case 1:
        break;
    case 2:
        format_index += 1;
        break;
    default:
        return VLC_EGENERIC;
    }

    assert(format_index < ARRAY_SIZE(formats));

    struct interop_formats* format = &formats[format_index];

    GLint plane1_intfmt = format->intfmt;
    GLint plane1_fmt = format->fmt;
    if (!fixGLFormat(interop, &plane1_intfmt, &plane1_fmt))
        return VLC_EGENERIC;

    GLint plane2_intfmt = format->plane2_intfmt;
    GLint plane2_fmt = format->plane2_fmt;
    if (!fixGLFormat(interop, &plane2_intfmt, &plane2_fmt))
        return VLC_EGENERIC;

    msg_Dbg(interop, "Using format at index %u", format_index);
    msg_Dbg(interop, "Plane1: fmt=%#x intfmt=%#x type=%#x", plane1_fmt,
            plane1_intfmt, format->type);
    msg_Dbg(interop, "Plane2: fmt=%#x intfmt=%#x type=%#x", plane2_fmt,
            plane2_intfmt, format->plane2_type);


    if (desc->pixel_size == 2)
    {
        if (vlc_gl_interop_GetTexFormatSize(interop, tex_target, format->fmt,
                                            format->intfmt, GL_UNSIGNED_SHORT) != 16)
            return VLC_EGENERIC;
    }

    if (desc->plane_count >= 3)
    {
        interop->tex_count = desc->plane_count;
        for (unsigned i = 0; i < interop->tex_count; ++i )
        {
            interop->texs[i] = (struct vlc_gl_tex_cfg) {
                { desc->p[i].w.num, desc->p[i].w.den },
                { desc->p[i].h.num, desc->p[i].h.den },
                plane1_intfmt, plane1_fmt, format->type
            };
        }
    }
    else if (desc->plane_count == 2)
    {
        interop->tex_count = 2;

        if (desc->pixel_size == 2 &&
            vlc_gl_interop_GetTexFormatSize(interop, tex_target, format->plane2_fmt,
                format->plane2_intfmt, format->plane2_type) != 16)
        {
            return VLC_EGENERIC;
        }

        interop->texs[0] = (struct vlc_gl_tex_cfg) {
            { desc->p[0].w.num, desc->p[0].w.den },
            { desc->p[0].h.num, desc->p[0].h.den },
            plane1_intfmt, plane1_fmt, format->type
        };
        interop->texs[1] = (struct vlc_gl_tex_cfg) {
            { desc->p[1].w.num, desc->p[1].w.den },
            { desc->p[1].h.num, desc->p[1].h.den },
            plane2_intfmt, plane2_fmt, format->plane2_type
        };

        /*
         * If plane_count == 2, then the chroma is semiplanar: the U and V
         * planes are packed in the second plane. As a consequence, the
         * horizontal scaling, as reported in the vlc_chroma_description_t, is
         * doubled.
         *
         * But once imported as an OpenGL texture, both components are stored
         * in a single texel (the two first components of the vec4).
         * Therefore, from OpenGL, the width is not doubled, so the horizontal
         * scaling must be divided by 2 to compensate.
         */
         DivideRationalByTwo(&interop->texs[1].w);
    }

    return VLC_SUCCESS;
}

static int
interop_rgb_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma)
{
    (void) tex_target;

    switch (chroma)
    {
        case VLC_CODEC_RGB24:
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE
            };
            break;

        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
            };
            break;
        case VLC_CODEC_BGRA: {
            if (vlc_gl_interop_GetTexFormatSize(interop, tex_target, GL_BGRA, GL_RGBA,
                                                GL_UNSIGNED_BYTE) != 32)
                return VLC_EGENERIC;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE
            };
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    interop->tex_count = 1;
    return VLC_SUCCESS;
}

static void
interop_xyz12_init(struct vlc_gl_interop *interop)
{
    interop->tex_count = 1;
    interop->tex_target = GL_TEXTURE_2D;
    interop->texs[0] = (struct vlc_gl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT
    };
}

static int
opengl_interop_init(struct vlc_gl_interop *interop, GLenum tex_target,
                    vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    bool is_yuv = vlc_fourcc_IsYUV(chroma);
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(chroma);
    if (!desc)
        return VLC_EGENERIC;

    assert(!interop->fmt_out.p_palette);
    interop->fmt_out.i_chroma = chroma;
    interop->fmt_out.space = yuv_space;
    interop->tex_target = tex_target;

    if (chroma == VLC_CODEC_XYZ12)
    {
        interop_xyz12_init(interop);
        return VLC_SUCCESS;
    }

    if (is_yuv)
        return interop_yuv_base_init(interop, tex_target, chroma, desc);

    return interop_rgb_base_init(interop, tex_target, chroma);
}

static void
opengl_interop_generic_deinit(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    free(priv->texture_temp_buf);
    free(priv);
}

static int
opengl_interop_generic_init(struct vlc_gl_interop *interop, bool allow_dr)
{

    struct priv *priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

    interop->priv = priv;

#define LOAD_SYMBOL(type, name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name); \
    assert(priv->gl.name != NULL);

    OPENGL_VTABLE_F(LOAD_SYMBOL);

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    /* OpenGL or OpenGL ES2 with GL_EXT_unpack_subimage ext */
    priv->has_unpack_subimage = interop->gl->api_type == VLC_OPENGL
        || vlc_gl_HasExtension(&extension_vt, "GL_EXT_unpack_subimage");

    /* RG textures are available natively since OpenGL 3.0 and OpenGL ES 3.0 */
    priv->has_gl_3 = vlc_gl_GetVersionMajor(&extension_vt) >= 3;

    priv->has_texture_rg =
        (interop->gl->api_type == VLC_OPENGL
            && vlc_gl_HasExtension(&extension_vt, "GL_ARB_texture_rg"))
        || (interop->gl->api_type == VLC_OPENGL_ES2
            && vlc_gl_HasExtension(&extension_vt, "GL_EXT_texture_rg"));

    video_color_space_t space;
    const vlc_fourcc_t *list;

    if (vlc_fourcc_IsYUV(interop->fmt_in.i_chroma))
    {
        GLint max_texture_units = 0;
        priv->gl.GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
        if (max_texture_units < 3)
            goto error;

        list = vlc_fourcc_GetYUVFallback(interop->fmt_in.i_chroma);
        space = interop->fmt_in.space;
    }
    else if (interop->fmt_in.i_chroma == VLC_CODEC_XYZ12)
    {
        static const vlc_fourcc_t xyz12_list[] = { VLC_CODEC_XYZ12, 0 };
        list = xyz12_list;
        space = COLOR_SPACE_UNDEF;
    }
    else
    {
        list = vlc_fourcc_GetRGBFallback(interop->fmt_in.i_chroma);
        space = COLOR_SPACE_UNDEF;
    }

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    /* Check whether the given chroma is translatable to OpenGL. */
    vlc_fourcc_t i_chroma = interop->fmt_in.i_chroma;
    int ret = opengl_interop_init(interop, GL_TEXTURE_2D, i_chroma, space);
    if (ret == VLC_SUCCESS)
        goto interop_init;

    /* Check whether any fallback for the chroma is translatable to OpenGL. */
    while (*list)
    {
        ret = opengl_interop_init(interop, GL_TEXTURE_2D, *list, space);
        if (ret == VLC_SUCCESS)
        {
            i_chroma = *list;
            goto interop_init;
        }
        list++;
    }

    goto error;

interop_init:
    /* We found a chroma with matching parameters for OpenGL. The interop can
     * be created. */

    // TODO: video_format_FixRgb is not fixing the mask we assign here
    if (i_chroma == VLC_CODEC_RGB32)
    {
#if defined(WORDS_BIGENDIAN)
        interop->fmt_out.i_rmask  = 0xff000000;
        interop->fmt_out.i_gmask  = 0x00ff0000;
        interop->fmt_out.i_bmask  = 0x0000ff00;
#else
        interop->fmt_out.i_rmask  = 0x000000ff;
        interop->fmt_out.i_gmask  = 0x0000ff00;
        interop->fmt_out.i_bmask  = 0x00ff0000;
#endif
        video_format_FixRgb(&interop->fmt_out);
    }

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = tc_common_allocate_textures,
        .update_textures = tc_common_update,
        .close = opengl_interop_generic_deinit,
    };
    interop->ops = &ops;
    interop->fmt_in.i_chroma = i_chroma;

    if (allow_dr && priv->has_unpack_subimage)
    {
        /* Ensure we do direct rendering / PBO with OpenGL 3.0 or higher. */
        const unsigned char *ogl_version = priv->gl.GetString(GL_VERSION);
        const bool glver_ok = strverscmp((const char *)ogl_version, "3.0") >= 0;

        const bool has_pbo = glver_ok &&
            (vlc_gl_HasExtension(&extension_vt, "GL_ARB_pixel_buffer_object") ||
             vlc_gl_HasExtension(&extension_vt, "GL_EXT_pixel_buffer_object"));

        const bool supports_pbo = has_pbo && priv->gl.BufferData
            && priv->gl.BufferSubData;
        if (supports_pbo && pbo_pics_alloc(interop) == VLC_SUCCESS)
        {
            static const struct vlc_gl_interop_ops pbo_ops = {
                .allocate_textures = tc_common_allocate_textures,
                .update_textures = tc_pbo_update,
                .close = opengl_interop_generic_deinit,
            };
            interop->ops = &pbo_ops;
            msg_Dbg(interop->gl, "PBO support enabled");
        }
    }

    return VLC_SUCCESS;

error:
    free(priv);
    interop->priv = NULL;
    return VLC_EGENERIC;
}

static int OpenInteropSW(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;
    return opengl_interop_generic_init(interop, false);
}

static int OpenInteropDirectRendering(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;
    return opengl_interop_generic_init(interop, true);
}

vlc_module_begin ()
    set_description("Software OpenGL interop")
    set_capability("opengl sw interop", 1)
    set_callback(OpenInteropSW)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("sw")

    add_submodule()
    set_callback(OpenInteropDirectRendering)
    set_capability("opengl sw interop", 2)
    add_shortcut("pbo")
vlc_module_end ()
