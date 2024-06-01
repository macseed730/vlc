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
import "qrc:///style/"

BusyIndicator {
    id: control

    property color color
    palette.text: color
    running: false

    property int delay: VLCStyle.duration_humanMoment
    property bool runningDelayed: false
    onRunningDelayedChanged: {
        if (runningDelayed) {
            controlDelay.start()
        } else {
            controlDelay.stop()
            control.running = false
        }
    }

    Timer {
        id: controlDelay
        interval: control.delay
        running: false
        repeat: false
        onTriggered: control.running = true
    }
}
