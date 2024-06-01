/*****************************************************************************
 * dbus_tracklist.c : dbus control module (mpris v2.2) - TrackList interface
 *****************************************************************************
 * Copyright © 2006-2011 Rafaël Carré
 * Copyright © 2007-2011 Mirsal Ennaime
 * Copyright © 2009-2011 The VideoLAN team
 * Copyright © 2013      Alex Merry
 *
 * Authors:    Mirsal Ennaime <mirsal at mirsal fr>
 *             Rafaël Carré <funman at videolanorg>
 *             Alex Merry <dev at randomguy3 me uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <assert.h>

#include "dbus_tracklist.h"
#include "dbus_common.h"


tracklist_append_event_t *tracklist_append_event_create(size_t index, vlc_playlist_item_t *const items[], size_t count) {
    tracklist_append_event_t* result = malloc(sizeof(tracklist_append_event_t) + sizeof(vlc_playlist_item_t *[count]));
    if (!result)
        return result;

    *result = (tracklist_append_event_t) { .change_ev = { .index = index, .count = count } };
    for (size_t i = 0; i < count; ++i) {
        result->items[i] = items[i];
        vlc_playlist_item_Hold(items[i]);
    }
    return result;
}

tracklist_remove_event_t *tracklist_remove_event_create(size_t index, size_t count) {
    tracklist_remove_event_t* result = malloc(sizeof(tracklist_remove_event_t));
    if (!result)
        return result;

    *result = (tracklist_remove_event_t) { .change_ev = { .index = index, .count = count } };
    return result;
}

void tracklist_append_event_destroy(tracklist_append_event_t *event) {
    if (!event)
        return;
    for (size_t i = 0; i < event->change_ev.count; ++i) {
        vlc_playlist_item_Release(event->items[i]);
    }
    free(event);
}

void tracklist_remove_event_destroy(tracklist_remove_event_t *event) {
    free(event);
}

static DBusHandlerResult InvalidTrackId(DBusConnection *p_conn,
                                        DBusMessage *p_from,
                                        const char *trackId,
                                        void *p_this) {
  msg_Err((vlc_object_t *)p_this, "Invalid track id: %s", trackId);

  DBusMessage *p_msg = dbus_message_new_error_printf(
      p_from, DBUS_ERROR_UNKNOWN_OBJECT, "Invalid track id: %s", trackId);
  if (!p_msg)
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  REPLY_SEND;
}

static DBusHandlerResult InvalidArguments(DBusConnection *p_conn,
                                          DBusMessage *p_from,
                                          void *p_this) {
  msg_Err((vlc_object_t *)p_this, "Invalid arguments");

  DBusMessage *p_msg = dbus_message_new_error(p_from, DBUS_ERROR_INVALID_ARGS,
                                              "Invalid arguments");
  if (!p_msg)
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  REPLY_SEND;
}

DBUS_METHOD( AddTrack )
{
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    char *psz_mrl, *psz_aftertrack;
    dbus_bool_t b_play;

    bool b_append = false;
    size_t i_pos;

    size_t i_append_len  = sizeof( DBUS_MPRIS_APPEND );
    size_t i_notrack_len = sizeof( DBUS_MPRIS_NOTRACK );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_mrl,
            DBUS_TYPE_OBJECT_PATH, &psz_aftertrack,
            DBUS_TYPE_BOOLEAN, &b_play,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );

        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if( !strncmp( DBUS_MPRIS_APPEND, psz_aftertrack, i_append_len ) )
        b_append = true;
    else if( !strncmp( DBUS_MPRIS_NOTRACK, psz_aftertrack, i_notrack_len ) )
        i_pos = 0;
    else if(sscanf( psz_aftertrack, MPRIS_TRACKID_FORMAT, &i_pos) == 1)
        ;
    else
    {
        msg_Warn( (vlc_object_t *) p_this,
                "AfterTrack: Invalid track ID \"%s\", appending instead",
                psz_aftertrack );
        b_append = true;
    }

    input_item_t *item = input_item_New( psz_mrl, NULL );
    if( unlikely(item == NULL) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    size_t count = vlc_playlist_Count(playlist);
    if (b_append || i_pos > count)
        i_pos = count;
    vlc_playlist_InsertOne(playlist, i_pos, item);
    if (b_play)
        vlc_playlist_PlayAt(playlist, i_pos);
    vlc_playlist_Unlock(playlist);

    input_item_Release( item );

    REPLY_SEND;
}

DBUS_METHOD( GetTracksMetadata )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    size_t i_track_id;
    const char *psz_track_id = NULL;

    vlc_playlist_t *playlist = PL;

    DBusMessageIter in_args, track_ids, meta;
    dbus_message_iter_init( p_from, &in_args );

    if( DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type( &in_args ) )
    {
        return InvalidArguments(p_conn, p_from, p_this);
    }

    dbus_message_iter_recurse( &in_args, &track_ids );
    dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "a{sv}", &meta );

    bool id_valid = true;
    while( DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type( &track_ids ) )
    {
        dbus_message_iter_get_basic( &track_ids, &psz_track_id );

        if( 1 != sscanf( psz_track_id, MPRIS_TRACKID_FORMAT, &i_track_id ) )
        {
            id_valid = false;
            break;
        }

        vlc_playlist_item_t *item = NULL;

        vlc_playlist_Lock(playlist);
        id_valid = i_track_id < vlc_playlist_Count(playlist);
        if( id_valid )
        {
            item = vlc_playlist_Get(playlist, i_track_id);
            vlc_playlist_item_Hold(item);
        }
        vlc_playlist_Unlock(playlist);
        if( !id_valid )
            break;

        GetInputMeta(i_track_id, item, &meta);
        vlc_playlist_item_Release(item);

        dbus_message_iter_next( &track_ids );
    }

    if( !id_valid )
    {
        dbus_message_iter_abandon_container( &args, &meta );
        dbus_message_unref(p_msg);
        return InvalidTrackId(p_conn, p_from, psz_track_id, p_this);
    }

    dbus_message_iter_close_container( &args, &meta );
    REPLY_SEND;
}

DBUS_METHOD( GoTo )
{
    REPLY_INIT;

    size_t i_track_id;
    const char *psz_track_id = NULL;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_OBJECT_PATH, &psz_track_id,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if( 1 != sscanf( psz_track_id, MPRIS_TRACKID_FORMAT, &i_track_id ) )
        goto invalid_track_id;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    bool id_valid = i_track_id < vlc_playlist_Count(playlist);
    if (id_valid)
        vlc_playlist_PlayAt(playlist, i_track_id);
    vlc_playlist_Unlock(playlist);
    if (!id_valid)
        goto invalid_track_id;

    REPLY_SEND;

invalid_track_id:
    return InvalidTrackId(p_conn, p_from, psz_track_id, p_this);
}

DBUS_METHOD( RemoveTrack )
{
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    size_t i_id;
    char *psz_id = NULL;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_OBJECT_PATH, &psz_id,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if( 1 != sscanf( psz_id, MPRIS_TRACKID_FORMAT, &i_id ) )
        goto invalid_track_id;

    vlc_playlist_t *playlist = PL;
    vlc_playlist_Lock(playlist);
    bool valid_id = i_id < vlc_playlist_Count(playlist);
    if (valid_id)
        vlc_playlist_RemoveOne(playlist, i_id);
    vlc_playlist_Unlock(playlist);
    if (!valid_id)
        goto invalid_track_id;

    REPLY_SEND;

invalid_track_id:
    return InvalidTrackId(p_conn, p_from, psz_id, p_this);
}

static int MarshalTrack( DBusMessageIter *iter, size_t index )
{
    char *psz_track_id = NULL;
    int ret = VLC_SUCCESS;

    if (asprintf(&psz_track_id, MPRIS_TRACKID_FORMAT, index) == -1)
        ret = VLC_ENOMEM;

    if (ret == VLC_SUCCESS &&
        !dbus_message_iter_append_basic( iter,
                                         DBUS_TYPE_OBJECT_PATH,
                                         &psz_track_id ) )
    {
       ret = VLC_ENOMEM;
    }
    free( psz_track_id );
    return ret;
}

static int
MarshalTracks( intf_thread_t *p_intf, DBusMessageIter *container )
{
    DBusMessageIter tracks;
    vlc_playlist_t *playlist = p_intf->p_sys->playlist;

    dbus_message_iter_open_container( container, DBUS_TYPE_ARRAY, "o",
                                      &tracks );

    vlc_playlist_Lock(playlist);
    size_t pl_size = vlc_playlist_Count(playlist);
    vlc_playlist_Unlock(playlist);
    for (size_t i = 0; i < pl_size; i++)
    {
        int err = MarshalTrack( &tracks, i );
        if (err !=  VLC_SUCCESS)
        {
            dbus_message_iter_abandon_container( container, &tracks );
            return err;
        }
    }

    if( !dbus_message_iter_close_container( container, &tracks ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
MarshalCanEditTracks( intf_thread_t *p_intf, DBusMessageIter *container )
{
    VLC_UNUSED( p_intf );
    const dbus_bool_t b_ret = TRUE;

    if( !dbus_message_iter_append_basic( container, DBUS_TYPE_BOOLEAN, &b_ret ) )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

#define PROPERTY_MAPPING_BEGIN if( 0 ) {}
#define PROPERTY_GET_FUNC( prop, signature ) \
    else if( !strcmp( psz_property_name,  #prop ) ) { \
        if( !dbus_message_iter_open_container( &args, DBUS_TYPE_VARIANT, signature, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        if( VLC_SUCCESS != Marshal##prop( p_this, &v ) ) { \
            dbus_message_iter_abandon_container( &args, &v ); \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
        } \
        if( !dbus_message_iter_close_container( &args, &v ) ) \
            return DBUS_HANDLER_RESULT_NEED_MEMORY; \
    }

#define PROPERTY_SET_FUNC( prop ) \
    else if( !strcmp( psz_property_name,  #prop ) ) { \
        return prop##Set( p_conn, p_from, p_this ); \
    }
#define PROPERTY_MAPPING_END else { return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; }


DBUS_METHOD( GetProperty )
{
    DBusError error;

    char *psz_interface_name = NULL;
    char *psz_property_name  = NULL;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_interface_name,
            DBUS_TYPE_STRING, &psz_property_name,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                                         error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    msg_Dbg( (vlc_object_t*) p_this, "Getting property %s",
                                     psz_property_name );

    if( strcmp( psz_interface_name, DBUS_MPRIS_TRACKLIST_INTERFACE ) ) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    REPLY_INIT;
    OUT_ARGUMENTS;
    DBusMessageIter v;

    PROPERTY_MAPPING_BEGIN
    PROPERTY_GET_FUNC( Tracks, "ao" )
    PROPERTY_GET_FUNC( CanEditTracks, "b" )
    PROPERTY_MAPPING_END

    REPLY_SEND;
}

#undef PROPERTY_MAPPING_BEGIN
#undef PROPERTY_GET_FUNC
#undef PROPERTY_SET_FUNC
#undef PROPERTY_MAPPING_END

#define ADD_PROPERTY( prop, signature ) \
    if( VLC_SUCCESS != AddProperty( (intf_thread_t*) p_this, \
                &dict, #prop, signature, Marshal##prop ) ) { \
        dbus_message_iter_abandon_container( &args, &dict ); \
        return VLC_ENOMEM; \
    }

DBUS_METHOD( GetAllProperties )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    DBusMessageIter dict;

    char *const psz_interface_name = NULL;

    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_interface_name,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s",
                                         error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if( !dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "{sv}", &dict ) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    ADD_PROPERTY ( Tracks,        "ao" )
    ADD_PROPERTY ( CanEditTracks, "b"  )

    if( !dbus_message_iter_close_container( &args, &dict ))
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    REPLY_SEND;
}

#undef ADD_PROPERTY

#define METHOD_FUNC( interface, method, function ) \
    else if( dbus_message_is_method_call( p_from, interface, method ) )\
        return function( p_conn, p_from, p_this )

DBusHandlerResult
handle_tracklist ( DBusConnection *p_conn, DBusMessage *p_from, void *p_this )
{
    if(0);

    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "Get",    GetProperty );
    METHOD_FUNC( DBUS_INTERFACE_PROPERTIES, "GetAll", GetAllProperties );

    /* here D-Bus method names are associated to an handler */

    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "GoTo",        GoTo );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "AddTrack",    AddTrack );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "RemoveTrack", RemoveTrack );
    METHOD_FUNC( DBUS_MPRIS_TRACKLIST_INTERFACE, "GetTracksMetadata",
                                                  GetTracksMetadata );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#undef METHOD_FUNC

