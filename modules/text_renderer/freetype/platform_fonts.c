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

/** \ingroup freetype_fonts
 * @{
 * \file
 * Platform-independent font management
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_text_style.h>                                  /* text_style_t */
#include <vlc_input.h>                             /* vlc_input_attachment_* */
#include <ctype.h>

#include "platform_fonts.h"
#include "freetype.h"
#include "fonts/backends.h"

FT_Face doLoadFace( void *ctx, const char *psz_fontfile, int i_idx )
{
    filter_t *p_filter = ctx;
    filter_sys_t *p_sys = p_filter->p_sys;

    FT_Face p_face = NULL;

    if( psz_fontfile[0] == ':' && psz_fontfile[1] == '/' )
    {
        int i_attach = atoi( psz_fontfile + 2 );
        if( i_attach < 0 || i_attach >= p_sys->i_font_attachments )
            msg_Err( p_filter, "LoadFace: Invalid font attachment index" );
        else
        {
            input_attachment_t *p_attach = p_sys->pp_font_attachments[ i_attach ];
            if( FT_New_Memory_Face( p_sys->p_library, p_attach->p_data,
                                    p_attach->i_data, i_idx, &p_face ) )
                return NULL;
        }
    }
#if defined( _WIN32 )
    else if( !memcmp( psz_fontfile, ":dw/", 4 ) )
    {
        int i_index = atoi( psz_fontfile + 4 );
        FT_Open_Args args = {0};
        args.flags = FT_OPEN_STREAM;
        if( DWrite_GetFontStream( p_sys->fs, i_index, &args.stream ) != VLC_SUCCESS ||
            FT_Open_Face( p_sys->p_library, &args, i_idx, &p_face ) )
        {
            msg_Err( p_filter, "LoadFace: Invalid font stream index" );
            return NULL;
        }
    }
#endif

    else
        if( FT_New_Face( p_sys->p_library, psz_fontfile, i_idx, &p_face ) )
            return NULL;

    return p_face;
}

bool CheckFace( vlc_font_select_t *fs, vlc_font_t *p_font, uni_char_t codepoint )
{
    filter_sys_t *p_sys = fs->p_filter->p_sys;

    vlc_face_id_t *faceid = p_font->faceid;
    if( !faceid )
    {
        faceid = vlc_ftcache_GetFaceID( p_sys->ftcache,
                                        p_font->psz_fontfile,
                                        p_font->i_index );
        p_font->faceid = faceid;
    }

    if( !faceid || vlc_ftcache_LoadFaceByIDNoSize( p_sys->ftcache, faceid ) )
        return false;

    return vlc_ftcache_LookupCMapIndex( p_sys->ftcache, faceid, codepoint );
}

/**
 * Select the best font from the list of vlc_font_t's of the given family.
 * If a family does not have the exact requested style, the nearest one will be returned.
 * Like when an italic font is requested from a family which has only a regular font. In this
 * case the regular font will be returned and FreeType will do synthetic styling on it.
 *
 * Not all fonts of a family support the same scripts. As an example, when an italic font
 * containing an Arabic codepoint is requested from the Arial family, the regular font will
 * be returned, because the italic font of Arial has no Arabic support.
 */
static vlc_font_t *GetBestFont( vlc_font_select_t *fs, const vlc_family_t *p_family,
                                int i_flags, uni_char_t codepoint )
{
    const int sameflagscheck[4] = {
        VLC_FONT_FLAG_BOLD | VLC_FONT_FLAG_ITALIC,
        VLC_FONT_FLAG_BOLD,
        VLC_FONT_FLAG_ITALIC,
        0
    };
    /* we do priority matching with different passes
       so we don't have to load & check every face or do store/sorting */
    for( int i=0; i<4; i++ )
    {
        for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
        {
            int i_sameflags = !(p_font->i_flags ^ i_flags);
            if( i_sameflags != sameflagscheck[i] )
                continue;
            if( !codepoint || CheckFace( fs, p_font, codepoint ) )
                return p_font;
        }
    }

    return p_family->p_fonts;
}

