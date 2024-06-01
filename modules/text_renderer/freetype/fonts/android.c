/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>                                      /* filter_sys_t */

# include <vlc_xml.h>
# include <vlc_stream.h>

#include "../platform_fonts.h"
#include "backends.h"

#define ANDROID_SYSTEM_FONTS_NOUGAT  "file:///system/etc/fonts.xml"
#define ANDROID_SYSTEM_FONTS_LEGACY  "file:///system/etc/system_fonts.xml"
#define ANDROID_FALLBACK_FONTS       "file:///system/etc/fallback_fonts.xml"
#define ANDROID_VENDOR_FONTS         "file:///vendor/etc/fallback_fonts.xml"

static int Android_ParseFont( vlc_font_select_t *fs, xml_reader_t *p_xml,
                              vlc_family_t *p_family )
{
    int               i_flags     = 0;
    const char       *psz_val     = NULL;
    const char       *psz_attr    = NULL;
    int               i_type      = 0;
    int               i_weight    = 0;

    while( ( psz_attr = xml_ReaderNextAttr( p_xml, &psz_val ) ) )
    {
        if( !psz_val || !*psz_val )
            continue;
        if( !strcasecmp( "weight", psz_attr ) )
            i_weight = atoi( psz_val );
        else if( !strcasecmp( "style", psz_attr ) )
            if( !strcasecmp( "italic", psz_val ) )
                i_flags |= VLC_FONT_FLAG_ITALIC;
    }

    if( i_weight == 700 )
        i_flags |= VLC_FONT_FLAG_BOLD;

    i_type = xml_ReaderNextNode( p_xml, &psz_val );

    if( i_type != XML_READER_TEXT || !psz_val || !*psz_val )
    {
        msg_Warn( fs->p_obj, "Android_ParseFont: no file name" );
        return VLC_EGENERIC;
    }

    char *psz_fontfile;

    /*
     * We don't need all font weights. Only 400 (regular) and 700 (bold)
     */
    if( i_weight == 400 || i_weight == 700 )
    {
        /* left trim */
        psz_val += strspn( psz_val, " \t\r\n" );
        /* right trim */
        size_t len = strlen( psz_val );
        const char *psz_end = psz_val + len;
        while( psz_end > psz_val &&
               ( psz_end[-1] == ' ' ||
                 psz_end[-1] == '\t' ||
                 psz_end[-1] == '\r' ||
                 psz_end[-1] == '\n' ) )
            psz_end--;
        len = psz_end - psz_val;

        psz_fontfile = malloc( sizeof(SYSTEM_FONT_PATH) + 1 + len );
        if( !psz_fontfile )
            return VLC_ENOMEM;
        memcpy( psz_fontfile, SYSTEM_FONT_PATH, sizeof(SYSTEM_FONT_PATH) - 1 );
        psz_fontfile[sizeof(SYSTEM_FONT_PATH) - 1] = '/';
        memcpy( &psz_fontfile[sizeof(SYSTEM_FONT_PATH)], psz_val, len );
        psz_fontfile[sizeof(SYSTEM_FONT_PATH) + len] = '\0';

        if( !NewFont( psz_fontfile, 0, i_flags, p_family ) )
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static int Android_Nougat_ParseFamily( vlc_font_select_t *fs, xml_reader_t *p_xml )
{
    vlc_dictionary_t *p_dict      = &fs->family_map;
    vlc_family_t     *p_family    = NULL;
    const char       *psz_val     = NULL;
    const char       *psz_attr    = NULL;
    const char       *psz_name    = NULL;
    char             *psz_lc      = NULL;
    int               i_type      = 0;

    while( ( psz_attr = xml_ReaderNextAttr( p_xml, &psz_val ) ) )
    {
        if( !strcasecmp( "name", psz_attr ) && psz_val && *psz_val )
        {
            psz_name = psz_val;
            break;
        }
    }

    if( psz_name && *psz_name )
    {
        /*
         * Family has a name. See if we have that name already.
         * If the name already exists, it's one of the font attachments.
         */
        psz_lc = LowercaseDup( psz_name );
        if( unlikely( !psz_lc ) )
            return VLC_ENOMEM;

        p_family = vlc_dictionary_value_for_key( p_dict, psz_lc );
    }

    if( p_family == NULL )
    {
        /*
         * We are either parsing a nameless family, or a named family that
         * was not previously added to fs->family_map.
         *
         * Create a new family with the given name or, if psz_name is NULL,
         * with the name fallback-xxxx
         */
        p_family = NewFamilyFromMixedCase( fs, psz_lc, &fs->p_families,
                              &fs->family_map, NULL );
    }

    free( psz_lc );

    if( unlikely( !p_family ) )
        return VLC_ENOMEM;

    while( ( i_type = xml_ReaderNextNode( p_xml, &psz_val ) ) > 0 )
    {
        switch( i_type )
        {
        case XML_READER_STARTELEM:
            if( !strcasecmp( "font", psz_val ) )
                if( Android_ParseFont( fs, p_xml, p_family ) == VLC_ENOMEM )
                    return VLC_ENOMEM;
            break;

        case XML_READER_ENDELEM:
            if( !strcasecmp( "family", psz_val ) )
            {
                if( strcasestr( p_family->psz_name, FB_NAME ) )
                {
                    /*
                     * If the family name has "fallback" in it add it to the
                     * default fallback list.
                     */
                    vlc_family_t *p_fallback =
                        NewFamily( fs, p_family->psz_name,
                                   NULL, &fs->fallback_map, FB_LIST_DEFAULT );

                    if( unlikely( !p_fallback ) )
                        return VLC_ENOMEM;

                    p_fallback->p_fonts = p_family->p_fonts;
                }

                return VLC_SUCCESS;
            }
            break;
        }
    }

    msg_Warn( fs->p_obj, "Android_ParseFamily: Corrupt font configuration file" );
    return VLC_EGENERIC;
}

static int Android_ParseAlias( vlc_font_select_t *fs, xml_reader_t *p_xml )
{
    vlc_dictionary_t *p_dict      = &fs->family_map;
    vlc_family_t     *p_dest      = NULL;
    char             *psz_name    = NULL;
    char             *psz_dest    = NULL;
    const char       *psz_val     = NULL;
    const char       *psz_attr    = NULL;
    int               i_weight    = 0;
    int               i_ret       = VLC_SUCCESS;

    while( ( psz_attr = xml_ReaderNextAttr( p_xml, &psz_val ) ) )
    {
        if( !psz_val || !*psz_val )
            continue;
        if( !strcasecmp( "weight", psz_attr ) )
            i_weight = atoi( psz_val );
        else if( !strcasecmp( "to", psz_attr ) )
            psz_dest = LowercaseDup( psz_val );
        else if( !strcasecmp( "name", psz_attr ) )
            psz_name = LowercaseDup( psz_val );
    }

    if( !psz_dest || !psz_name )
    {
        i_ret = VLC_EGENERIC;
        goto done;
    }

    p_dest = vlc_dictionary_value_for_key( p_dict, psz_dest );

    if( p_dest && i_weight == 0 )
        if( vlc_dictionary_value_for_key( p_dict, psz_name ) == NULL )
            vlc_dictionary_insert( p_dict, psz_name, p_dest );

done:
    free( psz_dest );
    free( psz_name );
    return i_ret;
}

static int Android_Legacy_ParseFamily( vlc_font_select_t *fs, xml_reader_t *p_xml )
{
    vlc_dictionary_t *p_dict      = &fs->family_map;
    vlc_family_t     *p_family    = NULL;
    char             *psz_lc      = NULL;
    int               i_counter   = 0;
    const char       *p_node      = NULL;
    int               i_type      = 0;

    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        switch( i_type )
        {
        case XML_READER_STARTELEM:
            /*
             * Multiple names can reference the same family in Android. When
             * the first name is encountered we set p_family to the vlc_family_t
             * in the master list matching this name, and if no such family
             * exists we create a new one and add it to the master list.
             * If the master list does contain a family with that name it's one
             * of the font attachments, and the family will end up having embedded
             * fonts and system fonts.
             */
            if( !strcasecmp( "name", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    msg_Warn( fs->p_obj, "Android_ParseFamily: empty name" );
                    continue;
                }

                psz_lc = LowercaseDup( p_node );
                if( unlikely( !psz_lc ) )
                    return VLC_ENOMEM;

                if( !p_family )
                {
                    p_family = vlc_dictionary_value_for_key( p_dict, psz_lc );
                    if( p_family == kVLCDictionaryNotFound )
                    {
                        p_family =
                            NewFamily( fs, psz_lc, &fs->p_families, NULL, NULL );

                        if( unlikely( !p_family ) )
                        {
                            free( psz_lc );
                            return VLC_ENOMEM;
                        }

                    }
                }

                if( vlc_dictionary_value_for_key( p_dict, psz_lc ) == kVLCDictionaryNotFound )
                    vlc_dictionary_insert( p_dict, psz_lc, p_family );
                free( psz_lc );
            }
            /*
             * If p_family has not been set by the time we encounter the first file,
             * it means this family has no name, and should be used only as a fallback.
             * We create a new family for it in the master list with the name "fallback-xxxx"
             * and later add it to the "default" fallback list.
             */
            else if( !strcasecmp( "file", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    ++i_counter;
                    continue;
                }

                if( !p_family )
                    p_family = NewFamily( fs, NULL, &fs->p_families,
                                          &fs->family_map, NULL );

                if( unlikely( !p_family ) )
                    return VLC_ENOMEM;

                int i_flags = 0;
                switch( i_counter )
                {
                case 0:
                    break;
                case 1:
                    i_flags = VLC_FONT_FLAG_BOLD;
                    break;
                case 2:
                    i_flags = VLC_FONT_FLAG_ITALIC;
                    break;
                case 3:
                    i_flags = VLC_FONT_FLAG_BOLD | VLC_FONT_FLAG_ITALIC;
                    break;
                default:
                    msg_Warn( fs->p_obj, "Android_ParseFamily: too many files" );
                    return VLC_EGENERIC;
                }

                char *psz_fontfile = NULL;
                if( asprintf( &psz_fontfile, "%s/%s", SYSTEM_FONT_PATH, p_node ) < 0
                 || !NewFont( psz_fontfile, 0, i_flags, p_family ) )
                    return VLC_ENOMEM;

                ++i_counter;
            }
            break;

        case XML_READER_ENDELEM:
            if( !strcasecmp( "family", p_node ) )
            {
                if( !p_family )
                {
                    msg_Warn( fs->p_obj, "Android_ParseFamily: empty family" );
                    return VLC_EGENERIC;
                }

                /*
                 * If the family name has "fallback" in it, add it to the
                 * "default" fallback list.
                 */
                if( strcasestr( p_family->psz_name, FB_NAME ) )
                {
                    vlc_family_t *p_fallback =
                        NewFamily( fs, p_family->psz_name,
                                   NULL, &fs->fallback_map, FB_LIST_DEFAULT );

                    if( unlikely( !p_fallback ) )
                        return VLC_ENOMEM;

                    p_fallback->p_fonts = p_family->p_fonts;
                }

                return VLC_SUCCESS;
            }
            break;
        }
    }

    msg_Warn( fs->p_obj, "Android_ParseOldFamily: Corrupt font configuration file" );
    return VLC_EGENERIC;
}

static int Android_ParseSystemFonts( vlc_font_select_t *fs, const char *psz_path,
                                     bool b_new_format )
{
    int i_ret = VLC_SUCCESS;
    stream_t *p_stream = vlc_stream_NewURL( fs->p_obj, psz_path );

    if( !p_stream )
        return VLC_EGENERIC;

    xml_reader_t *p_xml = xml_ReaderCreate( fs->p_obj, p_stream );

    if( !p_xml )
    {
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    const char *p_node;
    int i_type;
    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        if( i_type != XML_READER_STARTELEM )
            continue;
        int i_ret = 0;
        if( !strcasecmp( "family", p_node ) )
        {
            i_ret = b_new_format
                  ? Android_Nougat_ParseFamily( fs, p_xml )
                  : Android_Legacy_ParseFamily( fs, p_xml );
        }
        else if( !strcasecmp( "alias", p_node ) && b_new_format )
        {
            i_ret = Android_ParseAlias( fs, p_xml );
        }
        if( i_ret )
            break;
    }

    xml_ReaderDelete( p_xml );
    vlc_stream_Delete( p_stream );
    return i_ret;
}

int Android_Prepare( vlc_font_select_t *fs )
{
    if( Android_ParseSystemFonts( fs, ANDROID_SYSTEM_FONTS_NOUGAT, true ) != VLC_SUCCESS )
    {
        if( Android_ParseSystemFonts( fs, ANDROID_SYSTEM_FONTS_LEGACY, false ) == VLC_ENOMEM )
            return VLC_ENOMEM;
        if( Android_ParseSystemFonts( fs, ANDROID_FALLBACK_FONTS, false ) == VLC_ENOMEM )
            return VLC_ENOMEM;
        if( Android_ParseSystemFonts( fs, ANDROID_VENDOR_FONTS, false ) == VLC_ENOMEM )
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

int Android_GetFamily( vlc_font_select_t *fs, const char *psz_lcname,
                       const vlc_family_t **pp_result )
{
    *pp_result = vlc_dictionary_value_for_key( &fs->family_map, psz_lcname );
    if( *pp_result == kVLCDictionaryNotFound )
        *pp_result = NULL;

    return VLC_SUCCESS;
}

int Android_GetFallbacks( vlc_font_select_t *fs, const char *psz_lcname,
                          uni_char_t codepoint, vlc_family_t **pp_result )
{
    VLC_UNUSED( codepoint );

    *pp_result = vlc_dictionary_value_for_key( &fs->fallback_map, psz_lcname );
    if( *pp_result == kVLCDictionaryNotFound )
        *pp_result = NULL;

    return VLC_SUCCESS;
}
