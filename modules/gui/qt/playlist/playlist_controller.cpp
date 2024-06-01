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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist_controller.hpp"
#include "playlist_controller_p.hpp"
#include "vlc_player.h"
#include "vlc_url.h"
#include "util/qmlinputitem.hpp"
#include <algorithm>
#include <QVariant>
#include <QDesktopServices>

namespace vlc {
  namespace playlist {

static QVector<PlaylistItem> toVec(vlc_playlist_item_t *const items[],
                                   size_t len)
{
    QVector<PlaylistItem> vec;
    for (size_t i = 0; i < len; ++i)
        vec.push_back(items[i]);
    return vec;
}

template <typename RAW, typename WRAPPER>
static QVector<RAW> toRaw(const QVector<WRAPPER> &items)
{
    QVector<RAW> vec;
    int count = items.size();
    vec.reserve(count);
    for (int i = 0; i < count; ++i)
        vec.push_back(items[i].raw());
    return vec;
}

static QUrl resolveWinSymlinks(const QUrl &mrl)
{
#ifdef _WIN32
    QFileInfo info (mrl.toLocalFile());
    if ( info.isSymLink() )
    {
        QString target = info.symLinkTarget();
        return QFile::exists(target) ? QUrl::fromLocalFile(target) : mrl;
    }
#endif
    return mrl;
}

QVector<Media> toMediaList(const QVariantList &sources)
{
    QVector<Media> mediaList;
    std::transform(sources.begin(), sources.end(),
                   std::back_inserter(mediaList), [](const QVariant& value) {
        if (value.canConvert<QUrl>() || value.canConvert<QString>())
        {
            QUrl mrl = value.canConvert<QString>()
                    ? QUrl::fromUserInput(value.value<QString>())
                    : value.value<QUrl>();

            if (mrl.isLocalFile())
                mrl = resolveWinSymlinks(mrl);

            return Media(mrl.toString(QUrl::None), mrl.fileName());
        } else if (value.canConvert<QmlInputItem>())
        {
            const QmlInputItem & item = value.value<QmlInputItem>();
            return Media(item.item.get());
        }
        return Media{};
    });

    return mediaList;
}


extern "C" { // for C callbacks

static void
on_playlist_items_reset(vlc_playlist_t *playlist,
                        vlc_playlist_item_t *const items[],
                        size_t len, void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    auto vec = toVec(items, len);
    size_t totalCount = vlc_playlist_Count(playlist);

    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        PlaylistControllerModel* q = that->q_func();
        bool empty = vec.size() == 0;
        if (that->m_empty != empty)
        {
            that->m_empty = empty;
            emit q->isEmptyChanged(empty);
        }
        emit q->itemsReset(vec);

        if (totalCount != that->m_count)
        {
            that->m_count = totalCount;
            emit q->countChanged(totalCount);
        }
    });
}

static void
on_playlist_items_added(vlc_playlist_t *playlist, size_t index,
                        vlc_playlist_item_t *const items[], size_t len,
                        void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    auto vec = toVec(items, len);
    size_t totalCount = vlc_playlist_Count(playlist);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        PlaylistControllerModel* q = that->q_func();
        if (that->m_empty && vec.size() > 0)
        {
            that->m_empty = false;
            emit q->isEmptyChanged(that->m_empty);
        }
        emit q->itemsAdded(index, vec);

        if (totalCount != that->m_count)
        {
            that->m_count = totalCount;
            emit q->countChanged(totalCount);
        }
    });
}

static void
on_playlist_items_moved(vlc_playlist_t *playlist, size_t index, size_t count,
                        size_t target, void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        emit that->q_func()->itemsMoved(index, count, target);
    });
}

static void
on_playlist_items_removed(vlc_playlist_t *playlist, size_t index, size_t count,
                          void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    size_t totalCount = vlc_playlist_Count(playlist);
    bool empty = (totalCount == 0);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        PlaylistControllerModel* q = that->q_func();
        if (that->m_empty != empty)
        {
            that->m_empty = empty;
            emit q->isEmptyChanged(empty);
        }
        emit q->itemsRemoved(index, count);
        if (totalCount != that->m_count)
        {
            that->m_count = totalCount;
            emit q->countChanged(totalCount);
        }
    });
}