vlc_family_t *SearchFallbacks( vlc_font_select_t *fs, vlc_family_t *p_fallbacks,
                                      uni_char_t codepoint )
{
    vlc_family_t *p_family = NULL;

    for( vlc_family_t *p_fallback = p_fallbacks; p_fallback;
         p_fallback = p_fallback->p_next )
    {
        if( !p_fallback->p_fonts )
        {
            const vlc_family_t *p_temp = FontSelectFamily( fs, p_fallback->psz_name );
            if( !p_temp || !p_temp->p_fonts )
                continue;
            p_fallback->p_fonts = p_temp->p_fonts;
        }

        if( !CheckFace( fs, p_fallback->p_fonts, codepoint ) )
            continue;

        p_family = p_fallback;
        break;
    }

    return p_family;
}

static vlc_family_t *SearchFontByFamilyName( vlc_font_select_t *fs,
                                             vlc_family_t *p_list,
                                             const char *psz_familyname,
                                             uni_char_t codepoint )
{
    for( vlc_family_t *p = p_list; p; p = p->p_next )
    {
        if( !strcasecmp( p->psz_name, psz_familyname ) &&
            p->p_fonts &&
            CheckFace( fs, p->p_fonts, codepoint ) )
            return p;
    }
    return NULL;
}

static inline void AppendFont( vlc_font_t **pp_list, vlc_font_t *p_font )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_font;
}

static inline void AppendFamily( vlc_family_t **pp_list, vlc_family_t *p_family )
{
    while( *pp_list )
        pp_list = &( *pp_list )->p_next;

    *pp_list = p_family;
}

vlc_family_t *NewFamilyFromMixedCase( vlc_font_select_t *fs, const char *psz_family,
                                      vlc_family_t **pp_list, vlc_dictionary_t *p_dict,
                                      const char *psz_key )
{
    char *psz_alloc = NULL;
    char *psz_alloc_key = NULL;
    if( psz_family && *psz_family )
        psz_family = psz_alloc = LowercaseDup( psz_family );

    if( psz_key && p_dict )
        psz_key = psz_alloc_key = LowercaseDup( psz_key );

    vlc_family_t *ret = NewFamily( fs, psz_family, pp_list, p_dict, psz_key );

    free( psz_alloc );
    free( psz_alloc_key );

    return ret;
}

vlc_family_t *NewFamily( vlc_font_select_t *fs, const char *psz_lcname,
                         vlc_family_t **pp_list, vlc_dictionary_t *p_dict,
                         const char *psz_key )
{
    vlc_family_t *p_family = calloc( 1, sizeof( *p_family ) );
    if( unlikely(!p_family) )
        return NULL;

    assert(!psz_lcname || IsLowercase(psz_lcname));

    char *psz_alloc = NULL;
    if( !psz_lcname || !*psz_lcname )
        psz_lcname = psz_alloc = CreateUniqueFamilyKey( fs );
    if( unlikely( !psz_lcname ) )
    {
        free( p_family );
        return NULL;
    }

    p_family->psz_name = strdup( psz_lcname );
    if( unlikely(!p_family->psz_name) )
    {
        free( psz_alloc );
        free( p_family );
        return NULL;
    }

    if( pp_list )
        AppendFamily( pp_list, p_family );

    if( p_dict )
    {
        if( !psz_key )
            psz_key = psz_lcname;
        vlc_family_t *p_root = vlc_dictionary_value_for_key( p_dict, psz_key );
        if( p_root )
            AppendFamily( &p_root, p_family );
        else
            vlc_dictionary_insert( p_dict, psz_key, p_family );
    }

    free( psz_alloc );
    return p_family;
}

char *CreateUniqueFamilyKey( vlc_font_select_t *fs )
{
    char *psz_key;
    if( asprintf( &psz_key, FB_NAME"-%04d",
                  fs->i_fallback_counter++ ) < 0 )
        return NULL;
    return psz_key;
}

