/*****************************************************************************
 * VLCMediaSourceDataSource.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCInputItem;
@class VLCInputNode;
@class VLCInputNodePathControl;
@class VLCMediaSource;

@interface VLCMediaSourceDataSource : NSObject <NSCollectionViewDataSource,
                                                NSCollectionViewDelegate,
                                                NSCollectionViewDelegateFlowLayout,
                                                NSTableViewDelegate,
                                                NSTableViewDataSource>

@property (readwrite, retain) VLCMediaSource *displayedMediaSource;
@property (readwrite, retain, nonatomic) VLCInputNode *nodeToDisplay;
@property (readwrite, assign) NSCollectionView *collectionView;
@property (readwrite, assign) NSTableView *tableView;
@property (readwrite) VLCInputNodePathControl *pathControl;
@property (readwrite) BOOL gridViewMode;

- (void)setupViews;
- (VLCInputItem*)mediaSourceInputItemAtRow:(NSInteger)tableViewRow;

@end

NS_ASSUME_NONNULL_END
