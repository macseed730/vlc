/*****************************************************************************
 * filesystem.c: Windows file system helpers
 *****************************************************************************
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
 * Copyright © 2005-2008 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <winsock2.h>
#include <direct.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include "libvlc.h" /* vlc_mkdir */

#ifndef NTDDI_WIN10_RS3
#define NTDDI_WIN10_RS3  0x0A000004
#endif

static wchar_t *widen_path (const char *path)
{
    wchar_t *wpath;

    errno = 0;
    wpath = ToWide (path);
    if (wpath == NULL)
    {
        if (errno == 0)
            errno = ENOENT;
        return NULL;
    }
    return wpath;
}

int vlc_open (const char *filename, int flags, ...)
{
    int mode = 0;
    va_list ap;

    flags |= O_NOINHERIT; /* O_CLOEXEC */
    /* Defaults to binary mode */
    if ((flags & O_TEXT) == 0)
        flags |= O_BINARY;

    va_start (ap, flags);
    if (flags & O_CREAT)
    {
        int unixmode = va_arg(ap, int);
        if (unixmode & 0444)
            mode |= _S_IREAD;
        if (unixmode & 0222)
            mode |= _S_IWRITE;
    }
    va_end (ap);

    /*
     * open() cannot open files with non-“ANSI” characters on Windows.
     * We use _wopen() instead. Same thing for mkdir() and stat().
     */
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    int fd = _wopen (wpath, flags, mode);
    free (wpath);
    return fd;
}

int vlc_openat (int dir, const char *filename, int flags, ...)
{
    (void) dir; (void) filename; (void) flags;
    errno = ENOSYS;
    return -1;
}

int vlc_memfd (void)
{
#if 0
    int fd, err;

    FILE *stream = tmpfile();
    if (stream == NULL)
        return -1;

    fd = vlc_dup(fileno(stream));
    err = errno;
    fclose(stream);
    errno = err;
    return fd;
#else /* Not currently used */
    errno = ENOSYS;
    return -1;
#endif
}

int vlc_close (int fd)
{
    return close (fd);
}

int vlc_mkdir( const char *dirname, mode_t mode )
{
    wchar_t *wpath = widen_path (dirname);
    if (wpath == NULL)
        return -1;

    int ret = _wmkdir (wpath);
    free (wpath);
    (void) mode;
    return ret;
}

char *vlc_getcwd (void)
{
#ifdef VLC_WINSTORE_APP
    return NULL;
#else
    wchar_t *wdir = _wgetcwd (NULL, 0);
    if (wdir == NULL)
        return NULL;

    char *dir = FromWide (wdir);
    free (wdir);
    return dir;
#endif
}

struct vlc_DIR
{
    wchar_t *wildcard;
    HANDLE fHandle;
    WIN32_FIND_DATAW wdir;
    bool eol;

    char *entry;
    union
    {
        DWORD drives;
        bool insert_dot_dot;
    } u;
};

/* Under Windows, these wrappers return the list of drive letters
 * when called with an empty argument or just '\'. */
vlc_DIR *vlc_opendir (const char *dirname)
{
    vlc_DIR *p_dir = malloc (sizeof (*p_dir));
    if (unlikely(p_dir == NULL))
        return NULL;
    p_dir->entry = NULL;
    p_dir->eol = false;
    p_dir->fHandle = INVALID_HANDLE_VALUE;
    p_dir->wildcard = NULL;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) || NTDDI_VERSION >= NTDDI_WIN10_RS3
    /* Special mode to list drive letters */
    if (dirname[0] == '\0' || (strcmp (dirname, "\\") == 0))
    {
        p_dir->u.drives = GetLogicalDrives ();
        if (unlikely(p_dir->u.drives == 0))
        {
            free(p_dir);
            return NULL;
        }
        p_dir->entry = strdup("C:\\");
        if (unlikely(p_dir->entry == NULL))
        {
            free(p_dir);
            return NULL;
        }
        return p_dir;
    }
