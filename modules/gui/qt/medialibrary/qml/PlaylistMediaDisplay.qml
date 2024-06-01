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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQml.Models 2.2

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///util/" as Util
import "qrc:///style/"

FocusScope {
    id: root

    // Properties

    readonly property bool isViewMultiView: false

    property int leftPadding: 0
    property int rightPadding: 0

    readonly property int currentIndex: view.currentIndex
    property string name: ""

    property int initialIndex: 0

    property alias header: view.header
    property Item headerItem: view.headerItem

    property bool isMusic: true
    property string _placeHolder: isMusic ? VLCStyle.noArtAlbumCover : VLCStyle.noArtVideoCover


    // Aliases

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: model.parentId

    property alias model: model

    property alias dragItem: dragItem

    // Events

    onModelChanged: resetFocus()

    onInitialIndexChanged: resetFocus()

    // Functions

    function setCurrentItemFocus(reason) { view.setCurrentItemFocus(reason); }

    function resetFocus() {
        if (model.count === 0) return

        var initialIndex = root.initialIndex

        if (initialIndex >= model.count)
            initialIndex = 0

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        view.positionViewAtIndex(initialIndex, ItemView.Contain);
    }

    // Events

    function onDelete()
    {
        var indexes = modelSelect.selectedIndexes;

        if (indexes.length === 0)
            return;

        model.remove(indexes);
    }

    // Childs

    MLPlaylistModel {
        id: model

        ml: MediaLib

        onCountChanged: {
            // NOTE: We need to cancel the Drag item manually when resetting. Should this be called
            //       from 'onModelReset' only ?
            dragItem.Drag.cancel();

            if (count === 0 || modelSelect.hasSelection)
                return;

            resetFocus();
        }
    }

    Widgets.MLDragItem {
        id: dragItem

        mlModel: model

        indexes: modelSelect.selectedIndexes

        coverRole: "thumbnail"

        defaultCover: root._placeHolder
    }

    Util.SelectableDelegateModel {
        id: modelSelect

        model: root.model
    }

    PlaylistMediaContextMenu {
        id: contextMenu

        model: root.model
    }

    PlaylistMedia
    {
        id: view

        // Settings

        anchors.fill: parent

        clip: true

        focus: (model.count !== 0)

        model: root.model

        selectionDelegateModel: modelSelect

        dragItem: root.dragItem

        header: Widgets.SubtitleLabel {
            id: label

            leftPadding: view.contentLeftMargin
            rightPadding: view.contentRightMargin
            topPadding: VLCStyle.margin_normal
            bottomPadding: VLCStyle.margin_xsmall

            text: root.name
            color: view.colorContext.fg.primary
        }


        headerTopPadding: VLCStyle.margin_normal

        Navigation.parentItem: root

        Navigation.cancelAction: function () {
            if (view.currentIndex <= 0) {
                root.Navigation.defaultNavigationCancel()
            } else {
                view.currentIndex = 0;
                view.positionViewAtIndex(0, ItemView.Contain);
            }
        }

        // Events

        onContextMenuButtonClicked: contextMenu.popup(modelSelect.selectedIndexes,
                                                      globalMousePos)

        onRightClick: contextMenu.popup(modelSelect.selectedIndexes, globalMousePos)

        // Keys

        Keys.onDeletePressed: onDelete()
    }

    EmptyLabelButton {
        anchors.fill: parent

        visible: model.isReady && (model.count <= 0)

        focus: visible

        text: I18n.qtr("No media found")

        cover: root._placeHolder

        Navigation.parentItem: root
    }
}
