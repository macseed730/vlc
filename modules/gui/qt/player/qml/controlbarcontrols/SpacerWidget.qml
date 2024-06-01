/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Item {
    id: spacer
    enabled: false
    implicitWidth: VLCStyle.icon_toolbar
    implicitHeight: VLCStyle.icon_toolbar
    property alias spacetextExt: spacetext
    property bool paintOnly: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton

        enabled: spacer.enabled || spacer.paintOnly
    }

    T.Label {
        id: spacetext
        text: VLCIcons.space
        color: theme.fg.secondary
        visible: parent.paintOnly

        anchors.centerIn: parent

        font.pixelSize: VLCStyle.icon_toolbar
        font.family: VLCIcons.fontFamily

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
