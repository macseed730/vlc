/*****************************************************************************
 * VLCMain.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2020 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import "main/VLCMain.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <stdatomic.h>
#include <sys/sysctl.h>

#include <vlc_common.h>
#include <vlc_actions.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_variables.h>

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowController.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain+OldPrefs.h"
#import "main/VLCApplication.h"

#import "extensions/NSString+Helpers.h"

#import "menus/VLCMainMenu.h"
#import "menus/VLCStatusBarIcon.h"

#import "os-integration/VLCClickerManager.h"

#import "panels/dialogs/VLCCoreDialogProvider.h"
#import "panels/VLCAudioEffectsWindowController.h"
#import "panels/VLCBookmarksWindowController.h"
#import "panels/VLCVideoEffectsWindowController.h"
#import "panels/VLCTrackSynchronizationWindowController.h"

#import "library/VLCLibraryController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistModel.h"
#import "playlist/VLCPlaybackContinuityController.h"

#import "preferences/prefs.h"
#import "preferences/VLCSimplePrefsController.h"

#import "windows/extensions/VLCExtensionsManager.h"
#import "windows/logging/VLCLogWindowController.h"
#import "windows/convertandsave/VLCConvertAndSaveWindowController.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/VLCOpenInputMetadata.h"
#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"

#ifdef HAVE_SPARKLE
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */
NSString *const kIntel64UpdateURLString = @"https://update.videolan.org/vlc/sparkle/vlc-intel64.xml";
NSString *const kARM64UpdateURLString = @"https://update.videolan.org/vlc/sparkle/vlc-arm64.xml";
#endif

NSString *VLCConfigurationChangedNotification = @"VLCConfigurationChangedNotification";

#pragma mark -
#pragma mark Private extension

@interface VLCMain ()
#ifdef HAVE_SPARKLE
<SUUpdaterDelegate, NSApplicationDelegate>
#else
<NSApplicationDelegate>
#endif
{
    intf_thread_t *_p_intf;
    BOOL _launched;

    VLCMainMenu *_mainmenu;
    VLCPrefs *_prefs;
    VLCSimplePrefsController *_sprefs;
    VLCOpenWindowController *_open;
    VLCCoreDialogProvider *_coredialogs;
    VLCBookmarksWindowController *_bookmarks;
    VLCPlaybackContinuityController *_continuityController;
    VLCLogWindowController *_messagePanelController;
    VLCStatusBarIcon *_statusBarIcon;
    VLCTrackSynchronizationWindowController *_trackSyncPanel;
    VLCAudioEffectsWindowController *_audioEffectsPanel;
    VLCVideoEffectsWindowController *_videoEffectsPanel;
    VLCConvertAndSaveWindowController *_convertAndSaveWindow;
    VLCClickerManager *_clickerManager;

    bool _interfaceIsTerminating; /* Makes sure applicationWillTerminate will be called only once */
}
+ (void)killInstance;
- (void)applicationWillTerminate:(NSNotification *)notification;

@end

#pragma mark -
#pragma mark VLC Interface Object Callbacks

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/

static intf_thread_t *p_interface_thread;

intf_thread_t *getIntf()
{
    return p_interface_thread;
}

int OpenIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        p_interface_thread = p_intf;
        msg_Dbg(p_intf, "Starting macosx interface");

        @try {
            [VLCApplication sharedApplication];
            [VLCMain sharedInstance];

            msg_Dbg(p_intf, "Finished loading macosx interface");
            return VLC_SUCCESS;
        } @catch (NSException *exception) {
            msg_Err(p_intf, "Loading the macosx interface failed. Do you have a valid window server?");
            return VLC_EGENERIC;
        }
    }
}

void CloseIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        msg_Dbg(p_this, "Closing macosx interface");
        [[VLCMain sharedInstance] applicationWillTerminate:nil];
        [VLCMain killInstance];
    }

    p_interface_thread = nil;
}

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

#pragma mark -
#pragma mark Initialization

static VLCMain *sharedInstance = nil;

+ (VLCMain *)sharedInstance;
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        sharedInstance = [[VLCMain alloc] init];
    });

    return sharedInstance;
}