#endif

    assert (dirname[0]); // dirname[1] is defined
    p_dir->u.insert_dot_dot = !strcmp (dirname + 1, ":\\");

    char *wildcard;
    const size_t len = strlen(dirname);
    if (p_dir->u.insert_dot_dot || strncmp(dirname + 1, ":\\", 2) != 0)
    {
        // Prepending the string "\\?\" does not allow access to the root directory.
        // Don't use long path with relative pathes.
        wildcard = malloc(len + 3);
        if (unlikely(wildcard == NULL))
        {
            free (p_dir);
            return NULL;
        }
        memcpy(wildcard, dirname, len);
        size_t j = len;
        wildcard[j++] = '\\';
        wildcard[j++] = '*';
        wildcard[j++] = '\0';
    }
    else
    {
        wildcard = malloc(4 + len + 3);
        if (unlikely(wildcard == NULL))
        {
            free (p_dir);
            return NULL;
        }

        // prepend "\\?\"
        wildcard[0] = '\\';
        wildcard[1] = '\\';
        wildcard[2] = '?';
        wildcard[3] = '\\';
        size_t j = 4;
        for (size_t i=0; i<len;i++)
        {
            // remove forward slashes from long pathes to please FindFirstFileExW
            if (unlikely(dirname[i] == '/'))
                wildcard[j++] = '\\';
            else
                wildcard[j++] = dirname[i];
        }
        // append "\*" or "*"
        if (wildcard[j-1] != '\\')
            wildcard[j++] = '\\';
        wildcard[j++] = '*';
        wildcard[j++] = '\0';
    }
    p_dir->wildcard = ToWide(wildcard);
    free(wildcard);
    if (unlikely(p_dir->wildcard == NULL))
    {
        free (p_dir);
        return NULL;
    }

    p_dir->fHandle = FindFirstFileExW(p_dir->wildcard, FindExInfoBasic,
                                      &p_dir->wdir, (FINDEX_SEARCH_OPS)0,
                                      NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (p_dir->fHandle == INVALID_HANDLE_VALUE)
    {
        free(p_dir->wildcard);
        free(p_dir);
        return NULL;
    }
    return p_dir;
}

void vlc_closedir( vlc_DIR *vdir )
{
    if (vdir->fHandle != INVALID_HANDLE_VALUE)
        FindClose(vdir->fHandle);

    free( vdir->entry );
    free( vdir->wildcard );
    free( vdir );
}

const char *vlc_readdir (vlc_DIR *p_dir)
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) || NTDDI_VERSION >= NTDDI_WIN10_RS3
    /* Drive letters mode */
    if (p_dir->fHandle ==  INVALID_HANDLE_VALUE)
    {
        DWORD drives = p_dir->u.drives;
        if (drives == 0)
        {
            return NULL; /* end */
        }

        unsigned int i;
        for (i = 0; !(drives & 1); i++)
            drives >>= 1;
        p_dir->u.drives &= ~(1UL << i);
        assert (i < 26);

        p_dir->entry[0] = 'A' + i;
        return p_dir->entry;
    }
#endif

    free(p_dir->entry);
    if (p_dir->u.insert_dot_dot)
    {
        /* Adds "..", gruik! */
        p_dir->u.insert_dot_dot = false;
        p_dir->entry = strdup ("..");
    }
    else if (p_dir->eol)
        p_dir->entry = NULL;
    else
    {
        p_dir->entry = FromWide(p_dir->wdir.cFileName);
        p_dir->eol = !FindNextFileW(p_dir->fHandle, &p_dir->wdir);
    }
    return p_dir->entry;
}

void vlc_rewinddir( vlc_DIR *wdir )
{
    if (wdir->fHandle == INVALID_HANDLE_VALUE)
    {
        FindClose(wdir->fHandle);
        wdir->fHandle = FindFirstFileExW(wdir->wildcard, FindExInfoBasic,
                                         &wdir->wdir, (FINDEX_SEARCH_OPS)0,
                                         NULL, FIND_FIRST_EX_LARGE_FETCH);
    }
    else
    {
        wdir->u.drives = GetLogicalDrives();
    }
}