static void
on_playlist_items_updated(vlc_playlist_t *playlist, size_t index,
                          vlc_playlist_item_t *const items[], size_t len,
                          void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    auto vec = toVec(items, len);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        emit that->q_func()->itemsUpdated(index, vec);
        if (that->m_currentIndex != -1) {
            size_t currentIndex = static_cast<size_t>(that->m_currentIndex);
            if (currentIndex >= index && currentIndex < index + len)
            {
                that->m_currentItem = vec[currentIndex - index];
                emit that->q_func()->currentItemChanged();
            }
        }
    });
}

static void
on_playlist_playback_repeat_changed(vlc_playlist_t *playlist,
                                    enum vlc_playlist_playback_repeat repeat,
                                    void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        PlaylistControllerModel::PlaybackRepeat repeatMode = static_cast<PlaylistControllerModel::PlaybackRepeat>(repeat);
        if (that->m_repeat != repeatMode )
        {
            that->m_repeat = repeatMode;
            emit that->q_func()->repeatModeChanged(repeatMode);
        }
    });
}

static void
on_playlist_playback_order_changed(vlc_playlist_t *playlist,
                                   enum vlc_playlist_playback_order order,
                                   void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        bool isRandom = order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
        if (that->m_random != isRandom)
        {
            that->m_random = isRandom;
            emit that->q_func()->randomChanged(isRandom);
        }
    });
}

static void
on_playlist_current_item_changed(vlc_playlist_t *playlist, ssize_t index,
                                 void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);


    vlc_playlist_item_t* playlistItem = nullptr;
    if (index >= 0)
        playlistItem = vlc_playlist_Get(playlist, index);
    PlaylistItem newItem{ playlistItem };

    that->callAsync([=](){
        PlaylistControllerModel* q = that->q_func();

        if (that->m_playlist != playlist)
            return;
        if (that->m_currentIndex != index)
        {
            that->m_currentIndex = index;
            emit q->currentIndexChanged(that->m_currentIndex);
        }
        that->m_currentItem = newItem;
        emit q->currentItemChanged();
    });
}

static void
on_playlist_has_prev_changed(vlc_playlist_t *playlist, bool has_prev,
                             void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        if (that->m_hasPrev != has_prev)
        {
            that->m_hasPrev = has_prev;
            emit that->q_func()->hasPrevChanged(has_prev);
        }
    });
}

static void
on_playlist_has_next_changed(vlc_playlist_t *playlist, bool has_next,
                             void *userdata)
{
    PlaylistControllerModelPrivate *that = static_cast<PlaylistControllerModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        if (that->m_hasNext != has_next)
        {
            that->m_hasNext = has_next;
            emit that->q_func()->hasNextChanged(has_next);
        }
    });
}

} // extern "C"

static const struct vlc_playlist_callbacks playlist_callbacks = []{
    struct vlc_playlist_callbacks cbs {};
    cbs.on_items_reset = on_playlist_items_reset;
    cbs.on_items_added = on_playlist_items_added;
    cbs.on_items_moved = on_playlist_items_moved;
    cbs.on_items_removed = on_playlist_items_removed;
    cbs.on_items_updated = on_playlist_items_updated;
    cbs.on_playback_repeat_changed = on_playlist_playback_repeat_changed;
    cbs.on_playback_order_changed= on_playlist_playback_order_changed;
    cbs.on_current_index_changed = on_playlist_current_item_changed;
    cbs.on_has_next_changed = on_playlist_has_next_changed;
    cbs.on_has_prev_changed = on_playlist_has_prev_changed;
    return cbs;
}();


//private API

PlaylistControllerModelPrivate::PlaylistControllerModelPrivate(PlaylistControllerModel* playlistController)
    : q_ptr(playlistController)
{
    fillSortKeyTitleList();
}

PlaylistControllerModelPrivate::~PlaylistControllerModelPrivate()
{
    if (m_playlist && m_listener)
    {
        PlaylistLocker lock(m_playlist);
        vlc_playlist_RemoveListener(m_playlist, m_listener);
    }
}

//public API

PlaylistControllerModel::PlaylistControllerModel(QObject *parent)
    : QObject(parent)
    , d_ptr( new PlaylistControllerModelPrivate(this) )
{
    connect(this, &PlaylistControllerModel::itemsMoved, this, &PlaylistControllerModel::resetSortKey);
    connect(this, &PlaylistControllerModel::itemsAdded, this, &PlaylistControllerModel::resetSortKey);
    connect(this, &PlaylistControllerModel::isEmptyChanged, [this](bool isEmpty) {if (isEmpty) emit resetSortKey();});
}

