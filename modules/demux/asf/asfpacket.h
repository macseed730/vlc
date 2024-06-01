/*****************************************************************************
 * asfpacket.h :
 *****************************************************************************
 * Copyright © 2001-2004, 2011, 2014 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#ifndef VLC_ASF_ASFPACKET_H_
#define VLC_ASF_ASFPACKET_H_

#include <vlc_demux.h>
#include <vlc_es.h>
#include <vlc_messages.h>
#include "libasf.h"

#define ASFPACKET_PREROLL_FROM_CURRENT -1
#define ASFPACKET_DEDUPLICATE 8

typedef struct
{
    block_t *p_frame; /* used to gather complete frame */
    asf_object_stream_properties_t *p_sp;
    asf_object_extended_stream_properties_t *p_esp;
    int i_cat;
    struct
    {
        unsigned media_number;
        unsigned media_offset;
    } prev[ASFPACKET_DEDUPLICATE];
    unsigned i_pkt;
    unsigned i_pktcount;
} asf_track_info_t;

typedef struct asf_packet_sys_s asf_packet_sys_t;

struct asf_packet_sys_s
{
    void *priv;
    stream_t *s;
    struct vlc_logger *logger;

    /* global stream info */
    vlc_tick_t *pi_preroll;
    vlc_tick_t *pi_preroll_start;
    bool b_deduplicate; /* Flip4mac repeats data object payloads */
    bool b_can_hold_multiple_packets; /* Flip4mac passes multiple buffers */

    /* callbacks */
    void (*pf_send)(asf_packet_sys_t *, uint8_t, block_t **);
    asf_track_info_t * (*pf_gettrackinfo)(asf_packet_sys_t *, uint8_t);

    /* optional callbacks */
    bool (*pf_doskip)(asf_packet_sys_t *, uint8_t, bool);
    void (*pf_updatesendtime)(asf_packet_sys_t *, vlc_tick_t);
    void (*pf_updatetime)(asf_packet_sys_t *, uint8_t, vlc_tick_t);
    void (*pf_setaspectratio)(asf_packet_sys_t *, uint8_t, uint8_t, uint8_t);
};

int DemuxASFPacket( asf_packet_sys_t *, uint32_t, uint32_t, uint64_t, uint64_t );
void ASFPacketTrackInit( asf_track_info_t * );
void ASFPacketTrackReset( asf_track_info_t * );
#endif
