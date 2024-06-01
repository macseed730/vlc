/*****************************************************************************
 * VLCLibraryTableCellView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryTableCellView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"

@implementation VLCLibraryTableCellView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryTableCellView*)[NSView fromNibNamed:@"VLCLibraryTableCellView"
                                                withClass:[VLCLibraryTableCellView class]
                                                withOwner:owner];
}

- (void)awakeFromNib
{
    self.singlePrimaryTitleTextField.font = [NSFont VLClibraryLargeCellTitleFont];
    self.primaryTitleTextField.font = [NSFont VLClibraryLargeCellTitleFont];
    self.secondaryTitleTextField.font = [NSFont VLClibraryLargeCellSubtitleFont];
    [self prepareForReuse];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.representedImageView.image = nil;
    self.primaryTitleTextField.hidden = YES;
    self.secondaryTitleTextField.hidden = YES;
    self.singlePrimaryTitleTextField.hidden = YES;
    self.trackingView.viewToHide = nil;
    self.playInstantlyButton.hidden = YES;
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    _representedItem = representedItem;

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playMediaItemInstantly:);
    self.playInstantlyButton.target = self;

    self.representedImageView.image = representedItem.smallArtworkImage;

    if(representedItem.detailString.length > 0) {
        self.primaryTitleTextField.hidden = NO;
        self.primaryTitleTextField.stringValue = representedItem.displayString;
        self.secondaryTitleTextField.hidden = NO;
        self.secondaryTitleTextField.stringValue = representedItem.detailString;
    } else {
        self.singlePrimaryTitleTextField.hidden = NO;
        self.singlePrimaryTitleTextField.stringValue = representedItem.displayString;
    }
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;

    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = _representedInputItem.name;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        NSImage *image = [VLCLibraryImageCache thumbnailForInputItem:self->_representedInputItem];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.representedImageView.image = image;
        });
    });

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.playInstantlyButton.action = @selector(playInputItemInstantly:);
    self.playInstantlyButton.target = self;
}

- (void)setRepresentedVideoLibrarySection:(NSUInteger)section
{
    NSString *sectionString = @"";
    switch(section + 1) { // Group 0 is Invalid, so add one
        case VLCLibraryVideoRecentsGroup:
            sectionString = _NS("Recents");
            break;
        case VLCLibraryVideoLibraryGroup:
            sectionString = _NS("Library");
            break;
        default:
            NSAssert(1, @"Reached unreachable case for video library section");
            break;
    }
    
    self.singlePrimaryTitleTextField.hidden = NO;
    self.singlePrimaryTitleTextField.stringValue = sectionString;
    self.representedImageView.image = [NSImage imageNamed: @"noart.png"];
}

- (void)playMediaItemInstantly:(id)sender
{
    VLCLibraryController *libraryController = VLCMain.sharedInstance.libraryController;

    // We want to add all the tracks to the playlist but only play the first one immediately,
    // otherwise we will skip straight to the last track of the last album from the artist
    __block BOOL playImmediately = YES;
    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [libraryController appendItemToPlaylist:mediaItem playImmediately:playImmediately];

        if(playImmediately) {
            playImmediately = NO;
        }
    }];
}

- (void)playInputItemInstantly:(id)sender
{
    [[[VLCMain sharedInstance] playlistController] addInputItem:_representedInputItem.vlcInputItem atPosition:-1 startPlayback:YES];
}

@end
