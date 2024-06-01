/*****************************************************************************
 * d3d11_filters.h : D3D11 filters module callbacks
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
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

#ifndef VLC_D3D11_FILTERS_H
#define VLC_D3D11_FILTERS_H

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include "../../video_chroma/d3d11_fmt.h"

#ifdef __cplusplus
extern "C" {
#endif

int  D3D11OpenDeinterlace(filter_t *);
int  D3D11OpenConverter(filter_t *);
int  D3D11OpenCPUConverter(filter_t *);
int  D3D11OpenBlockDecoder(vlc_object_t *);
void D3D11CloseBlockDecoder(vlc_object_t *);

int  D3D11OpenDecoderDeviceW8(vlc_decoder_device *, vlc_window_t *);
int  D3D11OpenDecoderDeviceAny(vlc_decoder_device *, vlc_window_t *);

#ifdef __cplusplus
}
#endif

#endif /* VLC_D3D11_FILTERS_H */
