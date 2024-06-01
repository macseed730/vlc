/*****************************************************************************
 * VLCVideoFilterHelper.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2006-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Foundation/Foundation.h>
#import <vlc_common.h>

@interface VLCVideoFilterHelper : NSObject

+ (void)setVideoFilter: (const char *)psz_name on:(BOOL)b_on;
+ (void)setVideoFilterProperty: (const char *)psz_property forFilter: (const char *)psz_filter withValue: (vlc_value_t)value;

@end
