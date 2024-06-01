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

#ifndef MCVIDEOMODEL_H
#define MCVIDEOMODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "mlbasemodel.hpp"
#include "mlvideo.hpp"

#include <QObject>

class MLVideoModel : public MLBaseModel
{
    Q_OBJECT

public:
    enum Role {
        VIDEO_ID = Qt::UserRole + 1,
        VIDEO_IS_NEW,
        VIDEO_IS_FAVORITE,
        VIDEO_FILENAME,
        VIDEO_TITLE,
        VIDEO_THUMBNAIL,
        VIDEO_DURATION,
        VIDEO_PROGRESS,
        VIDEO_PLAYCOUNT,
        VIDEO_RESOLUTION,
        VIDEO_CHANNEL,
        VIDEO_MRL,
        VIDEO_DISPLAY_MRL,
        VIDEO_VIDEO_TRACK,
        VIDEO_AUDIO_TRACK,

        VIDEO_TITLE_FIRST_SYMBOL,
    };


public:
    explicit MLVideoModel(QObject* parent = nullptr);
    virtual ~MLVideoModel() = default;

    QHash<int, QByteArray> roleNames() const override;

public: // Interface
    // NOTE: These functions are useful when we want to apply a change before the database event.

    Q_INVOKABLE void setItemPlayed(const QModelIndex & index, bool played);

    Q_INVOKABLE void setItemFavorite(const QModelIndex & index, bool played);

protected:
    QVariant itemRoleData(MLItem *item, int role) const override;

    void thumbnailUpdated(const QModelIndex& , MLItem* , const QString& , vlc_ml_thumbnail_status_t ) override;

    std::unique_ptr<MLBaseModel::BaseLoader> createLoader() const override;

protected: // MLBaseModel reimplementation
    virtual void onVlcMlEvent( const MLEvent &event ) override;

private:
    void generateThumbnail(uint64_t id) const;

    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;
    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    static QHash<QByteArray, vlc_ml_sorting_criteria_t> M_names_to_criteria;
    QByteArray criteriaToName(vlc_ml_sorting_criteria_t criteria) const override;

    struct Loader : public BaseLoader
    {
        Loader(const MLVideoModel &model) : BaseLoader(model) {}
        size_t count(vlc_medialibrary_t* ml) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MCVIDEOMODEL_H
