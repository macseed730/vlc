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

#include "mlalbumtrackmodel.hpp"

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLAlbumTrackModel::M_names_to_criteria = {
    {"id", VLC_ML_SORTING_DEFAULT},
    {"title", VLC_ML_SORTING_ALPHA},
    {"album_title", VLC_ML_SORTING_ALBUM},
    {"track_number", VLC_ML_SORTING_TRACKNUMBER},
    {"release_year", VLC_ML_SORTING_RELEASEDATE},
    {"main_artist", VLC_ML_SORTING_ARTIST},
    {"duration", VLC_ML_SORTING_DURATION}
};

MLAlbumTrackModel::MLAlbumTrackModel(QObject *parent)
    : MLBaseModel(parent)
{
}

QVariant MLAlbumTrackModel::itemRoleData(MLItem *item, const int role) const
{
    const MLAlbumTrack* ml_track = static_cast<MLAlbumTrack *>(item);
    assert( ml_track );

    switch (role)
    {
    // Tracks
    case TRACK_ID:
        return QVariant::fromValue( ml_track->getId() );
    case TRACK_TITLE :
        return QVariant::fromValue( ml_track->getTitle() );
    case TRACK_COVER :
        return QVariant::fromValue( ml_track->getCover() );
    case TRACK_NUMBER :
        return QVariant::fromValue( ml_track->getTrackNumber() );
    case TRACK_DISC_NUMBER:
        return QVariant::fromValue( ml_track->getDiscNumber() );
    case TRACK_DURATION :
        return QVariant::fromValue( ml_track->getDuration() );
    case TRACK_ALBUM:
        return QVariant::fromValue( ml_track->getAlbumTitle() );
    case TRACK_ARTIST:
        return QVariant::fromValue( ml_track->getArtist() );
    case TRACK_TITLE_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_track->getTitle() ) );
    case TRACK_ALBUM_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_track->getAlbumTitle() ) );
    case TRACK_ARTIST_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_track->getArtist() ) );
    default :
        return QVariant();
    }
}

QHash<int, QByteArray> MLAlbumTrackModel::roleNames() const
{
    return {
        { TRACK_ID, "id" },
        { TRACK_TITLE, "title" },
        { TRACK_COVER, "cover" },
        { TRACK_NUMBER, "track_number" },
        { TRACK_DISC_NUMBER, "disc_number" },
        { TRACK_DURATION, "duration" },
        { TRACK_ALBUM, "album_title"},
        { TRACK_ARTIST, "main_artist"},
        { TRACK_TITLE_FIRST_SYMBOL, "title_first_symbol"},
        { TRACK_ALBUM_FIRST_SYMBOL, "album_title_first_symbol"},
        { TRACK_ARTIST_FIRST_SYMBOL, "main_artist_first_symbol"},
    };
}

vlc_ml_sorting_criteria_t MLAlbumTrackModel::roleToCriteria(int role) const
{
    switch (role) {
    case TRACK_TITLE :
        return VLC_ML_SORTING_ALPHA;
    case TRACK_NUMBER :
        return VLC_ML_SORTING_TRACKNUMBER;
    case TRACK_DURATION :
        return VLC_ML_SORTING_DURATION;
    default:
        return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLAlbumTrackModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLAlbumTrackModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
{
    return M_names_to_criteria.key(criteria, "");
}

void MLAlbumTrackModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
            if ( event.creation.media.i_subtype == VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_MEDIA_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_MEDIA_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_ALBUM_UPDATED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM &&
                 m_parent.id == event.modification.i_entity_id )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_ALBUM_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM &&
                 m_parent.id == event.deletion.i_entity_id )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 m_parent.id == event.deletion.i_entity_id )
                emit resetRequested();
            return;
    }

    MLBaseModel::onVlcMlEvent( event );
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLAlbumTrackModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLAlbumTrackModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    if ( m_parent.id <= 0 )
        return vlc_ml_count_audio_media(ml, &queryParams);
    return vlc_ml_count_media_of(ml, &queryParams, m_parent.type, m_parent.id );
}

std::vector<std::unique_ptr<MLItem>>
MLAlbumTrackModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> media_list;

    if ( m_parent.id <= 0 )
        media_list.reset( vlc_ml_list_audio_media(ml, &queryParams) );
    else
        media_list.reset( vlc_ml_list_media_of(ml, &queryParams, m_parent.type, m_parent.id ) );
    if ( media_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_media_t& media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::make_unique<MLAlbumTrack>( ml, &media ) );
    return res;
}

std::unique_ptr<MLItem>
MLAlbumTrackModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLAlbumTrack>(ml, media.get());
}