/**
 * PropertiesChangedSignal: synthetizes and sends the
 * org.freedesktop.DBus.Properties.PropertiesChanged signal
 */
static DBusHandlerResult
PropertiesChangedSignal( intf_thread_t    *p_intf,
                         vlc_dictionary_t *p_changed_properties )
{
    DBusConnection  *p_conn = p_intf->p_sys->p_conn;
    DBusMessageIter changed_properties, invalidated_properties;
    const char *psz_interface_name = DBUS_MPRIS_TRACKLIST_INTERFACE;

    SIGNAL_INIT( DBUS_INTERFACE_PROPERTIES,
                 DBUS_MPRIS_OBJECT_PATH,
                 "PropertiesChanged" );

    OUT_ARGUMENTS;
    ADD_STRING( &psz_interface_name );

    if( unlikely(!dbus_message_iter_open_container( &args,
                                                    DBUS_TYPE_ARRAY, "{sv}",
                                                    &changed_properties )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( unlikely(!dbus_message_iter_close_container( &args,
                                                     &changed_properties )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if( unlikely(!dbus_message_iter_open_container( &args, DBUS_TYPE_ARRAY, "s",
                                                    &invalidated_properties )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;


    if( vlc_dictionary_has_key( p_changed_properties, "Tracks" ) )
        dbus_message_iter_append_basic( &invalidated_properties,
                                        DBUS_TYPE_STRING,
                                        &(char const*){ "Tracks" } );

    if( unlikely(!dbus_message_iter_close_container( &args,
                    &invalidated_properties )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    SIGNAL_SEND;
}

/**
 * TrackAddedSignal: synthetizes and sends the
 * org.mpris.MediaPlayer2.TrackList.TrackAdded signal
 */
static DBusHandlerResult
TrackAddedSignal( intf_thread_t    *p_intf,
                  size_t index,
                  vlc_playlist_item_t *item )
{
    DBusConnection  *p_conn = p_intf->p_sys->p_conn;
    DBusMessageIter meta;

    SIGNAL_INIT( "MediaPlayer2.TrackList",
                 DBUS_MPRIS_OBJECT_PATH,
                 "TrackAdded" );

    OUT_ARGUMENTS;

    if( unlikely(!dbus_message_iter_open_container( &args,
                                                    DBUS_TYPE_ARRAY, "a{sv}",
                                                    &meta )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    GetInputMeta(index, item, &meta);

    if( unlikely(!dbus_message_iter_close_container( &args,
                                                     &meta )) )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    if ( MarshalTrack( &args, index ) !=  VLC_SUCCESS )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    SIGNAL_SEND;
}

/**
 * TrackRemovedSignal: synthetizes and sends the
 * org.mpris.MediaPlayer2.TrackList.TrackRemoved signal
 */
static DBusHandlerResult
TrackRemovedSignal( intf_thread_t    *p_intf, size_t index )
{
    DBusConnection  *p_conn = p_intf->p_sys->p_conn;

    SIGNAL_INIT( "MediaPlayer2.TrackList",
                 DBUS_MPRIS_OBJECT_PATH,
                 "TrackRemoved" );

    OUT_ARGUMENTS;

    if ( MarshalTrack( &args, index ) !=  VLC_SUCCESS )
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    SIGNAL_SEND;
}
/**
 * TrackListPropertiesChangedEmit: Emits the following signals:
 * - org.freedesktop.DBus.Properties.PropertiesChanged
 * - org.mpris.MediaPlayer2.TrackList.TrackAdded
 */
int TrackListPropertiesChangedEmit( intf_thread_t    * p_intf,
                                    vlc_dictionary_t * p_changed_properties )
{
    if( p_intf->p_sys->b_dead )
        return VLC_SUCCESS;

    PropertiesChangedSignal( p_intf, p_changed_properties );

    if( vlc_dictionary_has_key( p_changed_properties, "TrackAdded" ) ) {
        tracklist_append_event_t *added_tracks =
            vlc_dictionary_value_for_key( p_changed_properties, "TrackAdded" );

        while ( added_tracks ) {
            for (size_t i = 0; i < added_tracks->change_ev.count; ++i) {
                TrackAddedSignal( p_intf,
                        added_tracks->change_ev.index + i,
                        added_tracks->items[i] );
            }
            tracklist_append_event_t *next = tracklist_append_event_next( added_tracks );
            tracklist_append_event_destroy( added_tracks );
            added_tracks = next;
        }
    }

    if( vlc_dictionary_has_key( p_changed_properties, "TrackRemoved" ) ) {
        tracklist_remove_event_t *removed_tracks =
            vlc_dictionary_value_for_key( p_changed_properties, "TrackRemoved" );

        while ( removed_tracks ) {
            for (size_t i = 0; i < removed_tracks->change_ev.count; ++i) {
                TrackRemovedSignal( p_intf, removed_tracks->change_ev.index + i );
            }
            tracklist_remove_event_t *next = tracklist_remove_event_next( removed_tracks );
            tracklist_remove_event_destroy( removed_tracks );
            removed_tracks = next;
        }

    }

    return VLC_SUCCESS;
}