vlc_family_t * DeclareNewFamily( vlc_font_select_t *fs, const char *psz_family )
{
    char *psz_lc;

    if( psz_family )
        psz_lc = LowercaseDup( psz_family );
    else
        psz_lc = CreateUniqueFamilyKey( fs );

    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family = vlc_dictionary_value_for_key( &fs->family_map, psz_lc );
    if( p_family == kVLCDictionaryNotFound )
    {
        p_family = NewFamily( fs, psz_lc, &fs->p_families,
                              &fs->family_map, psz_lc );
    }

    free( psz_lc );

    return p_family;
}

int DeclareFamilyAsAttachMenFallback( vlc_font_select_t *fs, vlc_family_t *p_family )
{
    vlc_family_t *p_decl = NewFamily( fs, p_family->psz_name,
                                      NULL, NULL, NULL );
    if( unlikely(!p_decl) )
        return VLC_EGENERIC;

    p_decl->p_fonts = p_family->p_fonts;

    vlc_family_t *p_list =
            vlc_dictionary_value_for_key( &fs->fallback_map, FB_LIST_ATTACHMENTS );
    if( p_list )
        AppendFamily( &p_list, p_decl );
    else
        vlc_dictionary_insert( &fs->fallback_map, FB_LIST_ATTACHMENTS, p_decl );

    return VLC_SUCCESS;
}

vlc_font_t *NewFont( char *psz_fontfile, int i_index,
                     int i_flags, vlc_family_t *p_parent )
{
    vlc_font_t *p_font = calloc( 1, sizeof( *p_font ) );

    if( unlikely( !p_font ) )
    {
        free( psz_fontfile );
        return NULL;
    }

    p_font->psz_fontfile = psz_fontfile;
    p_font->i_index = i_index;
    p_font->i_flags = i_flags;

    if( p_parent )
    {
        /* Keep regular faces first */
        if( p_parent->p_fonts
         && p_parent->p_fonts->i_flags
         && !i_flags )
        {
            p_font->p_next = p_parent->p_fonts;
            p_parent->p_fonts = p_font;
        }
        else
            AppendFont( &p_parent->p_fonts, p_font );
    }

    return p_font;
}

void FreeFamiliesAndFonts( vlc_family_t *p_family )
{
    if( p_family->p_next )
        FreeFamiliesAndFonts( p_family->p_next );

    for( vlc_font_t *p_font = p_family->p_fonts; p_font; )
    {
        vlc_font_t *p_temp = p_font->p_next;
        free( p_font->psz_fontfile );
        free( p_font );
        p_font = p_temp;
    }

    free( p_family->psz_name );
    free( p_family );
}

void FreeFamilies( void *p_families, void *p_obj )
{
    vlc_family_t *p_family = ( vlc_family_t * ) p_families;

    if( p_family->p_next )
        FreeFamilies( p_family->p_next, p_obj );

    free( p_family->psz_name );
    free( p_family );
}

vlc_family_t *InitDefaultList( vlc_font_select_t *fs, const char *const *ppsz_default,
                               int i_size )
{

    vlc_family_t  *p_default  = NULL;

    for( int i = 0; i < i_size; ++i )
    {
        char *psz_lc = LowercaseDup( ppsz_default[ i ] );
        if( !psz_lc )
            continue;

        const vlc_family_t *p_family = FontSelectFamily( fs, psz_lc );
        if( p_family )
        {
            vlc_family_t *p_temp =
                NewFamily( fs, psz_lc, &p_default, NULL, NULL );

            if( unlikely( !p_temp ) )
            {
                free( psz_lc );
                goto error;
            }

            p_temp->p_fonts = p_family->p_fonts;
        }

        free( psz_lc );
    }

    if( p_default )
        vlc_dictionary_insert( &fs->fallback_map, FB_LIST_DEFAULT, p_default );

    return p_default;

error:
    if( p_default ) FreeFamilies( p_default, NULL );
    return NULL;
}

