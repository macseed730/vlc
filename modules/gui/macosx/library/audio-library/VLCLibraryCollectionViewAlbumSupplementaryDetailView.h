/*****************************************************************************
 * VLCLibraryCollectionViewAlbumSupplementaryDetailView.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import <Cocoa/Cocoa.h>

#import "library/VLCLibraryCollectionViewSupplementaryDetailView.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCMediaLibraryAlbum;
@class VLCImageView;

extern NSString *const VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier;
extern NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind;

@interface VLCLibraryCollectionViewAlbumSupplementaryDetailView : VLCLibraryCollectionViewSupplementaryDetailView

@property (readwrite, retain, nonatomic) VLCMediaLibraryAlbum *representedAlbum;
@property (readwrite, weak) IBOutlet NSTextField *albumTitleTextField;
@property (readwrite, weak) IBOutlet NSTextField *albumDetailsTextField;
@property (readwrite, weak) IBOutlet NSTextField *albumYearAndDurationTextField;
@property (readwrite, weak) IBOutlet VLCImageView *albumArtworkImageView;
@property (readwrite, weak) IBOutlet NSTableView *albumTracksTableView;
@property (readwrite, weak) IBOutlet NSButton *playAlbumButton;

- (IBAction)playAction:(id)sender;
- (IBAction)enqueueAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