+ (void)killInstance
{
    sharedInstance = nil;
}

+ (void)relaunchApplication
{
    const char *path = [[[NSBundle mainBundle] executablePath] UTF8String];

    /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
    if (fork() != 0) {
        exit(0);
    }
    execl(path, path, (char *)NULL);
}

- (id)init
{
    self = [super init];
    if (self) {
        _p_intf = getIntf();

        [VLCApplication sharedApplication].delegate = self;

        _playlistController = [[VLCPlaylistController alloc] initWithPlaylist:vlc_intf_GetMainPlaylist(_p_intf)];
        _libraryController = [[VLCLibraryController alloc] init];
        _continuityController = [[VLCPlaybackContinuityController alloc] init];

        // first initialize extensions dialog provider, then core dialog
        // provider which will register both at the core
        _extensionsManager = [[VLCExtensionsManager alloc] init];
        _coredialogs = [[VLCCoreDialogProvider alloc] init];

        _mainmenu = [[VLCMainMenu alloc] init];
        _voutProvider = [[VLCVideoOutputProvider alloc] init];

        // Load them here already to apply stored profiles
        _videoEffectsPanel = [[VLCVideoEffectsWindowController alloc] init];
        _audioEffectsPanel = [[VLCAudioEffectsWindowController alloc] init];

        if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
            [_playlistController.playerController setFullscreen:YES];
    }

    return self;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    _clickerManager = [[VLCClickerManager alloc] init];

    [[NSBundle mainBundle] loadNibNamed:@"MainMenu" owner:_mainmenu topLevelObjects:nil];

#ifdef HAVE_SPARKLE
    [[SUUpdater sharedUpdater] setDelegate:self];
#endif

    NSImage *appIconImage = [[VLCApplication sharedApplication] vlcAppIconImage];
    [[VLCApplication sharedApplication]
        setApplicationIconImage:appIconImage];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    _launched = YES;

    if (_libraryWindowController == nil) {
        _libraryWindowController = [[VLCLibraryWindowController alloc] initWithLibraryWindow];
    }
    
    [_libraryWindowController.window makeKeyAndOrderFront:nil];

    if (!_p_intf)
        return;

    [self migrateOldPreferences];

    _statusBarIcon = [[VLCStatusBarIcon alloc] init];

    /* on macOS 11 and later, check whether the user attempts to deploy
     * the x86_64 binary on ARM-64 - if yes, log it */
    if (OSX_BIGSUR_AND_HIGHER) {
        if ([self processIsTranslated] > 0) {
            msg_Warn(getIntf(), "Process is translated!");
        }
    }
}

- (int)processIsTranslated
{
   int ret = 0;
   size_t size = sizeof(ret);
   if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
      if (errno == ENOENT)
         return 0;
      return -1;
   }
   return ret;
}

#pragma mark -
#pragma mark Termination

- (BOOL)isTerminating
{
    return _interfaceIsTerminating;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    if (_interfaceIsTerminating)
        return;
    _interfaceIsTerminating = true;

    NSNotificationCenter *notiticationCenter = [NSNotificationCenter defaultCenter];
    if (notification == nil) {
        [notiticationCenter postNotificationName: NSApplicationWillTerminateNotification object: nil];
    }
    [notiticationCenter removeObserver: self];

    // closes all open vouts
    _voutProvider = nil;
    _continuityController = nil;

    /* write cached user defaults to disk */
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

#pragma mark -
#pragma mark Sparkle delegate

#ifdef HAVE_SPARKLE
/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [NSApp activateIgnoringOtherApps:YES];
    [_playlistController stopPlayback];
}

/* don't be enthusiastic about an update if we currently play a video */
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    if ([_playlistController.playerController activeVideoPlayback])
        return NO;

    return YES;
}

/* use the correct feed depending on the hardware architecture */
- (nullable NSString *)feedURLStringForUpdater:(SUUpdater *)updater
{
#ifdef __x86_64__
    if (OSX_BIGSUR_AND_HIGHER) {
        if ([self processIsTranslated] > 0) {
            msg_Dbg(getIntf(), "Process is translated. On update, VLC will install the native ARM-64 binary.");
            return kARM64UpdateURLString;
        }
    }
    return kIntel64UpdateURLString;
#elif __arm64__
    return kARM64UpdateURLString;
#else
    #error unsupported architecture
#endif
}

