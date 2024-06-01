/*****************************************************************************
 * dxgi_fmt.c : DXGI GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#include <vlc_es.h>

#include "dxgi_fmt.h"

#include <assert.h>

typedef struct
{
    const char   *name;
    DXGI_FORMAT  format;
    vlc_fourcc_t vlc_format;
} dxgi_format_t;

static const dxgi_format_t dxgi_formats[] = {
    { "NV12",        DXGI_FORMAT_NV12,                VLC_CODEC_NV12     },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,          0                  },
    { "RGBA",        DXGI_FORMAT_R8G8B8A8_UNORM,      VLC_CODEC_RGBA     },
    { "RGBA_SRGB",   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, VLC_CODEC_RGBA     },
    { "BGRX",        DXGI_FORMAT_B8G8R8X8_UNORM,      VLC_CODEC_RGB32    },
    { "BGRA",        DXGI_FORMAT_B8G8R8A8_UNORM,      VLC_CODEC_BGRA     },
    { "BGRA_SRGB",   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, VLC_CODEC_BGRA     },
    { "AYUV",        DXGI_FORMAT_AYUV,                VLC_CODEC_VUYA     },
    { "YUY2",        DXGI_FORMAT_YUY2,                VLC_CODEC_YUYV     },
    { "AI44",        DXGI_FORMAT_AI44,                0                  },
    { "P8",          DXGI_FORMAT_P8,                  0                  },
    { "A8P8",        DXGI_FORMAT_A8P8,                0                  },
    { "B5G6R5",      DXGI_FORMAT_B5G6R5_UNORM,        VLC_CODEC_RGB16    },
    { "Y416",        DXGI_FORMAT_Y416,                0                  },
    { "P010",        DXGI_FORMAT_P010,                VLC_CODEC_P010     },
    { "P016",        DXGI_FORMAT_P016,                VLC_CODEC_P016     },
    { "Y210",        DXGI_FORMAT_Y210,                VLC_CODEC_Y210     },
    { "Y410",        DXGI_FORMAT_Y410,                VLC_CODEC_Y410     },
    { "NV11",        DXGI_FORMAT_NV11,                0                  },
    { "RGB10A2",     DXGI_FORMAT_R10G10B10A2_UNORM,   VLC_CODEC_RGBA10   },
    { "RGB16",       DXGI_FORMAT_R16G16B16A16_UNORM,  VLC_CODEC_RGBA64   },
    { "RGB16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT,  0                  },
    { "UNKNOWN",     DXGI_FORMAT_UNKNOWN,             0                  },

    { NULL, 0, 0}
};

static const d3d_format_t d3d_formats[] = {
    { "NV12",     DXGI_FORMAT_NV12,           VLC_CODEC_NV12,              8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "VA_NV12",  DXGI_FORMAT_NV12,           VLC_CODEC_D3D11_OPAQUE,      8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "P010",     DXGI_FORMAT_P010,           VLC_CODEC_P010,             10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_P010",  DXGI_FORMAT_P010,           VLC_CODEC_D3D11_OPAQUE_10B, 10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_AYUV",  DXGI_FORMAT_AYUV,           VLC_CODEC_D3D11_OPAQUE,      8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "YUY2",     DXGI_FORMAT_YUY2,           VLC_CODEC_YUYV,              8, 1, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_YUY2",  DXGI_FORMAT_YUY2,           VLC_CODEC_D3D11_OPAQUE,      8, 1, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
#ifdef BROKEN_PIXEL
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L,     16, 1, 1, { DXGI_FORMAT_R16G16B16A16_UINT } },
#endif
    { "VA_Y210",  DXGI_FORMAT_Y210,           VLC_CODEC_D3D11_OPAQUE_10B, 10, 1, 2, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "VA_Y410",  DXGI_FORMAT_Y410,           VLC_CODEC_D3D11_OPAQUE_10B, 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
#ifdef UNTESTED
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L,     10, 1, 2, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444,         10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,          8, 4, 1, { DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM} },
#endif
    { "I420",     DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I420,          8, 2, 2, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I420_10",  DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I420_10L,     10, 2, 2, { DXGI_FORMAT_R16_UNORM,     DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM } },
    { "YUVA",     DXGI_FORMAT_UNKNOWN,        VLC_CODEC_YUVA,          8, 1, 1, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I444",     DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I444,          8, 1, 1, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I444_16",  DXGI_FORMAT_UNKNOWN,        VLC_CODEC_I444_16L,     16, 1, 1, { DXGI_FORMAT_R16_UNORM,     DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM } },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_BGRA,          8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, VLC_CODEC_D3D11_OPAQUE_BGRA,  8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_RGBA,          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, VLC_CODEC_D3D11_OPAQUE_RGBA,  8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, VLC_CODEC_RGB32,         8, 1, 1, { DXGI_FORMAT_B8G8R8X8_UNORM } },
    { "RGBA64",   DXGI_FORMAT_R16G16B16A16_UNORM, VLC_CODEC_RGBA64,   16, 1, 1, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "RGB10A2",  DXGI_FORMAT_R10G10B10A2_UNORM, VLC_CODEC_RGBA10,    10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "VA_RGB10", DXGI_FORMAT_R10G10B10A2_UNORM, VLC_CODEC_D3D11_OPAQUE_RGBA, 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "AYUV",     DXGI_FORMAT_AYUV,           VLC_CODEC_VUYA,          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   VLC_CODEC_RGB16,         5, 1, 1, { DXGI_FORMAT_B5G6R5_UNORM } },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,  VLC_CODEC_D3D11_OPAQUE,  8, 2, 2, { DXGI_FORMAT_UNKNOWN } },

    { NULL, 0, 0, 0, 0, 0, { DXGI_FORMAT_UNKNOWN } }
};

const char *DxgiFormatToStr(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->name;
    }
    return NULL;
}

vlc_fourcc_t DxgiFormatFourcc(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->vlc_format;
    }
    return 0;
}

DXGI_FORMAT DxgiFourccFormat(vlc_fourcc_t fcc)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->vlc_format == fcc)
            return f->format;
    }
    return DXGI_FORMAT_UNKNOWN;
}

const d3d_format_t *DxgiGetRenderFormatList(void)
{
    return d3d_formats;
}

void DxgiFormatMask(DXGI_FORMAT format, video_format_t *fmt)
{
    if (format == DXGI_FORMAT_B8G8R8X8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        fmt->i_rmask = 0x0000ff00;
        fmt->i_gmask = 0x00ff0000;
        fmt->i_bmask = 0xff000000;
    }
}

const char *DxgiVendorStr(unsigned int gpu_vendor)
{
    static const struct {
        unsigned   id;
        const char name[32];
    } vendors [] = {
        { GPU_MANUFACTURER_AMD,      "ATI"         },
        { GPU_MANUFACTURER_NVIDIA,   "NVIDIA"      },
        { GPU_MANUFACTURER_VIA,      "VIA"         },
        { GPU_MANUFACTURER_INTEL,    "Intel"       },
        { GPU_MANUFACTURER_S3,       "S3 Graphics" },
        { GPU_MANUFACTURER_QUALCOMM, "Qualcomm"    },
        { 0,                         "Unknown" }
    };

    int i = 0;
    for (i = 0; vendors[i].id != 0; i++) {
        if (vendors[i].id == gpu_vendor)
            break;
    }
    return vendors[i].name;
}

UINT DxgiResourceCount(const d3d_format_t *d3d_fmt)
{
    for (UINT count=0; count<DXGI_MAX_SHADER_VIEW; count++)
    {
        if (d3d_fmt->resourceFormat[count] == DXGI_FORMAT_UNKNOWN)
            return count;
    }
    return DXGI_MAX_SHADER_VIEW;
}

bool DxgiIsRGBFormat(const d3d_format_t *cfg)
{
    return cfg->resourceFormat[0] != DXGI_FORMAT_R8_UNORM &&
           cfg->resourceFormat[0] != DXGI_FORMAT_R16_UNORM &&
           cfg->formatTexture != DXGI_FORMAT_YUY2 &&
           cfg->formatTexture != DXGI_FORMAT_AYUV &&
           cfg->formatTexture != DXGI_FORMAT_Y210 &&
           cfg->formatTexture != DXGI_FORMAT_Y410 &&
           cfg->formatTexture != DXGI_FORMAT_420_OPAQUE;
}

void DXGI_GetBlackColor( const d3d_format_t *pixelFormat,
                         union DXGI_Color black[DXGI_MAX_RENDER_TARGET],
                         size_t colors[DXGI_MAX_RENDER_TARGET] )
{
    static const union DXGI_Color blackY    = { .y = 0.0f };
    static const union DXGI_Color blackUV   = { .u = 0.5f, .v = 0.5f };
    static const union DXGI_Color blackRGBA = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f };
    static const union DXGI_Color blackYUY2 = { .r = 0.0f, .g = 0.5f, .b = 0.0f, .a = 0.5f };
    static const union DXGI_Color blackVUYA = { .r = 0.5f, .g = 0.5f, .b = 0.0f, .a = 1.0f };
    static const union DXGI_Color blackY210 = { .r = 0.0f, .g = 0.5f, .b = 0.5f, .a = 0.0f };

    static_assert(DXGI_MAX_RENDER_TARGET >= 2, "we need at least 2 RenderTargetView for NV12/P010");

    switch (pixelFormat->formatTexture)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        colors[0] = 1; black[0] = blackY;
        colors[1] = 2; black[1] = blackUV;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
        colors[0] = 4; black[0] = blackRGBA;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_YUY2:
        colors[0] = 4; black[0] = blackYUY2;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_Y410:
        colors[0] = 4; black[0] = blackVUYA;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_Y210:
        colors[0] = 4; black[0] = blackY210;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_AYUV:
        colors[0] = 4; black[0] = blackVUYA;
        colors[1] = 0;
        break;
    default:
        vlc_assert_unreachable();
    }
}
