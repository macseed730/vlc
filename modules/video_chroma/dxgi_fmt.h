/*****************************************************************************
 * dxgi_fmt.h : DXGI helper calls
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

#ifndef VLC_VIDEOCHROMA_DXGI_FMT_H_
#define VLC_VIDEOCHROMA_DXGI_FMT_H_

#include <dxgi.h>
#include <dxgiformat.h>

#include <vlc_common.h>
#include <vlc_fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif// __cplusplus

#define GPU_MANUFACTURER_AMD           0x1002
#define GPU_MANUFACTURER_NVIDIA        0x10DE
#define GPU_MANUFACTURER_VIA           0x1106
#define GPU_MANUFACTURER_INTEL         0x8086
#define GPU_MANUFACTURER_S3            0x5333
#define GPU_MANUFACTURER_QUALCOMM  0x4D4F4351

#define DXGI_MAX_SHADER_VIEW     4
#define DXGI_MAX_RENDER_TARGET   2 // for NV12/P010 we render Y and UV separately

typedef struct
{
    const char   *name;
    DXGI_FORMAT  formatTexture;
    vlc_fourcc_t fourcc;
    uint8_t      bitsPerChannel;
    uint8_t      widthDenominator;
    uint8_t      heightDenominator;
    DXGI_FORMAT  resourceFormat[DXGI_MAX_SHADER_VIEW];
} d3d_format_t;

const char *DxgiFormatToStr(DXGI_FORMAT format);
vlc_fourcc_t DxgiFormatFourcc(DXGI_FORMAT format);
const d3d_format_t *DxgiGetRenderFormatList(void);
void DxgiFormatMask(DXGI_FORMAT format, video_format_t *);
DXGI_FORMAT DxgiFourccFormat(vlc_fourcc_t fcc);
const char *DxgiVendorStr(unsigned int gpu_vendor);
UINT DxgiResourceCount(const d3d_format_t *);

bool DxgiIsRGBFormat(const d3d_format_t *);

#define DXGI_RGB_FORMAT  1
#define DXGI_YUV_FORMAT  2

#define DXGI_CHROMA_CPU 1
#define DXGI_CHROMA_GPU 2

union DXGI_Color
{
    struct {
        FLOAT r, g, b, a;
    };
    struct {
        FLOAT y;
    };
    struct {
        FLOAT u, v;
    };
    FLOAT array[4];
};
void DXGI_GetBlackColor( const d3d_format_t *,
                         union DXGI_Color black[DXGI_MAX_RENDER_TARGET],
                         size_t colors[DXGI_MAX_RENDER_TARGET] );

#ifdef __cplusplus
}
#endif// __cplusplus

#endif /* include-guard */
