/**
 * @file events.h
 * @brief X C Bindings VLC module common header
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

#ifndef XCB_CURSOR_NONE
# define XCB_CURSOR_NONE ((xcb_cursor_t) 0U)
#endif

struct vlc_logger;

/* events.c */

/**
 * Checks for an XCB error.
 */
int vlc_xcb_error_Check(struct vlc_logger *, xcb_connection_t *conn,
                        const char *str, xcb_void_cookie_t);

/**
 * Allocates a window for XCB-based output.
 *
 * Creates a VLC video X window object, connects to the corresponding X server,
 * finds the corresponding X server screen.
 */
int vlc_xcb_parent_Create(struct vlc_logger *, const vlc_window_t *wnd,
                          xcb_connection_t **connp,
                          const xcb_screen_t **screenp);
/**
 * Processes XCB events.
 */
int vlc_xcb_Manage(struct vlc_logger *, xcb_connection_t *conn);
