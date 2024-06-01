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

#include "mlvideo.hpp"

#include <cassert>

#include <vlc_thumbnailer.h>
#include "mlhelper.hpp"

VideoDescription::VideoDescription(const QString &codec, const QString &language, const unsigned int fps)
    : m_codec(codec)
    , m_language(language)
    , m_fps(fps)
{
}

QString VideoDescription::getCodec() const
{
    return m_codec;
}

QString VideoDescription::getLanguage() const
{
    return m_language;
}

unsigned int VideoDescription::getFps() const
{
    return m_fps;
}

AudioDescription::AudioDescription(const QString &codec, const QString &language, const unsigned int nbChannels, const unsigned int sampleRate)
    : m_codec(codec)
    , m_language(language)
    , m_nbchannels(nbChannels)
    , m_sampleRate(sampleRate)
{
}

QString AudioDescription::getCodec() const
{
    return m_codec;
}

QString AudioDescription::getLanguage() const
{
    return m_language;
}

unsigned int AudioDescription::getNbChannels() const
{
    return m_nbchannels;
}

unsigned int AudioDescription::getSampleRate() const
{
    return m_sampleRate;
}

MLVideo::MLVideo(const vlc_ml_media_t* data)
    : MLItem( MLItemId( data->i_id, VLC_ML_PARENT_UNKNOWN ) )
    , m_title( QString::fromUtf8( data->psz_title ) )
    , m_thumbnail( QString::fromUtf8( data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_duration( data->i_duration )
    , m_progress( data->f_progress )
    , m_playCount( data->i_playcount )
    , m_thumbnailStatus( data->thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status )
{
    assert( data->i_type == VLC_ML_MEDIA_TYPE_VIDEO || data->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN );

    m_isNew = (m_playCount == 0 && m_progress <= 0);

    m_isFavorite = data->b_is_favorite;

    for( const vlc_ml_file_t& file: ml_range_iterate<vlc_ml_file_t>( data->p_files ) )
        if( file.i_type == VLC_ML_FILE_TYPE_MAIN )
        {
            //FIXME should we store every mrl
            m_mrl = QUrl::fromEncoded(file.psz_mrl);

            m_fileName = QUrl(m_mrl).fileName();

            break;
        }

    unsigned int numChannel = 0 , maxWidth = 0 , maxHeight = 0;
    for( const vlc_ml_media_track_t& track: ml_range_iterate<vlc_ml_media_track_t>( data->p_tracks ) ) {
        if ( track.i_type == VLC_ML_TRACK_TYPE_AUDIO ) {
            numChannel = std::max( numChannel , track.a.i_nbChannels );

            m_audioDesc.push_back( { QString::fromUtf8( track.psz_codec ) ,
                                     QString::fromUtf8( track.psz_language  ) ,
                                     track.a.i_nbChannels ,
                                     track.a.i_sampleRate }
                                 );
        }
        else if ( track.i_type == VLC_ML_TRACK_TYPE_VIDEO ){
            maxWidth = std::max( maxWidth, track.v.i_width );
            maxHeight = std::max( maxHeight, track.v.i_height );

            m_videoDesc.push_back( { QString::fromUtf8( track.psz_codec ) ,
                                     QString::fromUtf8( track.psz_language ) ,
                                     track.v.i_fpsNum }
                                 );
        }
    }

    m_channel = "";
    if ( numChannel >= 8 )
        m_channel = "7.1";
    else if ( numChannel >= 6 )
        m_channel = "5.1";

    m_resolution = "";
    if ( maxWidth >= 7680 && maxHeight >= 4320 )
        m_resolution = "8K";
    else if ( maxWidth >= 3840 && maxHeight >= 2160 )
        m_resolution = "4K";
    else if ( maxWidth >= 1440 && maxHeight >= 1080 )
        m_resolution = "HD";
    else if ( maxWidth >= 720 && maxHeight >= 1280 )
        m_resolution = "720p";  
}

bool MLVideo::isNew() const
{
    return m_isNew;
}

void MLVideo::setIsNew(bool isNew)
{
    m_isNew = isNew;
}

bool MLVideo::isFavorite() const
{
    return m_isFavorite;
}

void MLVideo::setIsFavorite(bool isFavorite)
{
    m_isFavorite = isFavorite;
}

QString MLVideo::getFileName() const
{
    return m_fileName;
}

QString MLVideo::getTitle() const
{
    return m_title;
}

QString MLVideo::getThumbnail(vlc_ml_thumbnail_status_t* status)
{
    if (status)
        *status = m_thumbnailStatus;
    return m_thumbnail;
}

void MLVideo::setThumbnail(vlc_ml_thumbnail_status_t status, QString mrl)
{
    m_thumbnailStatus = status;
    m_thumbnail = mrl;
}

VLCTick MLVideo::getDuration() const
{
    return VLCTick::fromMS(m_duration);
}

QString MLVideo::getMRL() const
{
    return m_mrl.toEncoded();
}

QString MLVideo::getDisplayMRL() const
{
    return m_mrl.toString(QUrl::PrettyDecoded | QUrl::RemoveUserInfo | QUrl::PreferLocalFile | QUrl::NormalizePathSegments);
}

QString MLVideo::getResolutionName() const
{
    return m_resolution;
}
QString MLVideo::getChannel() const
{
    return m_channel;
}

float MLVideo::getProgress() const
{
    return m_progress;
}

unsigned int MLVideo::getPlayCount() const
{
    return m_playCount;
}

VLCTick MLVideo::getProgressTime() const
{
    return VLCTick::fromMS(m_duration * m_progress);
}

QList<VideoDescription> MLVideo::getVideoDesc() const
{
    return m_videoDesc;
}

QList<AudioDescription> MLVideo::getAudioDesc() const
{
    return m_audioDesc;
}
