/*****************************************************************************
 * subtitles.c : subtitles detection
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 * This is adapted code from the GPL'ed MPlayer (http://mplayerhq.hu)
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

/**
 *  \file
 *  This file contains functions to detect subtitle files.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h> /* isalnum() */
#include <unistd.h>
#include <sys/stat.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_url.h>

#include "input_internal.h"

/**
 * The possible extensions for subtitle files we support
 */
static const char *const sub_exts[] = { SLAVE_SPU_EXTENSIONS, "" };

/**
 * Remove file extension in-place
 */
static void filename_strip_ext_inplace(char *str)
{
    char *tmp = strrchr(str, '.');
    if (tmp)
        *tmp = '\0';
}

/**
 * Trim special characters from a filename
 *
 * Trims whitespaces and other non-alphanumeric
 * characters from filenames.
 *
 * \warning This function operates on the passed string
 * without copying. It might return a pointer different to
 * the passed one, in case it trims characters at the beginning.
 * Therefore it is essential that the return value is used where
 * the trimmed version of the string is needed and the returned
 * pointer must not be free()d but rather the original pointer!
 */
VLC_USED static char *filename_trim_inplace(char *str)
{
    char *ret = str;
    unsigned char c;

    // Trim leading non-alnum
    while( (c = *str) != '\0' && !isalnum(c) )
        str++;
    ret = str;

    // Trim inline nonalnum groups
    char *writehead = str;
    bool consecutive = false;
    while( (c = *str) != '\0' )
    {
        if ( !isalnum(c) )
        {
            if ( consecutive )
            {
                str++;
                continue;
            }

            c = ' ';
            consecutive = true;
        } else {
            consecutive = false;
        }

        *writehead++ = tolower(c);
        str++;
    }
    *writehead = '\0';

    // Remove trailing space, if any
    if ( consecutive )
        *(writehead - 1) = '\0';

    return ret;
}

static int whiteonly( const char *s )
{
    unsigned char c;

    while( (c = *s) != '\0' )
    {
        if( isalnum( c ) )
            return 0;
        s++;
    }
    return 1;
}

static int slave_strcmp( const void *a, const void *b )
{
    const input_item_slave_t *p_slave0 = *((const input_item_slave_t **) a);
    const input_item_slave_t *p_slave1 = *((const input_item_slave_t **) b);

    if( p_slave0 == NULL || p_slave1 == NULL )
        return 0;

    /* We can compare these uris since they come from the file system */
#ifdef HAVE_STRCOLL
    return strcoll( p_slave0->psz_uri, p_slave1->psz_uri );
#else
    return strcmp( p_slave0->psz_uri, p_slave1->psz_uri );
#endif
}

/*
 * Check if a file ends with a subtitle extension
 */
int subtitles_Filter( const char *psz_dir_content )
{
    const char *tmp = strrchr( psz_dir_content, '.');

    if( !tmp )
        return 0;
    tmp++;

    for( int i = 0; sub_exts[i][0]; i++ )
        if( strcasecmp( sub_exts[i], tmp ) == 0 )
            return 1;
    return 0;
}


/**
 * Convert a list of paths separated by ',' to a char**
 */
static char **paths_to_list( const char *psz_dir, char *psz_path )
{
    unsigned int i, k, i_nb_subdirs;
    char **subdirs; /* list of subdirectories to look in */
    char *psz_parser = psz_path;

    if( !psz_dir || !psz_path )
        return NULL;

    for( k = 0, i_nb_subdirs = 1; psz_path[k] != '\0'; k++ )
    {
        if( psz_path[k] == ',' )
            i_nb_subdirs++;
    }

    subdirs = calloc( i_nb_subdirs + 1, sizeof(char*) );
    if( !subdirs )
        return NULL;

    for( i = 0; psz_parser && *psz_parser != '\0' ; )
    {
        char *psz_subdir = psz_parser;
        psz_parser = strchr( psz_subdir, ',' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            while( *psz_parser == ' ' )
                psz_parser++;
        }

        if( asprintf( &subdirs[i], "%s%s",
                  psz_subdir[0] == '.' ? psz_dir : "",
                  psz_subdir ) == -1 )
            break;
        i++;
    }
    subdirs[i] = NULL;

    return subdirs;
}