PlaylistControllerModel::PlaylistControllerModel(vlc_playlist_t *playlist, QObject *parent)
    : QObject(parent)
    , d_ptr( new PlaylistControllerModelPrivate(this) )
{
    setPlaylistPtr(playlist);
}

PlaylistControllerModel::~PlaylistControllerModel()
{
}

PlaylistItem PlaylistControllerModel::getCurrentItem() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_currentItem;
}

void PlaylistControllerModel::append(const QVariantList& sourceList, bool startPlaying)
{
    append(toMediaList(sourceList), startPlaying);
}

void PlaylistControllerModel::insert(unsigned index, const QVariantList& sourceList, bool startPlaying)
{
    insert(index, toMediaList(sourceList), startPlaying);
}

void
PlaylistControllerModel::append(const QVector<Media> &media, bool startPlaying)
{
    Q_D(PlaylistControllerModel);

    PlaylistLocker locker(d->m_playlist);

    auto rawMedia = toRaw<input_item_t *>(media);
    /* We can't append an empty media. */
    assert(rawMedia.size() > 0);

    int ret = vlc_playlist_Append(d->m_playlist,
                                  rawMedia.constData(), rawMedia.size());
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
    if (startPlaying)
    {
        ssize_t playIndex = (ssize_t)vlc_playlist_Count( d->m_playlist ) - rawMedia.size();
        ret = vlc_playlist_GoTo(d->m_playlist, playIndex);
        if (ret != VLC_SUCCESS)
            return;
        vlc_playlist_Start(d->m_playlist);
    }
}

void
PlaylistControllerModel::insert(size_t index, const QVector<Media> &media, bool startPlaying)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker locker(d->m_playlist);

    auto rawMedia = toRaw<input_item_t *>(media);
    /* We can't insert an empty media. */
    assert(rawMedia.size() > 0);

    int ret = vlc_playlist_RequestInsert(d->m_playlist, index,
                                         rawMedia.constData(), rawMedia.size());
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
    if (startPlaying)
    {
        ret = vlc_playlist_GoTo(d->m_playlist, index);
        if (ret != VLC_SUCCESS)
            return;
        vlc_playlist_Start(d->m_playlist);
    }
}

void
PlaylistControllerModel::move(const QVector<PlaylistItem> &items, size_t target,
               ssize_t indexHint)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker locker(d->m_playlist);

    auto rawItems = toRaw<vlc_playlist_item_t *>(items);
    int ret = vlc_playlist_RequestMove(d->m_playlist, rawItems.constData(),
                                       rawItems.size(), target, indexHint);
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
}

void
PlaylistControllerModel::remove(const QVector<PlaylistItem> &items, ssize_t indexHint)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker locker(d->m_playlist);
    auto rawItems = toRaw<vlc_playlist_item_t *>(items);
    int ret = vlc_playlist_RequestRemove(d->m_playlist, rawItems.constData(),
                                         rawItems.size(), indexHint);
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
}

void
PlaylistControllerModel::shuffle()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker locker(d->m_playlist);
    vlc_playlist_Shuffle(d->m_playlist);
}

void
PlaylistControllerModel::sort(const QVector<vlc_playlist_sort_criterion> &criteria)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker locker(d->m_playlist);
    vlc_playlist_Sort(d->m_playlist, criteria.constData(), criteria.size());
}

void PlaylistControllerModel::sort(PlaylistControllerModel::SortKey key, PlaylistControllerModel::SortOrder order)
{
    if(key != SortKey::SORT_KEY_NONE)
        setSortKey(key);
    setSortOrder(order);

    sort();
}

void PlaylistControllerModel::sort(PlaylistControllerModel::SortKey key)
{
    if (key == SortKey::SORT_KEY_NONE)
        return;

    if (getSortKey() != key)
    {
        setSortOrder(SortOrder::SORT_ORDER_ASC);
        setSortKey(key);
    }
    else
    {
        switchSortOrder();
    }

    sort();
}

