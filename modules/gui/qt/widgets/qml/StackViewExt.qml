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

/*
 * Custom StackView with brief transitions and helper to load view from the history
 */
StackView {
    id: root

    // Private

    property string _currentView: ""

    // Settings

    replaceEnter: null

    replaceExit: null

    // Events

    onCurrentItemChanged: {
        if (currentItem === null)
            return

        // NOTE: When the currentItem has a padding defined we propagate the StackView values.

        if (currentItem.leftPadding !== undefined)
        {
            currentItem.leftPadding = Qt.binding(function() {
                return leftPadding
            })
        }

        if (currentItem.rightPadding !== undefined)
        {
            currentItem.rightPadding = Qt.binding(function() {
                return rightPadding
            })
        }
    }

    // Functions

    /**
     * viewModel: model with the definition of the available view
     *            elements should contains at least :
     *     name: name of the view
     *     url or component: the url of the Component or the component to load
     * view: string (name of the view to load)
     * viewProperties: map of the propertes to apply to the view
     */
    function loadView(viewModel, view, viewProperties)
    {
        if (root.currentItem && root.currentItem.hasOwnProperty("dismiss"))
            root.currentItem.dismiss()

        if (view === _currentView) {
            if (Object.keys(viewProperties).length === 0 && root.currentItem.hasOwnProperty("loadDefaultView") ) {
                root.currentItem.loadDefaultView()
            } else {
                for ( var viewProp in viewProperties ) {
                    if ( root.currentItem.hasOwnProperty(viewProp) ) {
                        root.currentItem[viewProp] = viewProperties[viewProp]
                    }
                }
            }
            return true
        }

        var found = false
        for (var tab = 0; tab < viewModel.length; tab++ )
        {
            var model = viewModel[tab]
            if (model.name === view) {
                if (model.guard !== undefined && typeof model.guard === "function" && !model.guard(viewProperties)) {
                    continue //we're not allowed to load this page
                }

                //we can't use push(url, properties) as Qt interprets viewProperties
                //as a second component to load
                var component = undefined
                if (model.component) {
                    component = model.component
                } else if ( model.url ) {
                    component = Qt.createComponent(model.url)
                } else {
                    console.warn( "you should define either component or url of the view to load" )
                    return false
                }

                if (component.status === Component.Ready ) {
                    //note doesn't work with qt 5.9
                    root.replace(null, component, viewProperties)
                    found = true
                    break;
                } else {
                    console.warn("component is not ready: " + component.errorString())
                }
            }
        }
        if (!found)
            console.warn("unable to load view " + view)
        else
            _currentView = view
        return found
    }

    function setCurrentItemFocus(reason) {
        currentItem.setCurrentItemFocus(reason);
    }
}