#ifdef DEBUG_PLATFORM_FONTS
static void DumpFamily( vlc_object_t *p_obj, const vlc_family_t *p_family,
                        bool b_dump_fonts, int i_max_families )
{

    if( i_max_families < 0 )
        i_max_families = INT_MAX;

    for( int i = 0; p_family && i < i_max_families ; p_family = p_family->p_next, ++i )
    {
        msg_Dbg( p_obj, "\t[%p] %s", (void *)p_family, p_family->psz_name );

        if( b_dump_fonts )
        {
            for( vlc_font_t *p_font = p_family->p_fonts; p_font; p_font = p_font->p_next )
            {
                const char *psz_style = NULL;
                if( p_font->i_flags == 0 )
                    psz_style = "Regular";
                else if( p_font->i_flags == VLC_FONT_FLAG_BOLD )
                    psz_style = "Bold";
                else if( p_font->i_flags == VLC_FONT_FLAG_ITALIC )
                    psz_style = "Italic";
                else if( p_font->i_flags == (VLC_FONT_FLAG_ITALIC|VLC_FONT_FLAG_BOLD) )
                    psz_style = "Bold Italic";

                msg_Dbg( p_obj, "\t\t[%p] (%s): %s - %d", (void *)p_font,
                         psz_style, p_font->psz_fontfile, p_font->i_index );
            }

        }
    }
}

static void DumpLRUElement( void *priv, const char *key, void *val )
{
    vlc_object_t *p_obj = priv;
    vlc_family_t *p_family = val;
    msg_Dbg( p_obj, "Key: %s", key );
    DumpFamily( p_obj, p_family, false, -1 );
}

static void DumpDictionary( vlc_object_t *p_obj, const vlc_dictionary_t *p_dict,
                            bool b_dump_fonts, int i_max_families )
{
    char **ppsz_keys = vlc_dictionary_all_keys( p_dict );

    if( unlikely( !ppsz_keys ) )
        return;

    for( int i = 0; ppsz_keys[ i ]; ++i )
    {
        vlc_family_t *p_family = vlc_dictionary_value_for_key( p_dict, ppsz_keys[ i ] );
        msg_Dbg( p_obj, "Key: %s", ppsz_keys[ i ] );
        if( p_family )
            DumpFamily( p_obj, p_family, b_dump_fonts, i_max_families );
        free( ppsz_keys[ i ] );
    }
    free( ppsz_keys );
}

void DumpFamilies( vlc_font_select_t *fs )
{
    vlc_object_t *p_obj = fs->p_obj;
    msg_Dbg( p_obj, "------------------" );
    msg_Dbg( p_obj, "p_families:" );
    msg_Dbg( p_obj, "------------------" );
    DumpFamily( p_obj, fs->p_families, true, -1 );
    msg_Dbg( p_obj, "-----------------" );
    msg_Dbg( p_obj, "family_map" );
    msg_Dbg( p_obj, "-----------------" );
    DumpDictionary( p_obj, &fs->family_map, false, 1 );
    msg_Dbg( p_obj, "-----------------" );
    msg_Dbg( p_obj, "families_lookup_lru" );
    msg_Dbg( p_obj, "-----------------" );
    vlc_lru_Apply( fs->families_lookup_lru, DumpLRUElement, p_obj );
    msg_Dbg( p_obj, "-------------------" );
    msg_Dbg( p_obj, "fallback_map" );
    msg_Dbg( p_obj, "-------------------" );
    DumpDictionary( p_obj, &fs->fallback_map, false, -1 );
# ifdef _WIN32
    msg_Dbg( p_obj, "-------------------" );
    msg_Dbg( p_obj, "fontlinking_map" );
    msg_Dbg( p_obj, "-------------------" );
    DumpDictionary( p_obj, &fs->fontlinking_map, true, 1 );
# endif
}
#endif

bool IsLowercase( const char *psz_src )
{
    size_t i_size=strlen(psz_src);
    for(size_t i = 0; i < i_size; ++i )
    {
        if( psz_src[ i ] != (char)tolower( psz_src[ i ] ) )
            return false;
    }
    return true;
}

void LowercaseTransform( char *psz )
{
    if( !psz )
        return;
    size_t i_size = strlen( psz );
    for( size_t i = 0; i < i_size; ++i )
        psz[ i ] = tolower( psz[ i ] );
}

