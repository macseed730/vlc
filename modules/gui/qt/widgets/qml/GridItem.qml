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
import QtQml.Models 2.2

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

T.Control {
    id: root

    // Properties

    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall
    property Item dragItem: null

    readonly property bool highlighted: (mouseHoverHandler.hovered || visualFocus)

    readonly property int selectedBorderWidth: VLCStyle.gridItemSelectedBorder

    property int _modifiersOnLastPress: Qt.NoModifier

    // if true, texts are horizontally centered, provided it can fit in pictureWidth
    property bool textAlignHCenter: false

    // if the item is selected
    property bool selected: false

    // Aliases

    property alias image: picture.source
    property alias isImageReady: picture.isImageReady
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias playCoverBorderWidth: picture.playCoverBorderWidth
    property alias playCoverShowPlay: picture.playCoverShowPlay
    property alias playIconSize: picture.playIconSize
    property alias pictureRadius: picture.radius
    property alias pictureOverlay: picture.imageOverlay

    property alias selectedShadow: selectedShadow
    property alias unselectedShadow: unselectedShadow

    // Signals

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent, point globalMousePos)

    // Settings

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    Accessible.role: Accessible.Cell
    Accessible.name: title

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

    // States

    states: [
        State {
            name: "highlighted"
            when: highlighted

            PropertyChanges {
                target: selectedShadow
                opacity: 1.0
            }

            PropertyChanges {
                target: unselectedShadow
                opacity: 0
            }

            PropertyChanges {
                target: picture
                playCoverVisible: true
                playCoverOpacity: 1.0
            }

        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "highlighted"
            // reversible: true // doesn't work

            SequentialAnimation {
                PropertyAction {
                    target: picture
                    properties: "playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.InSine
                }
            }
        },

        Transition {
            from: "highlighted"
            to: ""

            SequentialAnimation {
                PropertyAction {
                    target: picture
                    property: "playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.OutSine
                }
            }
        }
    ]

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: root.activeFocus
        hovered: root.hovered
    }

    background: AnimatedBackground {
        width: root.width + (selectedBorderWidth * 2)
        height: root.height + (selectedBorderWidth * 2)

        x: - selectedBorderWidth
        y: - selectedBorderWidth

        active: visualFocus
        animate: theme.initialized

        //don't show the backgroud unless selected
        backgroundColor: root.selected ?  theme.bg.highlight : theme.bg.primary
        activeBorderColor: theme.visualFocus
        visible: animationRunning || active || root.selected || root.hovered
    }

    contentItem: MouseArea {
        implicitWidth: layout.implicitWidth
        implicitHeight: layout.implicitHeight

        acceptedButtons: Qt.RightButton | Qt.LeftButton

        drag.target: root.dragItem

        drag.axis: Drag.XAndYAxis

        onClicked: {
            if (mouse.button === Qt.RightButton)
                contextMenuButtonClicked(picture, root.mapToGlobal(mouse.x,mouse.y));
            else {
                root.itemClicked(picture, mouse.button, mouse.modifiers);
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton)
                root.itemDoubleClicked(picture,mouse.buttons, mouse.modifiers)
        }

        onPressed: _modifiersOnLastPress = mouse.modifiers

        onPositionChanged: {
            if (drag.active) {
                var pos = drag.target.parent.mapFromItem(root, mouseX, mouseY)
                drag.target.x = pos.x + VLCStyle.dragDelta
                drag.target.y = pos.y + VLCStyle.dragDelta
            }
        }

        drag.onActiveChanged: {
            // perform the "click" action because the click action is only executed on mouse release (we are in the pressed state)
            // but we will need the updated list on drop
            if (drag.active && !selected) {
                root.itemClicked(picture, Qt.LeftButton, root._modifiersOnLastPress)
            } else if (root.dragItem) {
                root.dragItem.Drag.drop()
            }
            root.dragItem.Drag.active = drag.active
        }

        TouchScreenTapHandlerCompat {
            onTapped: {
                root.itemClicked(picture, Qt.LeftButton, Qt.NoModifier)
                root.itemDoubleClicked(picture, Qt.LeftButton, Qt.NoModifier)
            }

            onLongPressed: {
                contextMenuButtonClicked(picture, point.scenePosition);
            }
        }

        MouseHoverHandlerCompat {
            id: mouseHoverHandler
        }

        ColumnLayout {
            id: layout

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 0

            Widgets.MediaCover {
                id: picture

                playCoverVisible: false
                playCoverOpacity: 0
                radius: VLCStyle.gridCover_radius
                color: theme.bg.secondary

                Layout.preferredWidth: pictureWidth
                Layout.preferredHeight: pictureHeight

                onPlayIconClicked: {
                    // emulate a mouse click before delivering the play signal as to select the item
                    // this helps in updating the selection and restore of initial index in the parent views
                    root.itemClicked(picture, mouse.button, mouse.modifiers)
                    root.playClicked()
                }

                DoubleShadow {
                    id: unselectedShadow

                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(1) // outside border (unselected)
                    z: -1

                    opacity: 0.62
                    visible: opacity > 0

                    xRadius: parent.radius
                    yRadius: parent.radius

                    primaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                    primaryBlurRadius: VLCStyle.dp(3, VLCStyle.scale)

                    secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                    secondaryBlurRadius: VLCStyle.dp(14, VLCStyle.scale)
                }

                DoubleShadow {
                    id: selectedShadow

                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(1)
                    z: -1

                    visible: opacity > 0
                    opacity: 0

                    xRadius: parent.radius
                    yRadius: parent.radius

                    primaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                    primaryBlurRadius: VLCStyle.dp(18, VLCStyle.scale)

                    secondaryVerticalOffset: VLCStyle.dp(32, VLCStyle.scale)
                    secondaryBlurRadius: VLCStyle.dp(72, VLCStyle.scale)
                }
            }

            Widgets.ScrollingText {
                id: titleTextRect

                label: titleLabel
                forceScroll: highlighted
                visible: root.title !== ""
                clip: scrolling

                Layout.preferredWidth: Math.min(titleLabel.implicitWidth, pictureWidth)
                Layout.preferredHeight: titleLabel.height
                Layout.topMargin: root.titleMargin
                Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft

                Widgets.ListLabel {
                    id: titleLabel

                    height: implicitHeight
                    color: root.selected
                        ? theme.fg.highlight
                        : theme.fg.primary
                    textFormat: Text.PlainText
                }
            }

            Widgets.MenuCaption {
                id: subtitleTxt

                visible: text !== ""
                text: root.subtitle
                elide: Text.ElideRight
                color: root.selected
                    ? theme.fg.highlight
                    : theme.fg.secondary
                textFormat: Text.PlainText

                Layout.preferredWidth: Math.min(pictureWidth, implicitWidth)
                Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft
                Layout.topMargin: VLCStyle.margin_xsmall

                ToolTip.delay: VLCStyle.delayToolTipAppear
                ToolTip.text: subtitleTxt.text
                ToolTip.visible: subtitleTxtMouseHandler.hovered

                MouseHoverHandlerCompat {
                    id: subtitleTxtMouseHandler
                }
            }
        }
    }
}
