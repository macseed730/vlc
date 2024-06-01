/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import org.videolan.vlc 0.1
import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Item {
    id: root
    width: VLCStyle.dp(320, VLCStyle.scale)
    height: VLCStyle.dp(180, VLCStyle.scale)

    //VideoSurface x,y won't update
    onXChanged: videoSurface.onSurfacePositionChanged()
    onYChanged: videoSurface.onSurfacePositionChanged()

    property real dragXMin: 0
    property real dragXMax: 0
    property real dragYMin: undefined
    property real dragYMax: undefined

    Connections {
        target: mouseArea.drag
        onActiveChanged: {
            root.anchors.left = undefined;
            root.anchors.right = undefined
            root.anchors.top = undefined
            root.anchors.bottom = undefined
            root.anchors.verticalCenter = undefined;
            root.anchors.horizontalCenter = undefined
        }
    }
    Drag.active: mouseArea.drag.active

    VideoSurface {
        id: videoSurface

        anchors.fill: parent

        enabled: root.enabled
        visible: root.visible

        ctx: MainCtx
    }

    MouseArea {
        id: mouseArea

        anchors.fill: videoSurface
        z: 1

        hoverEnabled: true
        onClicked: mainPlaylistController.togglePlayPause()

        enabled: root.enabled
        visible: root.visible

        cursorShape: drag.active ? Qt.DragMoveCursor : undefined
        drag.target: root
        drag.minimumX: root.dragXMin
        drag.minimumY: root.dragYMin
        drag.maximumX: root.dragXMax
        drag.maximumY: root.dragYMax

        onWheel: wheel.accept()

        Rectangle {
            color: "#10000000"
            anchors.fill: parent
            visible: parent.containsMouse

            Widgets.IconButton {
                anchors.centerIn: parent

                font.pixelSize: VLCStyle.icon_large
                text: (Player.playingState !== Player.PLAYING_STATE_PAUSED
                       && Player.playingState !== Player.PLAYING_STATE_STOPPED)
                      ? VLCIcons.pause
                      : VLCIcons.play

                onClicked: mainPlaylistController.togglePlayPause()
            }

            Widgets.IconButton {
                anchors {
                    top: parent.top
                    topMargin: VLCStyle.margin_small
                    right: parent.right
                    rightMargin: VLCStyle.margin_small
                }

                font.pixelSize: VLCStyle.icon_PIP
                text: VLCIcons.close

                onClicked: mainPlaylistController.stop()
            }


            Widgets.IconButton {
                anchors {
                    top: parent.top
                    topMargin: VLCStyle.margin_small
                    left: parent.left
                    leftMargin: VLCStyle.margin_small
                }

                font.pixelSize: VLCStyle.icon_PIP
                text: VLCIcons.fullscreen

                onClicked: History.push(["player"])
            }
        }
    }
}