char* LowercaseDup( const char *psz_src )
{
    int i_size = strlen( psz_src ) + 1;
    char *psz_buffer = malloc( i_size );
    if( unlikely( !psz_buffer ) )
        return NULL;

    for( int i = 0; i < i_size; ++i )
        psz_buffer[ i ] = tolower( psz_src[ i ] );

    return psz_buffer;
}

/* Face loading */
int ConvertToLiveSize( filter_t *p_filter, const text_style_t *p_style )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    int i_font_size = STYLE_DEFAULT_FONT_SIZE;
    if( p_style->i_font_size )
    {
        i_font_size = p_style->i_font_size;
    }
    else if ( p_style->f_font_relsize )
    {
        i_font_size = (int) p_filter->fmt_out.video.i_height * p_style->f_font_relsize / 100;
    }

    if( p_sys->i_scale != 100 )
        i_font_size = i_font_size * p_sys->i_scale / 100;

    return i_font_size;
}

static void TrimWhiteSpace( const char **pp, const char **ppend )
{
    for( ; *pp < *ppend ; ++(*pp) )
        if( **pp != ' ' && **pp != '\t' )
            break;
    for( ; *ppend > *pp ; --(*ppend) )
        if( *((*ppend)- 1) != ' ' && *((*ppend)- 1) != '\t' )
            break;
}

static void AddSingleFamily( const char *psz_start,
                             const char *psz_end,
                             fontfamilies_t *families )
{
    TrimWhiteSpace( &psz_start, &psz_end );
    /* basic unquote */
    if( psz_end > psz_start &&
        *psz_start == '"' && psz_end[-1] == '"' )
    {
        ++psz_start;
        --psz_end;
    }
    if( psz_end > psz_start )
    {
        char *psz = strndup( psz_start, psz_end - psz_start );
        if( psz )
            vlc_vector_push( &families->vec, psz );
    }
}

static void SplitIntoSingleFamily( const char *psz_spec, fontfamilies_t *families )
{
    if( !psz_spec )
        return;

    char *dup = strdup( psz_spec );
    char *p_savetpr;
    for ( const char *psz = strtok_r( dup, ",", &p_savetpr );
          psz != NULL;
          psz = strtok_r( NULL, ",", &p_savetpr ) )
    {
        AddSingleFamily( psz, psz + strlen( psz ), families );
    }
    free( dup );
}

