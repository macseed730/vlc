/*****************************************************************************
 * VLCMediaSourceBaseDataSource.h: MacOS X interface module
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

typedef NS_ENUM(NSInteger, VLCMediaSourceMode) {
    VLCMediaSourceModeLAN,
    VLCMediaSourceModeInternet,
};

NS_ASSUME_NONNULL_BEGIN

@class VLCInputNodePathControl;
@class VLCMediaSourceDataSource;

@interface VLCMediaSourceBaseDataSource : NSObject <NSCollectionViewDataSource,
                                                    NSCollectionViewDelegate,
                                                    NSCollectionViewDelegateFlowLayout,
                                                    NSTableViewDelegate,
                                                    NSTableViewDataSource>

@property (readwrite) NSCollectionView *collectionView;
@property (readwrite) NSScrollView *collectionViewScrollView;
@property (readwrite) NSTableView *tableView;
@property (readwrite) NSSegmentedControl *gridVsListSegmentedControl;
@property (readwrite) NSButton *homeButton;
@property (readwrite) VLCInputNodePathControl *pathControl;
@property (readwrite, nonatomic) VLCMediaSourceMode mediaSourceMode;
@property (readwrite, nonatomic) VLCMediaSourceDataSource *childDataSource;

- (void)setupViews;
- (void)reloadViews;
- (void)homeButtonAction:(id)sender;
- (void)pathControlAction:(id)sender;
- (void)setGridOrListMode:(id)sender;

@end

NS_ASSUME_NONNULL_END
