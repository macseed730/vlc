/*****************************************************************************
 * mmdevice.h : Windows Multimedia Device API audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_AOUT_MMDEVICE_H
# define VLC_AOUT_MMDEVICE_H 1

#define MM_PASSTHROUGH_DISABLED 0
#define MM_PASSTHROUGH_ENABLED 1
#define MM_PASSTHROUGH_ENABLED_HD 2
#define MM_PASSTHROUGH_DEFAULT MM_PASSTHROUGH_DISABLED

typedef struct aout_stream aout_stream_t;

/**
 * Audio output simplified API for Windows
 */
struct aout_stream
{
    struct vlc_object_t obj;
    void *sys;

    void (*stop)(aout_stream_t *);
    HRESULT (*play)(aout_stream_t *, block_t *, vlc_tick_t);
    HRESULT (*pause)(aout_stream_t *, bool);
    HRESULT (*flush)(aout_stream_t *);
};

struct aout_stream_owner
{
    aout_stream_t s;
    void *device;
    HRESULT (*activate)(void *device, REFIID, PROPVARIANT *, void **);
    HANDLE buffer_ready_event;
    struct {
        vlc_tick_t deadline;
        void (*callback)(aout_stream_t *);
    } timer;

    block_t *chain;
    block_t **last;

    audio_output_t *aout;
};

/*
 * "aout output" helpers
 */

static inline
struct aout_stream_owner *aout_stream_owner(aout_stream_t *s)
{
    return container_of(s, struct aout_stream_owner, s);
}

/**
 * Creates an audio output stream on a given Windows multimedia device.
 * \param s audio output stream object to be initialized
 * \param fmt audio output sample format [IN/OUT]
 * \param sid audio output session GUID [IN]
 */
typedef HRESULT (*aout_stream_start_t)(aout_stream_t *s,
    audio_sample_format_t *fmt, const GUID *sid);

/**
 * Destroys an audio output stream.
 */
static inline
void aout_stream_owner_Stop(struct aout_stream_owner *owner)
{
    owner->s.stop(&owner->s);
}

static inline
HRESULT aout_stream_owner_Play(struct aout_stream_owner *owner,
                               block_t *block, vlc_tick_t date)
{
    return owner->s.play(&owner->s, block, date);
}

static inline
HRESULT aout_stream_owner_Pause(struct aout_stream_owner *owner, bool paused)
{
    return owner->s.pause(&owner->s, paused);
}

static inline
HRESULT aout_stream_owner_Flush(struct aout_stream_owner *owner)
{
    block_ChainRelease(owner->chain);
    owner->chain = NULL;
    owner->last = &owner->chain;

    return owner->s.flush(&owner->s);
}

static inline
void aout_stream_owner_AppendBlock(struct aout_stream_owner *owner,
                                   block_t *block, vlc_tick_t date)
{
    block->i_dts = date;
    block_ChainLastAppend(&owner->last, block);
}

static inline
HRESULT aout_stream_owner_PlayAll(struct aout_stream_owner *owner)
{
    HRESULT hr;

    block_t *block = owner->chain, *next;
    while (block != NULL)
    {
        next = block->p_next;

        vlc_tick_t date = block->i_dts;
        block->i_dts = VLC_TICK_INVALID;

        hr = aout_stream_owner_Play(owner, block, date);

        if (hr == S_FALSE)
            return hr;
        else
        {
            block = owner->chain = next;
            if (FAILED(hr))
            {
                if (block == NULL)
                    owner->last = &owner->chain;
                return hr;
            }
        }
    }
    owner->last = &owner->chain;

    return S_OK;
}

static inline
DWORD aout_stream_owner_ProcessTimer(struct aout_stream_owner *owner)
{
    if (owner->timer.deadline != VLC_TICK_INVALID)
    {
        vlc_tick_t now = vlc_tick_now();
        /* Subtract 1 ms since WaitForMultipleObjects will likely sleep
         * a little less than requested */
        if (now >= owner->timer.deadline - VLC_TICK_FROM_MS(1))
        {
            assert(owner->timer.callback != NULL);
            owner->timer.deadline = VLC_TICK_INVALID;
            owner->timer.callback(&owner->s);

            /* timer.deadline might be updated from timer.callback */
            if (owner->timer.deadline != VLC_TICK_INVALID)
            {
                now = vlc_tick_now();
                return MS_FROM_VLC_TICK(owner->timer.deadline - now);
            }
        }
        else
            return MS_FROM_VLC_TICK(owner->timer.deadline - now);
    }

    return INFINITE;
}

static inline
void *aout_stream_owner_New(audio_output_t *aout, size_t size,
                            HRESULT (*activate)(void *device, REFIID, PROPVARIANT *, void **))
{
    assert(size >= sizeof(struct aout_stream_owner));

    void *obj = vlc_object_create(aout, size);
    if (unlikely(obj == NULL))
        return NULL;
    struct aout_stream_owner *owner = obj;

    owner->aout = aout;
    owner->chain = NULL;
    owner->last = &owner->chain;
    owner->activate = activate;
    owner->timer.deadline = VLC_TICK_INVALID;
    owner->timer.callback = NULL;

    owner->buffer_ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (unlikely(owner->buffer_ready_event == NULL))
    {
        vlc_object_delete(&owner->s);
        return NULL;
    }

    return obj;
}

static inline
void aout_stream_owner_Delete(struct aout_stream_owner *owner)
{
    CloseHandle(owner->buffer_ready_event);
    vlc_object_delete(&owner->s);
}

/*
 * "aout stream" helpers
 */

static inline
void aout_stream_TimingReport(aout_stream_t *s, vlc_tick_t system_ts,
                              vlc_tick_t audio_ts)
{
    struct aout_stream_owner *owner = aout_stream_owner(s);
    aout_TimingReport(owner->aout, system_ts, audio_ts);
}

static inline
HRESULT aout_stream_Activate(aout_stream_t *s, REFIID iid,
                             PROPVARIANT *actparms, void **pv)
{
    struct aout_stream_owner *owner = aout_stream_owner(s);
    return owner->activate(owner->device, iid, actparms, pv);
}

static inline
HANDLE aout_stream_GetBufferReadyEvent(aout_stream_t *s)
{
    struct aout_stream_owner *owner = aout_stream_owner(s);
    return owner->buffer_ready_event;
}

static inline
void aout_stream_TriggerTimer(aout_stream_t *s,
                              void (*callback)(aout_stream_t *),
                              vlc_tick_t deadline)
{
    struct aout_stream_owner *owner = aout_stream_owner(s);
    owner->timer.deadline = deadline;
    owner->timer.callback = callback;
}

static inline
void aout_stream_DisarmTimer(aout_stream_t *s)
{
    struct aout_stream_owner *owner = aout_stream_owner(s);
    owner->timer.deadline = VLC_TICK_INVALID;
    owner->timer.callback = NULL;
}

#endif