static char* SelectFontWithFamilyFallback( vlc_font_select_t *fs,
                                    const fontfamilies_t *families,
                                    const text_style_t *p_style,
                                    int *pi_idx, uni_char_t codepoint )
{
    const vlc_family_t *p_family = NULL;

    if( codepoint && !p_family )
    {
        vlc_family_t *p_fallbacks;
        const char *psz_lcname;
        /*
         * Try regular face of the same family first.
         * It usually has the best coverage.
         */
        vlc_vector_foreach( psz_lcname, &families->vec )
        {
            Debug( fs->p_obj, "Looking for family \"%s\"", psz_lcname );

            p_fallbacks = vlc_dictionary_value_for_key( &fs->fallback_map,
                                                        FB_LIST_ATTACHMENTS );
            if( p_fallbacks )
            {
                p_family = SearchFontByFamilyName( fs, p_fallbacks,
                                                   psz_lcname, codepoint );
                if( p_family )
                    break;
            }

            p_family = NULL;
        }

        p_family = FontSelectAmongFamilies( fs, families );
        if( p_family )
        {
            if( p_family->p_fonts &&
                CheckFace( fs, p_family->p_fonts, codepoint ) )
            {
                Debug( fs->p_obj, "Found family \"%s\" for codepoint %x",
                       psz_lcname, codepoint );
            }
            else p_family = NULL;
        }

        /* Try font attachments if not available locally */
        if( !p_family )
        {
            Debug( fs->p_obj, "Looking for family \"%s\" in attachments cp %x", psz_lcname, codepoint );
            p_fallbacks = vlc_dictionary_value_for_key( &fs->fallback_map,
                                                        FB_LIST_ATTACHMENTS );
            if( p_fallbacks )
            {
                p_family = SearchFallbacks( fs, p_fallbacks, codepoint );
                if( p_family )
                {
                    Debug( fs->p_obj, "Selected family \"%s\" in attachments",
                           p_family->psz_name );
                }
            }
        }

        /* Try system fallbacks */
        if( !p_family )
        {
            p_fallbacks = FontFallbacksAmongFamilies( fs, families, codepoint );
            if( p_fallbacks )
            {
                Debug( fs->p_obj, "Looking for families \"%s\" in system fallbacks cp %x",
                                   families->psz_key, codepoint );
                p_family = SearchFallbacks( fs, p_fallbacks, codepoint );
                if( p_family && p_family->p_fonts )
                {
                    Debug( fs->p_obj, "Selected family \"%s\" in system fallbacks",
                           p_family->psz_name );
                }
                else p_family = NULL;
            }
        }

        /* Try the default fallback list, if any */
        if( !p_family )
        {
            p_fallbacks = vlc_dictionary_value_for_key( &fs->fallback_map,
                                                        FB_LIST_DEFAULT );
            if( p_fallbacks )
                p_family = SearchFallbacks( fs, p_fallbacks, codepoint );
            if( p_family )
            {
                Debug( fs->p_obj, "Selected family \"%s\" in default fallback list",
                       p_family->psz_name );
            }
        }

        if( !p_family )
            return NULL;
    }

    if( !p_family || !p_family->p_fonts )
    {
        Debug( fs->p_obj, "Looking for DEFAULT_FAMILY \"%s\" as a last resort", DEFAULT_FAMILY );
        char *lc = LowercaseDup(DEFAULT_FAMILY);
        p_family = FontSelectFamily( fs, lc );
        free(lc);
    }

    int i_flags = 0;
    if( p_style->i_style_flags & STYLE_BOLD )
        i_flags |= VLC_FONT_FLAG_BOLD;
    if( p_style->i_style_flags & STYLE_ITALIC )
        i_flags |= VLC_FONT_FLAG_ITALIC;

    vlc_font_t *p_font;
    if( p_family && ( p_font = GetBestFont( fs, p_family, i_flags, codepoint ) ) )
    {
        Debug( fs->p_obj, "Selected best font file \"%s\" %x", p_font->psz_fontfile, i_flags );
        *pi_idx = p_font->i_index;
        return strdup( p_font->psz_fontfile );
    }

    Debug( fs->p_obj, "Could not match any family or font : that should not happen" );

    return NULL;
}

vlc_face_id_t *
SelectAndLoadFace( filter_t *p_filter, const text_style_t *p_style, uni_char_t codepoint )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const char *psz_fontname = (p_style->i_style_flags & STYLE_MONOSPACED)
                               ? p_style->psz_monofontname : p_style->psz_fontname;

    char *psz_alloc = !IsLowercase(psz_fontname) ? LowercaseDup(psz_fontname) : NULL;
    if( psz_alloc )
        psz_fontname = psz_alloc;

    fontfamilies_t families;
    families.psz_key = psz_fontname;
    vlc_vector_init( &families.vec );
    SplitIntoSingleFamily( psz_fontname, &families );
    if( families.vec.size == 0 )
    {
        vlc_vector_clear( &families.vec );
        free(psz_alloc);
        return NULL;
    }

    vlc_face_id_t *p_faceid = NULL;

    int  i_idx = 0;
    char *psz_fontfile =
            SelectFontWithFamilyFallback( p_sys->fs, &families, p_style,
                                          &i_idx, codepoint );

    if( psz_fontfile && *psz_fontfile != '\0' )
    {
        p_faceid  = vlc_ftcache_GetFaceID( p_sys->ftcache, psz_fontfile, i_idx );
        if( p_faceid )
        {
            vlc_ftcache_metrics_t metrics;
            metrics.height_px = ConvertToLiveSize( p_filter, p_style );
            metrics.width_px = GetFontWidthForStyle( p_style, metrics.height_px );
            if( !vlc_ftcache_LoadFaceByID( p_sys->ftcache, p_faceid, &metrics ) )
                p_faceid = NULL;
        }
    }

    if( !p_faceid )
    {
        msg_Warn( p_filter,
                  "SelectAndLoadFace: no font found for family: %s, codepoint: 0x%x",
                  psz_fontname, codepoint );
    }

    char *psz_temp;
    vlc_vector_foreach( psz_temp, &families.vec )
        free( psz_temp );
    vlc_vector_clear( &families.vec );
    free( psz_fontfile );
    free(psz_alloc);

    return p_faceid;
}

