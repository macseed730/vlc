/*****************************************************************************
 * charset.c: Locale's character encoding stuff.
 *****************************************************************************
 * See also unicode.c for Unicode to locale conversion helpers.
 *
 * Copyright (C) 2003-2008 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot
 *          Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#if !defined _WIN32
# include <locale.h>
#else
# include <windows.h>
#endif

#ifdef __APPLE__
#   include <string.h>
#   include <xlocale.h>
#endif

#include "libvlc.h"
#include <vlc_charset.h>

double vlc_strtod_c(const char *restrict str, char **restrict end)
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    double res = strtod (str, end);

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return res;
}

float vlc_strtof_c(const char *restrict str, char **restrict end)
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    float res = strtof (str, end);

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return res;
}

int vlc_vasprintf_c(char **restrict ret, const char *restrict format,
                    va_list ap)
{
    locale_t loc = newlocale( LC_NUMERIC_MASK, "C", NULL );
    locale_t oldloc = uselocale( loc );

    int i_rc = vasprintf( ret, format, ap );

    if ( loc != (locale_t)0 )
    {
        uselocale( oldloc );
        freelocale( loc );
    }

    return i_rc;
}

int vlc_asprintf_c(char **restrict ret, const char *restrict format, ...)
{
    va_list ap;
    int i_rc;

    va_start( ap, format );
    i_rc = vlc_vasprintf_c(ret, format, ap);
    va_end( ap );

    return i_rc;
}

int vlc_vsscanf_c(const char *restrict buf, const char *restrict format,
                  va_list ap)
{
    locale_t loc = newlocale(LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale(loc);
    int ret = vsscanf(buf, format, ap);

    if (loc != (locale_t)0)
    {
        uselocale(oldloc);
        freelocale(loc);
    }

    return ret;
}

int vlc_sscanf_c(const char *restrict buf, const char *restrict format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vlc_vsscanf_c(buf, format, ap);
    va_end( ap );

    return ret;
}
