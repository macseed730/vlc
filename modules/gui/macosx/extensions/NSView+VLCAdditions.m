/*****************************************************************************
 * NSView+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2013-2019 VLC authors and VideoLAN
 *
 * Authors: David Fuhrmann <dfuhrmann # videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "NSView+VLCAdditions.h"

#import "main/CompatibilityFixes.h"

@implementation NSView (VLCAdditions)

+ (instancetype)fromNibNamed:(NSString *)nibName withClass:(Class)viewClass withOwner:(id)owner
{
    /* the following code saves us an instance of NSViewController which we don't need */
    NSNib *nib = [[NSNib alloc] initWithNibNamed:nibName bundle:nil];
    NSArray *topLevelObjects;
    if (![nib instantiateWithOwner:owner topLevelObjects:&topLevelObjects]) {
        NSAssert(1, @"Failed to load nib file to show view");
        return nil;
    }

    for (id topLevelObject in topLevelObjects) {
        if ([topLevelObject isKindOfClass:viewClass]) {
            return topLevelObject;
            break;
        }
    }

    return nil;
}

- (BOOL)shouldShowDarkAppearance
{
    if (@available(macOS 10.14, *)) {
        return [self.effectiveAppearance.name isEqualToString:NSAppearanceNameDarkAqua] ||
               [self.effectiveAppearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    return NO;
}

- (void)enableSubviews:(BOOL)enabled
{
    for (NSView *view in [self subviews]) {
        [view enableSubviews:enabled];

        // enable NSControl
        if ([view respondsToSelector:@selector(setEnabled:)]) {
            [(NSControl *)view setEnabled:enabled];
        }
        // also "enable / disable" text views
        if ([view respondsToSelector:@selector(setTextColor:)]) {
            if (enabled == NO) {
                [(NSTextField *)view setTextColor:[NSColor disabledControlTextColor]];
            } else {
                [(NSTextField *)view setTextColor:[NSColor controlTextColor]];
            }
        }

    }
}

@end