#ifndef HAVE_GET_FONT_BY_FAMILY_NAME
static int StaticMap_GetFamily( vlc_font_select_t *fs, const char *psz_lcname,
                                const vlc_family_t **pp_result )
{
    filter_t *p_filter = fs->p_filter;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_family_t *p_family =
            vlc_dictionary_value_for_key( &fs->family_map, psz_lcname );
    if( p_family )
    {
        *pp_result = p_family;
        return VLC_SUCCESS;
    }

    const char *psz_file = NULL;
    if( !strcasecmp( psz_lcname, DEFAULT_FAMILY ) )
    {
        psz_file = p_sys->psz_fontfile ? p_sys->psz_fontfile
                                       : DEFAULT_FONT_FILE;
    }
    else if( !strcasecmp( psz_lcname, DEFAULT_MONOSPACE_FAMILY ) )
    {
        psz_file = p_sys->psz_monofontfile ? p_sys->psz_monofontfile
                                           : DEFAULT_MONOSPACE_FONT_FILE;
    }

    if( !psz_file )
    {
        *pp_result = NULL;
        return VLC_SUCCESS;
    }

    /* Create new entry */
    p_family = NewFamily( fs, psz_lcname, &fs->p_families,
                          &fs->family_map, psz_lcname );

    if( unlikely( !p_family ) )
        return VLC_EGENERIC;

    char *psz_font_file = MakeFilePath( fs, psz_file );
    if( psz_font_file )
        NewFont( psz_font_file, 0, 0, p_family );

    *pp_result = p_family;
    return VLC_SUCCESS;
}
#endif

#if !defined(_WIN32) || defined(VLC_WINSTORE_APP)

char * MakeFilePath( vlc_font_select_t *fs, const char *psz_filename )
{
    VLC_UNUSED(fs);

    if( !psz_filename )
        return NULL;

    /* Handle the case where the user redefined *_FILE using FQN */
    if( psz_filename[0] == DIR_SEP_CHAR )
        return strdup( psz_filename );

    char *psz_filepath;
    if( asprintf( &psz_filepath, "%s" DIR_SEP "%s",
                  SYSTEM_FONT_PATH, psz_filename ) == -1 )
        psz_filepath = NULL;

    return psz_filepath;
}
#endif

vlc_font_select_t * FontSelectNew( filter_t *p_filter )
{
    vlc_font_select_t *fs = calloc( 1, sizeof(*fs) );
    if( !fs )
        return fs;

    fs->p_filter = p_filter;
    fs->p_obj = VLC_OBJECT(p_filter);

    /* Dictionaries for families */
    vlc_dictionary_init( &fs->family_map, 53 );
    vlc_dictionary_init( &fs->fallback_map, 23 );

    fs->families_lookup_lru = vlc_lru_New( 23, NULL, NULL );
    if( !fs->families_lookup_lru )
        goto error;

#ifdef HAVE_FONTCONFIG
    fs->pf_select_family = FontConfig_GetFamily;
    fs->pf_select_among_families = FontConfig_SelectAmongFamilies;
    fs->pf_get_fallbacks_among_families = FontConfig_GetFallbacksAmongFamilies;
    if( FontConfig_Prepare( fs ) )
    {
        fs->pf_select_family = NULL;
        goto error;
    }

#elif defined( __APPLE__ )
    fs->pf_select_family = CoreText_GetFamily;
    fs->pf_get_fallbacks = CoreText_GetFallbacks;
#elif defined( _WIN32 )
    vlc_dictionary_init( &fs->fontlinking_map, 20 );
    if( InitDWrite( fs ) == VLC_SUCCESS )
    {
        fs->pf_select_family = DWrite_GetFamily;
        fs->pf_get_fallbacks = DWrite_GetFallbacks;
    }
    else
    {
#ifdef VLC_WINSTORE_APP
        msg_Err( p_filter, "Error initializing DirectWrite" );
        goto error;
#else
        msg_Warn( p_filter, "DirectWrite initialization failed. Falling back to GDI/Uniscribe" );
        const char *const ppsz_default[] =
            { "tahoma", "fangsong", "simhei", "kaiti" };
        fs->pf_select_family = Win32_GetFamily;
        fs->pf_get_fallbacks = Win32_GetFallbacks;
        if( InitDefaultList( fs, ppsz_default, ARRAY_SIZE(ppsz_default) ) == NULL )
            goto error;
#endif
    }
#elif defined( __ANDROID__ )
    fs->pf_select_family = Android_GetFamily;
    fs->pf_get_fallbacks = Android_GetFallbacks;

    if( Android_Prepare( fs ) == VLC_ENOMEM )
        goto error;
#else
    fs->pf_select_family = StaticMap_GetFamily;
    fs->pf_get_fallbacks = NULL;
    /* The default static fonts are also fallback fonts */
    const char *const ppsz_default[] =
        { DEFAULT_FAMILY, DEFAULT_MONOSPACE_FAMILY };
    if( InitDefaultList( fs, ppsz_default, ARRAY_SIZE(ppsz_default) ) == NULL )
        goto error;
#endif

    return fs;

error:
    FontSelectDelete( fs );
    return NULL;
}

