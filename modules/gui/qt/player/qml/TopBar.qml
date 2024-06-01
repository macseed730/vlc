/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus

FocusScope{
    id: root

    // Properties

    /* required */ property int textWidth

    property string title

    property bool showCSD: false
    property bool showToolbar: false
    property bool pinControls: false

    property int topMargin: 0
    property int sideMargin: 0

    property int reservedHeight: 0

    readonly property int _sideMargin: VLCStyle.margin_small + sideMargin

    // Aliases

    property alias resumeVisible: resumeDialog.visible

    // Signals

    signal togglePlaylistVisibility()
    signal requestLockUnlockAutoHide(bool lock)
    signal backRequested()

    Component.onCompleted:  root._layout()

    onShowCSDChanged: root._layout()
    onPinControlsChanged: root._layout()
    onShowToolbarChanged: root._layout()
    onTopMarginChanged: root._layout()
    onSideMarginChanged: root._layout()

    function _layoutLine(c1, c2, offset)
    {
        var c1Height = c1 !== undefined ? c1.implicitHeight : 0
        var c2Height = c2 !== undefined ? c2.implicitHeight : 0

        if (c2 === csdDecorations) {
            //csdDecorations.implicitHeight gets overwritten when the height is set,
            //VLCStyle.icon_normal is its initial value
            c2Height = VLCStyle.icon_normal
        }

        var lineHeight = Math.max(c1Height, c2Height)

        if (c1) {
            c1.height = lineHeight
            c1.anchors.topMargin = offset
        }

        if (c2) {
            c2.height = lineHeight
            c2.anchors.topMargin = offset
        }
        return lineHeight
    }

    //FIXME: if CSD will be weirdly placed if application safe-area are used,
    //nota that if you need a safe area (kiosk mode), you probably don't need CSD
    function _layout() {
        var offset = root.topMargin

        if (root.pinControls && !root.showToolbar && root.showCSD) {
            //place everything on one line
            //csdDecorations.implicitHeight gets overwritten when the height is set,
            //VLCStyle.icon_normal is its initial value
            var lineHeight = Math.max(logoOrResume.implicitHeight, playlistGroup.implicitHeight, VLCStyle.icon_normal)

            centerTitleText.y = 0
            centerTitleText.height = lineHeight

            csdDecorations.height  = lineHeight

            logoOrResume.height =  lineHeight

            playlistGroup.height = lineHeight
            playlistGroup.anchors.topMargin = 0
            playlistGroup.extraRightMargin = Qt.binding(function() { return root.width - csdDecorations.x })


            root.implicitHeight = lineHeight
            offset += lineHeight

        } else {
            playlistGroup.extraRightMargin = 0

            var left = undefined
            var right = undefined
            var logoPlaced = false

            if (root.showToolbar) {
                left = menubar
            }

            if (root.showCSD) {
                right = csdDecorations
                if (!left) {
                    left = logoOrResume
                    logoPlaced = true
                }
            }

            if (!!left || !!right) {
                offset += root._layoutLine(left, right, offset)

                if (root.showCSD) {
                    tapNDrag.height = offset
                }
            }

            if (!logoPlaced) {
                left = logoOrResume
            } else {
                left = undefined
            }

            right = playlistGroup

            var secondLineOffset = offset
            var secondLineHeight = root._layoutLine(left, right, offset)

            offset += secondLineHeight

            if (root.pinControls) {
                centerTitleText.y = secondLineOffset
                centerTitleText.height = secondLineHeight
            }

        }

        root.implicitHeight = offset
        reservedHeight = offset
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    //drag and dbl click the titlebar in CSD mode
    Loader {
        id: tapNDrag

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        active: root.showCSD
        source: "qrc:///widgets/CSDTitlebarTapNDrapHandler.qml"
    }

    // Components -
    Menus.Menubar {
        id: menubar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: root._sideMargin

        width: implicitWidth

        visible: root.showToolbar
        enabled: root.showToolbar

        onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
        onMenuOpenedChanged: root.requestLockUnlockAutoHide(menuOpened)
    }


    Item {
        id: logoOrResume

        anchors.left: root.left
        anchors.top: root.top
        anchors.leftMargin:  root._sideMargin

        implicitWidth: resumeVisible ? resumeDialog.implicitWidth
                                     : logoGroup.implicitWidth

        implicitHeight: resumeVisible ? resumeDialog.implicitHeight
                                      : logoGroup.implicitHeight

        onImplicitHeightChanged: root._layout()

        Item {
            id: logoGroup

            anchors.fill: parent
            visible: !resumeVisible

            implicitHeight: VLCStyle.icon_banner + VLCStyle.margin_xxsmall * 2
            implicitWidth: backBtn.implicitWidth + logo.implicitWidth + VLCStyle.margin_xxsmall

            Widgets.IconControlButton {
                id: backBtn

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left

                objectName: "IconToolButton"
                size: VLCStyle.icon_banner
                iconText: VLCIcons.back
                text: I18n.qtr("Back")
                focus: true

                Navigation.parentItem: root
                Navigation.rightItem: menuSelector
                onClicked: root.backRequested()

                onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
            }

            Widgets.BannerCone {
                id: logo

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: backBtn.right
                anchors.leftMargin: VLCStyle.margin_xxsmall

                color: theme.accent

                Connections {
                    target: logo.button

                    onSystemMenuVisibilityChanged: {
                        root.requestLockUnlockAutoHide(visible)
                    }
                }
            }
        }


        ResumeDialog {
            id: resumeDialog

            anchors.fill: parent
            //add aditional margin so it align with menubar text when visible (see MenuBar.qml)
            anchors.leftMargin: VLCStyle.margin_xsmall

            maxWidth: ((root.showCSD && !root.pinControls) ? csdDecorations : playlistGroup).x
                - VLCStyle.applicationHorizontalMargin
                - VLCStyle.margin_large

            colorContext.palette: theme.palette

            Navigation.parentItem: rootPlayer

            onHidden: {
                if (activeFocus) {
                    backBtn.forceActiveFocus()
                }
            }

            onVisibleChanged: {
                root.requestLockUnlockAutoHide(visible)
            }
        }
    }

    //FIXME use the the right class
    T.Label {
        id: centerTitleText

        readonly property int _leftLimit: logoOrResume.x + logoOrResume.width
        readonly property int _rightLimit: playlistGroup.x
        readonly property int _availableWidth: _rightLimit - _leftLimit
        readonly property int _centerX: ((root.width - centerTitleText.implicitWidth) / 2)
        readonly property bool _alignHCenter: _centerX > _leftLimit
                                              && _centerX + centerTitleText.implicitWidth < _rightLimit

        visible: root.pinControls && !resumeVisible

        width: Math.min(centerTitleText._availableWidth, centerTitleText.implicitWidth)

        leftPadding: VLCStyle.margin_small
        rightPadding: VLCStyle.margin_small

        text: root.title
        color: theme.fg.primary
        font.pixelSize: VLCStyle.dp(13, VLCStyle.scale)
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter

        on_AlignHCenterChanged: _layout()
        Component.onCompleted: _layout()

        function _layout() {
            if (_alignHCenter) {
                centerTitleText.x = 0
                centerTitleText.anchors.horizontalCenter = root.horizontalCenter
            } else {
                centerTitleText.anchors.horizontalCenter = undefined
                centerTitleText.x = Qt.binding(function() { return centerTitleText._leftLimit })
            }
        }
    }

    //FIXME use the the right class
    T.Label {
        id: leftTitleText

        anchors.left: parent.left
        anchors.top: logoOrResume.bottom
        anchors.leftMargin: root._sideMargin

        width: root.textWidth - VLCStyle.margin_normal

        visible: !root.pinControls

        topPadding: VLCStyle.margin_large
        leftPadding: logo.x

        text: root.title
        horizontalAlignment: Text.AlignLeft
        color: theme.fg.primary
        font.weight: Font.DemiBold
        font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
        elide: Text.ElideRight
    }

    Loader {
        id: csdDecorations

        anchors.top: parent.top
        anchors.right: parent.right

        focus: false
        height: VLCStyle.icon_normal
        active: root.showCSD
        enabled: root.showCSD
        visible: root.showCSD
        source:  VLCStyle.palette.hasCSDImage
            ? "qrc:///widgets/CSDThemeButtonSet.qml"
            : "qrc:///widgets/CSDWindowButtonSet.qml"

        Connections {
            target: csdDecorations.item
            enabled: csdDecorations.loaded
            onHoveredChanged: root.requestLockUnlockAutoHide(csdDecorations.item.hovered)
        }
    }

    Row {
        id: playlistGroup

        property int extraRightMargin: 0

        focus: true
        spacing: VLCStyle.margin_xxsmall

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: root._sideMargin + extraRightMargin

        Widgets.IconControlButton {
            id: menuSelector

            visible: !root.showToolbar
            enabled: visible
            focus: visible
            size: VLCStyle.icon_banner

            width: VLCStyle.bannerButton_width
            height: VLCStyle.bannerButton_height

            iconText: VLCIcons.menu
            text: I18n.qtr("Menu")

            Navigation.parentItem: root
            Navigation.leftItem: backBtn
            Navigation.rightItem: playlistButton

            onClicked: contextMenu.popup(this.mapToGlobal(0, height))

            onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

            QmlGlobalMenu {
                id: contextMenu

                ctx: MainCtx

                onAboutToShow: root.requestLockUnlockAutoHide(true)
                onAboutToHide: root.requestLockUnlockAutoHide(false)
            }
        }

        Widgets.IconControlButton {
            id: playlistButton

            objectName: ControlListModel.PLAYLIST_BUTTON
            size: VLCStyle.icon_banner
            iconText: VLCIcons.playlist
            text: I18n.qtr("Playlist")
            focus: root.showToolbar

            checked: MainCtx.playlistVisible

            width: VLCStyle.bannerButton_width
            height: VLCStyle.bannerButton_height

            Navigation.parentItem: root
            Navigation.leftItem: menuSelector.visible ? menuSelector : backBtn
            onClicked: togglePlaylistVisibility()

            onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
        }
    }
}