- (void)updaterDidNotFindUpdate:(SUUpdater *)updater
{
    msg_Dbg(getIntf(), "No update found");
}

- (void)updater:(SUUpdater *)updater failedToDownloadUpdate:(SUAppcastItem *)item error:(NSError *)error
{
    msg_Warn(getIntf(), "Failed to download update with error %li", error.code);
}

- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error
{
    msg_Err(getIntf(), "Updater aborted with error %li", error.code);
}
#endif

#pragma mark -
#pragma mark File opening over dock icon

- (void)application:(NSApplication *)o_app openFiles:(NSArray *)o_names
{
    // Only add items here which are getting dropped to to the application icon
    // or are given at startup. If a file is passed via command line, libvlccore
    // will add the item, but cocoa also calls this function. In this case, the
    // invocation is ignored here.
    NSArray *resultItems = o_names;
    if (_launched == NO) {
        NSArray *launchArgs = [[NSProcessInfo processInfo] arguments];

        if (launchArgs) {
            NSSet *launchArgsSet = [NSSet setWithArray:launchArgs];
            NSMutableSet *itemSet = [NSMutableSet setWithArray:o_names];
            [itemSet minusSet:launchArgsSet];
            resultItems = [itemSet allObjects];
        }
    }

    NSArray *o_sorted_names = [resultItems sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity: [o_sorted_names count]];
    for (NSString *filepath in o_sorted_names) {
        VLCOpenInputMetadata *inputMetadata;

        inputMetadata = [VLCOpenInputMetadata inputMetaWithPath:filepath];
        if (!inputMetadata)
            continue;

        [o_result addObject:inputMetadata];
    }

    [_playlistController addPlaylistItems:o_result];
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if (!hasVisibleWindows)
        [[self libraryWindow] makeKeyAndOrderFront:self];

    return YES;
}

#pragma mark -
#pragma mark Other objects getters

- (VLCMainMenu *)mainMenu
{
    return _mainmenu;
}

- (VLCLibraryWindow *)libraryWindow
{
    return (VLCLibraryWindow *)_libraryWindowController.window;
}

- (VLCLogWindowController *)debugMsgPanel
{
    if (!_messagePanelController)
        _messagePanelController = [[VLCLogWindowController alloc] init];

    return _messagePanelController;
}

- (VLCTrackSynchronizationWindowController *)trackSyncPanel
{
    if (!_trackSyncPanel)
        _trackSyncPanel = [[VLCTrackSynchronizationWindowController alloc] init];

    return _trackSyncPanel;
}

- (VLCAudioEffectsWindowController *)audioEffectsPanel
{
    return _audioEffectsPanel;
}

- (VLCVideoEffectsWindowController *)videoEffectsPanel
{
    return _videoEffectsPanel;
}

- (VLCBookmarksWindowController *)bookmarks
{
    if (!_bookmarks)
        _bookmarks = [[VLCBookmarksWindowController alloc] init];

    return _bookmarks;
}

- (VLCOpenWindowController *)open
{
    if (!_open)
        _open = [[VLCOpenWindowController alloc] init];

    return _open;
}

- (VLCConvertAndSaveWindowController *)convertAndSaveWindow
{
    if (_convertAndSaveWindow == nil)
        _convertAndSaveWindow = [[VLCConvertAndSaveWindowController alloc] init];

    return _convertAndSaveWindow;
}

- (VLCSimplePrefsController *)simplePreferences
{
    if (!_sprefs)
        _sprefs = [[VLCSimplePrefsController alloc] init];

    return _sprefs;
}

- (VLCPrefs *)preferences
{
    if (!_prefs)
        _prefs = [[VLCPrefs alloc] init];

    return _prefs;
}

- (VLCCoreDialogProvider *)coreDialogProvider
{
    return _coredialogs;
}

@end