int vlc_stat (const char *filename, struct stat *buf)
{
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    static_assert (sizeof (*buf) == sizeof (struct _stati64),
                   "Mismatched struct stat definition.");

    int ret = _wstati64 (wpath, buf);
    free (wpath);
    return ret;
}

int vlc_lstat (const char *filename, struct stat *buf)
{
    return vlc_stat (filename, buf);
}

int vlc_unlink (const char *filename)
{
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    int ret = _wunlink (wpath);
    free (wpath);
    return ret;
}

int vlc_rename (const char *oldpath, const char *newpath)
{
    int ret = -1;

    wchar_t *wold = widen_path (oldpath), *wnew = widen_path (newpath);
    if (wold == NULL || wnew == NULL)
        goto out;

    if (_wrename (wold, wnew) && (errno == EACCES || errno == EEXIST))
    {   /* Windows does not allow atomic file replacement */
        if (_wremove (wnew))
        {
            errno = EACCES; /* restore errno */
            goto out;
        }
        if (_wrename (wold, wnew))
            goto out;
    }
    ret = 0;
out:
    free (wnew);
    free (wold);
    return ret;
}

int vlc_dup (int oldfd)
{
    int fd = dup (oldfd);
    if (fd != -1)
        setmode (fd, O_BINARY);
    return fd;
}

int vlc_dup2(int oldfd, int newfd)
{
    int fd = dup2(oldfd, newfd);
    if (fd != -1)
        setmode(fd, O_BINARY);
    return fd;
}

int vlc_pipe (int fds[2])
{
#if (defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 8)
    // old mingw doesn't know about _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
# if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    return _pipe (fds, 32768, O_NOINHERIT | O_BINARY);
# else
    _set_errno(EPERM);
    return -1;
# endif
#elif defined(_CRT_USE_WINAPI_FAMILY_DESKTOP_APP)
    return _pipe (fds, 32768, O_NOINHERIT | O_BINARY);
#else
    _set_errno(EPERM);
    return -1;
#endif
}

ssize_t vlc_write(int fd, const void *buf, size_t len)
{
    return write(fd, buf, len);
}

ssize_t vlc_writev(int fd, const struct iovec *iov, int count)
{
    return writev(fd, iov, count);
}

#include <vlc_network.h>

int vlc_socket (int pf, int type, int proto, bool nonblock)
{
    int fd = socket (pf, type, proto);
    if (fd == -1)
        return -1;

    if (nonblock)
        ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
    return fd;
}

int vlc_socketpair(int pf, int type, int proto, int fds[2], bool nonblock)
{
    (void) pf; (void) type; (void) proto; (void) fds; (void) nonblock;
    errno = ENOSYS;
    return -1;
}

int vlc_accept (int lfd, struct sockaddr *addr, socklen_t *alen, bool nonblock)
{
    int fd = accept (lfd, addr, alen);
    if (fd != -1 && nonblock)
        ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
    else if (fd < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        errno = EAGAIN;
    return fd;
}

ssize_t vlc_send(int fd, const void *buf, size_t len, int flags)
{
    WSABUF wsabuf = { .buf = (char *)buf, .len = len };
    DWORD sent;

    return WSASend(fd, &wsabuf, 1, &sent, flags,
                   NULL, NULL) ? -1 : (ssize_t)sent;
}

ssize_t vlc_sendto(int fd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dst, socklen_t dstlen)
{
    WSABUF wsabuf = { .buf = (char *)buf, .len = len };
    DWORD sent;

    return WSASendTo(fd, &wsabuf, 1, &sent, flags, dst, dstlen,
                     NULL, NULL) ? -1 : (ssize_t)sent;
}

ssize_t vlc_sendmsg(int fd, const struct msghdr *msg, int flags)
{
    return sendmsg(fd, msg, flags);
}
