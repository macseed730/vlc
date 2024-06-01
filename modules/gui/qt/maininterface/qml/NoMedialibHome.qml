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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {

    ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Column {
        anchors.centerIn: parent

        spacing: VLCStyle.margin_small

        Widgets.IconLabel {
            text: VLCIcons.dropzone
            color: theme.fg.secondary
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: VLCStyle.dp(100, VLCStyle.scale)
        }

        Widgets.MenuLabel {
            anchors .horizontalCenter: parent.horizontalCenter
            text: I18n.qtr("Drop some content here")
            color: theme.fg.secondary
        }

        Widgets.ActionButtonPrimary {
            id: openFileButton
            text: I18n.qtr("Open File")
            focus: true
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: DialogsProvider.simpleOpenDialog()
        }
    }
}