void PlaylistControllerModel::sort(void)
{
    Q_D(PlaylistControllerModel);

    if (d->m_sortKey == SortKey::SORT_KEY_NONE)
        return;

    vlc_playlist_sort_criterion crit = {
        static_cast<vlc_playlist_sort_key>(d->m_sortKey),
        static_cast<vlc_playlist_sort_order>(d->m_sortOrder)
    };
    QVector<vlc_playlist_sort_criterion> criteria { crit };
    sort( criteria );
}

void PlaylistControllerModel::explore(const PlaylistItem& pItem)
{
    vlc_playlist_item_t * const playlistItem = pItem.raw();
    if( playlistItem )
    {
        input_item_t * const p_input = vlc_playlist_item_GetMedia(playlistItem);
        char * const uri = input_item_GetURI(p_input);

        if( uri && uri[0] != '\0')
        {
            char * const path = vlc_uri2path( uri );

            if( !path )
                return;

            QString containingDir = QFileInfo( path ).absolutePath();
            if( !QFileInfo( containingDir ).isDir() )
                return;

            QUrl file = QUrl::fromLocalFile( containingDir );
            free( path );

            if( !file.isLocalFile() )
                return;

            QDesktopServices::openUrl( file );
        }
    }
}

void PlaylistControllerModel::play()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Start( d->m_playlist );
}

void PlaylistControllerModel::pause()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Pause( d->m_playlist );
}

void PlaylistControllerModel::stop()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Stop( d->m_playlist );
}

void PlaylistControllerModel::next()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Next( d->m_playlist );
}

void PlaylistControllerModel::prev()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Prev( d->m_playlist );
}

void PlaylistControllerModel::prevOrReset()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    bool seek = false;
    vlc_player_t* player = vlc_playlist_GetPlayer( d->m_playlist );
    Q_ASSERT(player);
    if( !vlc_player_IsStarted(player) || vlc_player_GetTime(player) < VLC_TICK_FROM_MS(10) )
    {
        int ret = vlc_playlist_Prev( d->m_playlist );
        if (ret == VLC_SUCCESS)
            vlc_playlist_Start( d->m_playlist );
    }
    else
        seek = true;
    if (seek)
        vlc_player_JumpPos( player, 0.f );
}

void PlaylistControllerModel::togglePlayPause()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_player_t* player = vlc_playlist_GetPlayer( d->m_playlist );
    if ( vlc_player_IsStarted(player) )
        vlc_player_TogglePause( player );
    else
        vlc_playlist_Start( d->m_playlist );
}

void PlaylistControllerModel::toggleRandom()
{
    Q_D(PlaylistControllerModel);
    vlc_playlist_playback_order new_order;
    PlaylistLocker lock{ d->m_playlist };
    if ( d->m_random )
        new_order = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    else
        new_order = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    vlc_playlist_SetPlaybackOrder( d->m_playlist, new_order );
    config_PutInt( "random", new_order );
}

void PlaylistControllerModel::toggleRepeatMode()
{
    Q_D(PlaylistControllerModel);
    vlc_playlist_playback_repeat new_repeat;
    /* Toggle Normal -> Loop -> Repeat -> Normal ... */
    switch ( d->m_repeat ) {
    case PlaybackRepeat::PLAYBACK_REPEAT_NONE:
        new_repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
        break;
    case PlaybackRepeat::PLAYBACK_REPEAT_ALL:
        new_repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
        break;
    case PlaybackRepeat::PLAYBACK_REPEAT_CURRENT:
    default:
        new_repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
        break;
    }
    {
        PlaylistLocker lock{ d->m_playlist };
        vlc_playlist_SetPlaybackRepeat( d->m_playlist, new_repeat );
    }
    config_PutInt( "repeat", new_repeat );
}

void PlaylistControllerModel::clear()
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_Clear( d->m_playlist );
}

void PlaylistControllerModel::goTo(uint index, bool startPlaying)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    size_t count = vlc_playlist_Count( d->m_playlist );
    if (count == 0 || index > count-1)
        return;
    vlc_playlist_GoTo( d->m_playlist, index );
    if (startPlaying)
        vlc_playlist_Start( d->m_playlist );
}

bool PlaylistControllerModel::isRandom() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_random;
}