/**
 * Detect subtitle files.
 *
 * When called this function will split up the psz_name string into a
 * directory, filename and extension. It then opens the directory
 * in which the file resides and tries to find possible matches of
 * subtitles files.
 *
 * \ingroup Demux
 * \param p_this the calling \ref input_thread_t
 * \param psz_path a list of subdirectories (separated by a ',') to look in.
 * \param psz_name_org the complete filename to base the search on.
 * \param pp_slaves an initialized input item slave list to append detected subtitles to
 * \param p_slaves pointer to the size of the slave list
 * \return VLC_SUCCESS if ok
 */
int subtitles_Detect( input_thread_t *p_this, char *psz_path, const char *psz_name_org,
                      input_item_slave_t ***ppp_slaves, int *p_slaves )
{
    int i_fuzzy = var_GetInteger( p_this, "sub-autodetect-fuzzy" );
    if ( i_fuzzy == 0 )
        return VLC_EGENERIC;
    input_item_slave_t **pp_slaves = *ppp_slaves;
    int i_slaves = *p_slaves;
    char **subdirs; /* list of subdirectories to look in */

    if( !psz_name_org )
        return VLC_EGENERIC;

    char *psz_fname = vlc_uri2path( psz_name_org );
    if( !psz_fname )
        return VLC_EGENERIC;

    /* extract filename & dirname from psz_fname */
    char *f_dir = strdup( psz_fname );
    if( f_dir == 0 )
    {
        free( psz_fname );
        return VLC_ENOMEM;
    }

    char *f_fname_trim = strrchr( psz_fname, DIR_SEP_CHAR );
    if( !f_fname_trim )
    {
        free( f_dir );
        free( psz_fname );
        return VLC_EGENERIC;
    }
    f_fname_trim++; /* Skip the '/' */
    f_dir[f_fname_trim - psz_fname] = 0; /* keep dir separator in f_dir */

    filename_strip_ext_inplace(f_fname_trim);
    f_fname_trim = filename_trim_inplace(f_fname_trim);

    subdirs = paths_to_list( f_dir, psz_path );
    for( int j = -1; (j == -1) || ( j >= 0 && subdirs != NULL && subdirs[j] != NULL ); j++ )
    {
        const char *psz_dir = (j < 0) ? f_dir : subdirs[j];
        if( psz_dir == NULL || ( j >= 0 && !strcmp( psz_dir, f_dir ) ) )
            continue;

        /* parse psz_src dir */
        vlc_DIR *dir = vlc_opendir( psz_dir );
        if( dir == NULL )
            continue;

        msg_Dbg( p_this, "looking for a subtitle file in %s", psz_dir );

        const char *psz_name;
        while( (psz_name = vlc_readdir( dir )) )
        {
            if( psz_name[0] == '.' || !subtitles_Filter( psz_name ) )
                continue;

            char *tmp_fname = strdup(psz_name);
            if (!tmp_fname)
                break;

            const char *tmp;
            int i_prio = 0;

            /* retrieve various parts of the filename */
            filename_strip_ext_inplace(tmp_fname);
            char *tmp_fname_trim = filename_trim_inplace(tmp_fname);

            if( !strcmp( tmp_fname_trim, f_fname_trim ) )
            {
                /* matches the movie name exactly */
                i_prio = SLAVE_PRIORITY_MATCH_ALL;
            }
            else if( (tmp = strstr( tmp_fname_trim, f_fname_trim )) )
            {
                /* contains the movie name */
                tmp += strlen( f_fname_trim );
                if( whiteonly( tmp ) )
                {
                    /* chars in front of the movie name */
                    i_prio = SLAVE_PRIORITY_MATCH_RIGHT;
                }
                else
                {
                    /* chars after (and possibly in front of)
                     * the movie name */
                    i_prio = SLAVE_PRIORITY_MATCH_LEFT;
                }
            }
            else if( j == -1 )
            {
                /* doesn't contain the movie name, prefer files in f_dir over subdirs */
                i_prio = SLAVE_PRIORITY_MATCH_NONE;
            }
            free(tmp_fname);
            tmp_fname_trim = NULL;

            if( i_prio >= i_fuzzy )
            {
                struct stat st;
                char *path;

                size_t i_len = strlen( psz_dir );
                const char *psz_format;
                if ( i_len == 0 )
                    continue;
                if( psz_dir[i_len - 1] == DIR_SEP_CHAR )
                    psz_format = "%s%s";
                else
                    psz_format = "%s"DIR_SEP"%s";

                if( asprintf( &path, psz_format, psz_dir, psz_name ) < 0 )
                    continue;

                if( strcmp( path, psz_fname )
                 && vlc_stat( path, &st ) == 0
                 && S_ISREG( st.st_mode ) )
                {
                    msg_Dbg( p_this,
                            "autodetected subtitle: %s with priority %d",
                            path, i_prio );
                    char *psz_uri = vlc_path2uri( path, NULL );
                    input_item_slave_t *p_sub = psz_uri != NULL ?
                        input_item_slave_New( psz_uri, SLAVE_TYPE_SPU, i_prio )
                        : NULL;
                    if( p_sub )
                    {
                        p_sub->b_forced = true;
                        TAB_APPEND(i_slaves, pp_slaves, p_sub);
                    }
                    free( psz_uri );
                }
                free( path );
            }
        }
        vlc_closedir( dir );
    }
    if( subdirs )
    {
        for( size_t j = 0; subdirs[j] != NULL; j++ )
            free( subdirs[j] );
        free( subdirs );
    }
    free( f_dir );
    free( psz_fname );

    for( int i = 0; i < i_slaves; i++ )
    {
        input_item_slave_t *p_sub = pp_slaves[i];

        bool b_reject = false;
        char *psz_ext = strrchr( p_sub->psz_uri, '.' );
        if( !psz_ext )
            continue;
        psz_ext++;

        if( !strcasecmp( psz_ext, "sub" ) )
        {
            for( int j = 0; j < i_slaves; j++ )
            {
                input_item_slave_t *p_sub_inner = pp_slaves[j];

                /* A slave can be null if it's already rejected */
                if( p_sub_inner == NULL )
                    continue;

                /* check that the filenames without extension match */
                if( strncasecmp( p_sub->psz_uri, p_sub_inner->psz_uri,
                    strlen( p_sub->psz_uri ) - 3 ) )
                    continue;

                char *psz_ext_inner = strrchr( p_sub_inner->psz_uri, '.' );
                if( !psz_ext_inner )
                    continue;
                psz_ext_inner++;

                /* check that we have an idx file */
                if( !strcasecmp( psz_ext_inner, "idx" ) )
                {
                    b_reject = true;
                    break;
                }
            }
        }
        else if( !strcasecmp( psz_ext, "cdg" ) )
        {
            if( p_sub->i_priority < SLAVE_PRIORITY_MATCH_ALL )
                b_reject = true;
        }
        if( b_reject )
        {
            pp_slaves[i] = NULL;
            input_item_slave_Delete( p_sub );
        }
    }

    /* Sort alphabetically */
    if( i_slaves > 0 )
        qsort( pp_slaves, i_slaves, sizeof (input_item_slave_t*), slave_strcmp );

    *ppp_slaves = pp_slaves; /* in case of realloc */
    *p_slaves = i_slaves;
    return VLC_SUCCESS;
}
