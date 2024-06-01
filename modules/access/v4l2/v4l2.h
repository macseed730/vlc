/*****************************************************************************
 * v4l2.h : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
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

#if defined(HAVE_LINUX_VIDEODEV2_H)
#   include <linux/videodev2.h>
#elif defined(HAVE_SYS_VIDEOIO_H)
#   include <sys/ioccom.h>
#   include <sys/videoio.h>
#endif

/* libv4l2 functions */
extern int v4l2_fd_open(int, int);
extern int (*v4l2_close) (int);
extern int (*v4l2_ioctl) (int, unsigned long int, ...);
extern ssize_t (*v4l2_read) (int, void *, size_t);
extern void * (*v4l2_mmap) (void *, size_t, int, int, int, int64_t);
extern int (*v4l2_munmap) (void *, size_t);

#define CFG_PREFIX "v4l2-"

typedef struct vlc_v4l2_ctrl vlc_v4l2_ctrl_t;

#include <vlc_atomic.h>
#include <vlc_block.h>

struct vlc_v4l2_buffer {
    block_t block;
    struct vlc_v4l2_buffers *pool;
    uint32_t index;
};

struct vlc_v4l2_buffers {
    size_t count;
    struct vlc_v4l2_buffer **bufs;

    int fd;
    vlc_atomic_rc_t refs;
    _Atomic size_t unused;
    vlc_mutex_t lock;
};

/* v4l2.c */
void ParseMRL(vlc_object_t *, const char *);
int OpenDevice (vlc_object_t *, const char *, uint32_t *);
v4l2_std_id var_InheritStandard (vlc_object_t *, const char *);

/* video.c */
int SetupTuner (vlc_object_t *, int fd, uint32_t);
int SetupVideo(vlc_object_t *, int fd, uint32_t,
               es_format_t *, uint32_t *, uint32_t *);

struct vlc_v4l2_buffers *StartMmap(vlc_object_t *, int);
void StopMmap(struct vlc_v4l2_buffers *);

vlc_tick_t GetBufferPTS (const struct v4l2_buffer *);
block_t* GrabVideo(vlc_object_t *, struct vlc_v4l2_buffers *);

#ifdef ZVBI_COMPILED
/* vbi.c */
typedef struct vlc_v4l2_vbi vlc_v4l2_vbi_t;

vlc_v4l2_vbi_t *OpenVBI (demux_t *, const char *);
int GetFdVBI (vlc_v4l2_vbi_t *);
void GrabVBI (demux_t *p_demux, vlc_v4l2_vbi_t *);
void CloseVBI (vlc_v4l2_vbi_t *);
#endif

/* demux.c */
int DemuxOpen(vlc_object_t *);
void DemuxClose(vlc_object_t *);
float GetAbsoluteMaxFrameRate(vlc_object_t *, int fd, uint32_t fmt);
void GetMaxDimensions(vlc_object_t *, int fd, uint32_t fmt, float fps_min,
                      uint32_t *pwidth, uint32_t *pheight);

/* access.c */
int AccessOpen(vlc_object_t *);
void AccessClose(vlc_object_t *);

/* radio.c */
int RadioOpen(vlc_object_t *);
void RadioClose(vlc_object_t *);

/* controls.c */
vlc_v4l2_ctrl_t *ControlsInit(vlc_object_t *, int fd);
void ControlsDeinit(vlc_object_t *, vlc_v4l2_ctrl_t *);
