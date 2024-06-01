/*****************************************************************************
 * NSImage+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "NSImage+VLCAdditions.h"

#import <QuickLook/QuickLook.h>

@implementation NSImage(VLCAdditions)

+ (instancetype)quickLookPreviewForLocalPath:(NSString *)path withSize:(NSSize)size
{
    NSURL *pathUrl = [NSURL fileURLWithPath:path];
    return [self quickLookPreviewForLocalURL:pathUrl withSize:size];
}

+ (instancetype)quickLookPreviewForLocalURL:(NSURL *)url withSize:(NSSize)size
{
    NSDictionary *dict = @{(NSString*)kQLThumbnailOptionIconModeKey : [NSNumber numberWithBool:NO]};
    CFDictionaryRef dictRef = CFBridgingRetain(dict);
    if (dictRef == NULL) {
        NSLog(@"Got null dict for quickLook preview");
        return nil;
    }

    CFURLRef urlRef = CFBridgingRetain(url);
    if (urlRef == NULL) {
        NSLog(@"Got null url ref for quickLook preview");
        CFRelease(dictRef);
        return nil;
    }

    CGImageRef qlThumbnailRef = QLThumbnailImageCreate(kCFAllocatorDefault,
                                                       urlRef,
                                                       size,
                                                       dictRef);

    CFRelease(dictRef);
    CFRelease(urlRef);

    if (qlThumbnailRef == NULL) {
        return nil;
    }

    NSBitmapImageRep *bitmapImageRep = [[NSBitmapImageRep alloc] initWithCGImage:qlThumbnailRef];
    if (bitmapImageRep == nil) {
        CFRelease(qlThumbnailRef);
        return nil;
    }

    NSImage *image = [[NSImage alloc] initWithSize:[bitmapImageRep size]];
    [image addRepresentation:bitmapImageRep];
    CFRelease(qlThumbnailRef);
    return image;
}

@end