void PlaylistControllerModel::setRandom(bool random)
{
    Q_D(const PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_SetPlaybackOrder( d->m_playlist, random ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL );
}

PlaylistPtr PlaylistControllerModel::getPlaylistPtr() const
{
    Q_D(const PlaylistControllerModel);
    return PlaylistPtr(d->m_playlist);
}

void PlaylistControllerModel::setPlaylistPtr(vlc_playlist_t* newPlaylist)
{
    Q_D(PlaylistControllerModel);
    if (d->m_playlist && d->m_listener)
    {
        PlaylistLocker locker(d->m_playlist);
        vlc_playlist_RemoveListener(d->m_playlist, d->m_listener);
        d->m_playlist = nullptr;
        d->m_listener = nullptr;
    }
    if (newPlaylist)
    {
        PlaylistLocker locker(newPlaylist);
        d->m_playlist = newPlaylist;
        d->m_listener = vlc_playlist_AddListener(d->m_playlist, &playlist_callbacks, d, true);
        /*
         * Queue a playlistInitialized to be sent after the initial state callbacks
         * vlc_playlist_AddListener will synchronously call each callback in
         * playlist_callbacks, which will in turn queue an async call on the Qt
         * main thread
         */
        d->callAsync([=](){
            emit playlistInitialized();
        });
    }
    emit playlistPtrChanged( PlaylistPtr(newPlaylist) );
}

void PlaylistControllerModel::resetSortKey()
{
    Q_D(PlaylistControllerModel);
    d->m_sortKey = SortKey::SORT_KEY_NONE;
    emit sortKeyChanged();
}

void PlaylistControllerModel::setPlaylistPtr(PlaylistPtr ptr)
{
    setPlaylistPtr(ptr.m_playlist);
}

PlaylistControllerModel::PlaybackRepeat PlaylistControllerModel::getRepeatMode() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_repeat;
}

void PlaylistControllerModel::setRepeatMode(PlaylistControllerModel::PlaybackRepeat mode)
{
    Q_D(const PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_playlist_SetPlaybackRepeat( d->m_playlist, static_cast<vlc_playlist_playback_repeat>(mode) );
}


bool PlaylistControllerModel::isPlayAndExit() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_isPlayAndExit;
}

void PlaylistControllerModel::setPlayAndExit(bool enable)
{
    Q_D(PlaylistControllerModel);
    PlaylistLocker lock{ d->m_playlist };
    vlc_player_t* player = vlc_playlist_GetPlayer( d->m_playlist );
    vlc_player_SetMediaStoppedAction( player, enable ? VLC_PLAYER_MEDIA_STOPPED_EXIT :  VLC_PLAYER_MEDIA_STOPPED_CONTINUE );
}

bool PlaylistControllerModel::isEmpty() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_empty;
}

size_t PlaylistControllerModel::count() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_count;
}

void PlaylistControllerModel::setSortKey(SortKey sortKey)
{
    Q_D(PlaylistControllerModel);

    if (sortKey == d->m_sortKey)
        return;

    d->m_sortKey = sortKey;
    emit sortKeyChanged();
}

void PlaylistControllerModel::setSortOrder(SortOrder sortOrder)
{
    Q_D(PlaylistControllerModel);

    SortOrder order = d->m_sortOrder;
    if(sortOrder == order)
        return;

    d->m_sortOrder = sortOrder;
    emit sortOrderChanged();
}

void PlaylistControllerModel::switchSortOrder()
{
    Q_D(PlaylistControllerModel);

    SortOrder order = d->m_sortOrder;
    if (order == SortOrder::SORT_ORDER_ASC)
        order = SortOrder::SORT_ORDER_DESC;
    else if (order == SortOrder::SORT_ORDER_DESC)
        order = SortOrder::SORT_ORDER_ASC;
    else
        return;

    d->m_sortOrder = order;
    emit sortOrderChanged();
}

PlaylistControllerModel::SortKey PlaylistControllerModel::getSortKey() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_sortKey;
}

PlaylistControllerModel::SortOrder PlaylistControllerModel::getSortOrder() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_sortOrder;
}

bool PlaylistControllerModel::hasNext() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_hasNext;
}

bool PlaylistControllerModel::hasPrev() const
{
    Q_D(const PlaylistControllerModel);
    return d->m_hasPrev;
}

QVariantList PlaylistControllerModel::getSortKeyTitleList() const
{
    Q_D(const PlaylistControllerModel);

    return d->sortKeyTitleList;
}


  } // namespace playlist
} // namespace vlc
