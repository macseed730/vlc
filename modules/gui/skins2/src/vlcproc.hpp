/*****************************************************************************
 * vlcproc.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLCPROC_HPP
#define VLCPROC_HPP

#include <set>

#include <vlc_common.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include "../vars/equalizer.hpp"
#include "../vars/playtree.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"
#include "../utils/position.hpp"
#include "../utils/var_text.hpp"
#include "../utils/var_string.hpp"
#include "../controls/ctrl_video.hpp"

class OSTimer;
class VarBool;
struct vlc_window;


/// Singleton object handling VLC internal state and playlist
class VlcProc: public SkinObject
{
public:
    /// Get the instance of VlcProc
    /// Returns NULL if the initialization of the object failed
    static VlcProc *instance( intf_thread_t *pIntf );

    /// Delete the instance of VlcProc
    static void destroy( intf_thread_t *pIntf );

    /// Getter for the playtree variable
    Playtree &getPlaytreeVar() { return *((Playtree*)m_cPlaytree.get()); }

    /// Getter for the time variable
    StreamTime &getTimeVar() { return *((StreamTime*)(m_cVarTime.get())); }

    /// Getter for the volume variable
    Volume &getVolumeVar() { return *((Volume*)(m_cVarVolume.get())); }

    /// Getter for the current playback speed
    VarText &getSpeedVar()
       { return *((VarText*)(m_cVarSpeed.get())); }

    /// Getter for the stream name variable
    VarText &getStreamNameVar()
       { return *((VarText*)(m_cVarStreamName.get())); }

    /// Getter for the stream URI variable
    VarText &getStreamURIVar()
        { return *((VarText*)(m_cVarStreamURI.get())); }

    /// Getter for the stream bitrate variable
    VarText &getStreamBitRateVar()
        { return *((VarText*)(m_cVarStreamBitRate.get())); }

    /// Getter for the stream sample rate variable
    VarText &getStreamSampleRateVar()
        { return *((VarText*)(m_cVarStreamSampleRate.get())); }

    /// Getter for the stream Art url variable
    VarString &getStreamArtVar()
       { return *((VarString*)(m_cVarStreamArt.get())); }

    /// Getter/Setter for the fullscreen variable
    VarBool &getFullscreenVar() { return *((VarBool*)(m_cVarFullscreen.get())); }
    void setFullscreenVar( bool );

    /// Indicate whether the embedded video output is currently used
    bool isVoutUsed() const { return m_pVout != NULL; }

    /// initialize equalizer
    void init_equalizer( );

    void on_intf_event_changed( vlc_value_t newVal );
    void on_bit_rate_changed( vlc_value_t newVal );
    void on_sample_rate_changed( vlc_value_t newVal );

    void on_current_media_changed( vlc_value_t newVal );
    void on_random_changed( vlc_value_t newVal );
    void on_loop_changed( vlc_value_t newVal );
    void on_repeat_changed( vlc_value_t newVal );
    void on_order_changed( vlc_value_t newVal );

    void on_volume_changed( vlc_value_t newVal );
    void on_mute_changed( vlc_value_t newVal );

    void on_state_changed( vlc_value_t newVal );
    void on_rate_changed( vlc_value_t newVal );
    void on_capabilities_changed( vlc_value_t newVal );
    void on_position_changed( vlc_value_t newVal );
    void on_audio_es_changed( vlc_value_t newVal );
    void on_isDvd_changed( vlc_value_t newVal );
    void on_recording_changed( vlc_value_t newVal );
    void on_vout_changed( vlc_value_t newVal );

    void on_audio_filter_changed( vlc_value_t newVal );

    void on_intf_show_changed( vlc_value_t newVal );

    void on_mouse_moved_changed( vlc_value_t newVal );

protected:
    // Protected because it is a singleton
    VlcProc( intf_thread_t *pIntf );
    virtual ~VlcProc();

private:
    /// Playtree variable
    VariablePtr m_cPlaytree;
    VariablePtr m_cVarRandom;
    VariablePtr m_cVarLoop;
    VariablePtr m_cVarRepeat;
    /// Variable for current position of the stream
    VariablePtr m_cVarTime;
    /// Variable for audio volume
    VariablePtr m_cVarVolume;
    /// Variable for speed playback
    VariablePtr m_cVarSpeed;
    /// Variable for current stream properties
    VariablePtr m_cVarStreamName;
    VariablePtr m_cVarStreamURI;
    VariablePtr m_cVarStreamBitRate;
    VariablePtr m_cVarStreamSampleRate;
    VariablePtr m_cVarStreamArt;
    /// Variable for the "mute" state
    VariablePtr m_cVarMute;
    /// Variables related to the input
    VariablePtr m_cVarPlaying;
    VariablePtr m_cVarStopped;
    VariablePtr m_cVarPaused;
    VariablePtr m_cVarSeekable;
    VariablePtr m_cVarRecordable;
    VariablePtr m_cVarRecording;
    /// Variables related to the vout
    VariablePtr m_cVarFullscreen;
    VariablePtr m_cVarHasVout;
    /// Variables related to audio
    VariablePtr m_cVarHasAudio;
    /// Equalizer variables
    EqualizerBands m_varEqBands;
    VariablePtr m_cVarEqPreamp;
    VariablePtr m_cVarEqualizer;
    /// Variable for DVD detection
    VariablePtr m_cVarDvdActive;

    /// Vout thread
    vout_thread_t *m_pVout;
    audio_output_t *m_pAout;

    /// Listener to various objects
    vlc_playlist_listener_id *mPlaylistListenerId;
    vlc_player_listener_id *mPlayerListenerId;
    vlc_player_aout_listener_id *mPlayerAoutListenerId;
    vlc_player_vout_listener_id *mPlayerVoutListenerId;

    // reset variables when input is over
    void reset_input();

    // init variables (libvlc and playlist levels)
    void init_variables();

public:

    static int genericCallback( vlc_object_t *pObj,
                                const char *pVariable,
                                vlc_value_t oldVal, vlc_value_t newVal,
                                void *pParam );

    static int EqBandsCallback( vlc_object_t *pObj, const char *pVariable,
                                vlc_value_t oldVal, vlc_value_t newVal,
                                void *pParam );

    static int EqPreampCallback( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );


    static void onGenericCallback( const char *pVariable,
                                   vlc_value_t newVal, void *data );

};

#endif
