/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

import QtGraphicalEffects 1.0

Item {
    id: rootItem

    property alias source: blurSource.sourceItem
    property alias screenColor: shaderItem.screenColor
    property alias overlayColor: shaderItem.overlayColor

    function scheduleUpdate() {
        blurSource.scheduleUpdate()
    }

    onSourceChanged: blurSource.scheduleUpdate()

    ShaderEffectSource {
        id: blurSource

        width: 512
        height: 512
        textureSize: Qt.size(512,512)

        visible: false

        live: false
        hideSource: false
        smooth: false
        mipmap: false
    }

    FastBlur {
        id: blur

        source: blurSource

        width: 512
        height: 512

        radius: 50
        visible: false
    }

    ShaderEffectSource {
        id: proxySource
        live: true
        hideSource: false
        sourceItem: blur
        smooth: false
        visible: false
        mipmap: false
    }

    ShaderEffect {
        id: shaderItem

        property var backgroundSource: proxySource
        property color screenColor
        property color overlayColor

        anchors.fill: parent
        visible: true
        blending: false
        cullMode: ShaderEffect.BackFaceCulling

        fragmentShader: "qrc:///player/PlayerBlurredBackground.frag"
    }
}