void FontSelectDelete( vlc_font_select_t *fs )
{
#ifdef HAVE_FONTCONFIG
    if( fs->pf_select_family == FontConfig_GetFamily )
        FontConfig_Unprepare( fs );

#elif defined( _WIN32 )
    if( fs->pf_select_family == DWrite_GetFamily )
        ReleaseDWrite( fs );
    vlc_dictionary_clear( &fs->fontlinking_map, FreeFamilies, fs );
#endif

    if( fs->families_lookup_lru )
        vlc_lru_Release( fs->families_lookup_lru );

    /* Dicts */
    vlc_dictionary_clear( &fs->fallback_map, FreeFamilies, fs );
    vlc_dictionary_clear( &fs->family_map, NULL, NULL );
    if( fs->p_families )
        FreeFamiliesAndFonts( fs->p_families );

    free( fs );
}

const vlc_family_t * FontSelectFamily( vlc_font_select_t *fs, const char *psz_lcname )
{
    const vlc_family_t *p_family = NULL;
    if( fs->pf_select_family )
        fs->pf_select_family( fs, psz_lcname, &p_family );
    return p_family;
}

const vlc_family_t * FontSelectAmongFamilies( vlc_font_select_t *fs, const fontfamilies_t *families )
{
    /* We return empty NULL matches, so we check if key exists, not value */
    if( vlc_lru_HasKey( fs->families_lookup_lru, families->psz_key ) )
        return vlc_lru_Get( fs->families_lookup_lru, families->psz_key );

    const vlc_family_t *p_family = NULL;
    if( fs->pf_select_among_families )
    {
        if( fs->pf_select_among_families( fs, families, &p_family ) != VLC_SUCCESS )
            p_family = NULL;
    }
    else if( fs->pf_select_family )
    {
        const char *psz_name;
        vlc_vector_foreach( psz_name, &families->vec )
        {
            fs->pf_select_family( fs, psz_name, &p_family );
            if( p_family )
                break;
        }
    }
    vlc_lru_Insert( fs->families_lookup_lru, families->psz_key, (void *) p_family );
    return p_family;
}

vlc_family_t * FontFallbacksAmongFamilies( vlc_font_select_t *fs, const fontfamilies_t *families,
                                           uni_char_t codepoint )
{
    vlc_family_t *p_res = NULL;
    if ( fs->pf_get_fallbacks_among_families )
    {
        fs->pf_get_fallbacks_among_families( fs, families, codepoint, &p_res );
    }
    else if( fs->pf_get_fallbacks )
    {
        const char *psz_name;
        vlc_vector_foreach( psz_name, &families->vec )
        {
            fs->pf_get_fallbacks( fs, psz_name, codepoint, &p_res );
            if( p_res )
                break;
        }
    }
    return p_res;
}
