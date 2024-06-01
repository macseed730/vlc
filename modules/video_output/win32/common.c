/*****************************************************************************
 * common.c: Windows video output common code
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
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
 * Preamble: This file contains the functions related to the init of the vout
 *           structure, the common display code, the screensaver, but not the
 *           events and the Window Creation (events.c)
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <assert.h>

#include "events.h"
#include "common.h"
#include "../../video_chroma/copy.h"

void CommonInit(display_win32_area_t *area)
{
    area->place_changed = false;
}

#ifndef VLC_WINSTORE_APP
/* */
int CommonWindowInit(vout_display_t *vd, display_win32_area_t *area,
                     vout_display_sys_win32_t *sys, bool projection_gestures)
{
    if (unlikely(vd->cfg->window == NULL))
        return VLC_EGENERIC;

    /* */
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif
    sys->hvideownd = NULL;
    sys->hparent   = NULL;

    /* */
    sys->event = EventThreadCreate(VLC_OBJECT(vd), vd->cfg->window);
    if (!sys->event)
        return VLC_EGENERIC;

    /* */
    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.width  = vd->cfg->display.width;
    cfg.height = vd->cfg->display.height;
    cfg.is_projected = projection_gestures;

    event_hwnd_t hwnd;
    if (EventThreadStart(sys->event, &hwnd, &cfg))
        return VLC_EGENERIC;

    sys->hparent       = hwnd.hparent;
    sys->hvideownd     = hwnd.hvideownd;

    CommonPlacePicture(vd, area);

    return VLC_SUCCESS;
}
#endif /* !VLC_WINSTORE_APP */

/*****************************************************************************
* UpdateRects: update clipping rectangles
*****************************************************************************
* This function is called when the window position or size are changed, and
* its job is to update the source and destination RECTs used to display the
* picture.
*****************************************************************************/
void CommonPlacePicture(vout_display_t *vd, display_win32_area_t *area)
{
    /* Update the window position and size */
    vout_display_place_t before_place = area->place;
    vout_display_PlacePicture(&area->place, vd->source, &vd->cfg->display);

    /* Signal the change in size/position */
    if (!vout_display_PlaceEquals(&before_place, &area->place))
    {
        area->place_changed |= true;

#ifndef NDEBUG
        msg_Dbg(vd, "UpdateRects source offset: %i,%i visible: %ix%i decoded: %ix%i",
            vd->source->i_x_offset, vd->source->i_y_offset,
            vd->source->i_visible_width, vd->source->i_visible_height,
            vd->source->i_width, vd->source->i_height);
        msg_Dbg(vd, "UpdateRects image_dst coords: %i,%i %ix%i",
            area->place.x, area->place.y, area->place.width, area->place.height);
#endif
    }
}

#ifndef VLC_WINSTORE_APP
/* */
void CommonWindowClean(vout_display_sys_win32_t *sys)
{
    if (sys->event) {
        EventThreadStop(sys->event);
        EventThreadDestroy(sys->event);
    }
}
#endif /* !VLC_WINSTORE_APP */

void CommonControl(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys, int query)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
#ifndef VLC_WINSTORE_APP
        // Update dimensions
        if (sys->event != NULL)
        {
            RECT clientRect;
            GetClientRect(sys->hparent, &clientRect);

            SetWindowPos(sys->hvideownd, 0, 0, 0,
                         RECTWidth(clientRect),
                         RECTHeight(clientRect), SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
        }
#endif /* !VLC_WINSTORE_APP */
        // fallthrough
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        CommonPlacePicture(vd, area);
        break;

    default:
        vlc_assert_unreachable();
    }
}
