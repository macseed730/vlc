/*****************************************************************************
 * help.c: command line help
 *****************************************************************************
 * Copyright (C) 1998-2011 VLC authors and VideoLAN
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

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#include <float.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_charset.h>
#include "ansi_term.h"
#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

#if defined( _WIN32 )
# define wcwidth(cp) ((void)(cp), 1) /* LOL */
#endif
#include <unistd.h>

#if defined( _WIN32 ) && !defined( VLC_WINSTORE_APP )
static void ShowConsole (void);
static void PauseConsole (void);
#else
# define ShowConsole() (void)0
# define PauseConsole() (void)0
#endif

static void Help (libvlc_int_t *, const char *);
static void Usage (libvlc_int_t *, const char *);
static void Version (void);
static void ListModules (libvlc_int_t *, bool);

/**
 * Returns the console width or a best guess.
 */
static unsigned ConsoleWidth(void)
{
#ifdef TIOCGWINSZ
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        return ws.ws_col;
#endif
#ifdef WIOCGETD
    struct uwdata uw;

    if (ioctl(STDOUT_FILENO, WIOCGETD, &uw) == 0)
        return uw.uw_height / uw.uw_vs;
#endif
#if defined (_WIN32) && !defined(VLC_WINSTORE_APP)
    CONSOLE_SCREEN_BUFFER_INFO buf;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &buf))
        return buf.dwSize.X;
#endif
    return 80;
}

/**
 * Checks for help command line options such as --help or --version.
 * If one is found, print the corresponding text.
 * \return true if a command line options caused some help message to be
 * printed, false otherwise.
 */
bool config_PrintHelp (libvlc_int_t *obj)
{
    char *str;

    /* Check for version option */
    if (var_InheritBool (obj, "version"))
    {
        Version();
        return true;
    }

    /* Check for help on modules */
    str = var_InheritString (obj, "module");
    if (str != NULL)
    {
        Help (obj, str);
        free (str);
        return true;
    }

    /* Check for full help option */
    if (var_InheritBool (obj, "full-help"))
    {
        var_Create (obj, "help-verbose", VLC_VAR_BOOL);
        var_SetBool (obj, "help-verbose", true);
        Help (obj, "full-help");
        return true;
    }

    /* Check for long help option */
    if (var_InheritBool (obj, "longhelp"))
    {
        Help (obj, "longhelp");
        return true;
    }
    /* Check for short help option */
    if (var_InheritBool (obj, "help"))
    {
        Help (obj, "help");
        return true;
    }

    if (var_InheritBool (obj, "list-verbose"))
    {
        ListModules (obj, true);
        return true;
    }

    /* Check for module list option */
    if (var_InheritBool (obj, "list"))
    {
        ListModules (obj, false );
        return true;
    }

    return false;
}

/*****************************************************************************
 * Help: print program help
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static inline void print_help_on_full_help( void )
{
    putchar('\n');
    puts(_("To get exhaustive help, use '-H'."));
}

static const char vlc_usage[] = N_(
  "Usage: %s [options] [stream] ...\n"
  "You can specify multiple streams on the commandline.\n"
  "They will be enqueued in the playlist.\n"
  "The first item specified will be played first.\n"
  "\n"
  "Options-styles:\n"
  "  --option  A global option that is set for the duration of the program.\n"
  "   -option  A single letter version of a global --option.\n"
  "   :option  An option that only applies to the stream directly before it\n"
  "            and that overrides previous settings.\n"
  "\n"
  "Stream MRL syntax:\n"
  "  [[access][/demux]://]URL[#[title][:chapter][-[title][:chapter]]]\n"
  "  [:option=value ...]\n"
  "\n"
  "  Many of the global --options can also be used as MRL specific :options.\n"
  "  Multiple :option=value pairs can be specified.\n"
  "\n"
  "URL syntax:\n"
  "  file:///path/file              Plain media file\n"
  "  http://host[:port]/file        HTTP URL\n"
  "  ftp://host[:port]/file         FTP URL\n"
  "  mms://host[:port]/file         MMS URL\n"
  "  screen://                      Screen capture\n"
  "  dvd://[device]                 DVD device\n"
  "  vcd://[device]                 VCD device\n"
  "  cdda://[device]                Audio CD device\n"
  "  udp://[[<source address>]@[<bind address>][:<bind port>]]\n"
  "                                 UDP stream sent by a streaming server\n"
  "  vlc://pause:<seconds>          Pause the playlist for a certain time\n"
  "  vlc://quit                     Special item to quit VLC\n"
  "\n");

static void Help (libvlc_int_t *p_this, char const *psz_help_name)
{
    ShowConsole();

    if( psz_help_name && !strcmp( psz_help_name, "help" ) )
    {
        printf(_(vlc_usage), "vlc");
        Usage( p_this, "=core" );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        printf(_(vlc_usage), "vlc");
        Usage( p_this, NULL );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "full-help" ) )
    {
        printf(_(vlc_usage), "vlc");
        Usage( p_this, NULL );
    }
    else if( psz_help_name )
    {
        Usage( p_this, psz_help_name );
    }

    PauseConsole();
}

/*****************************************************************************
 * Usage: print module usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
#   define LINE_START      8
#   define PADDING_SPACES 25

static void print_section(const module_t *m, const module_config_t **sect,
                          bool color, bool desc)
{
    const module_config_t *item = *sect;

    if (item == NULL)
        return;
    *sect = NULL;

    printf(color ? TS_RED_BOLD "   %s:\n" TS_RESET : "   %s:\n",
           module_gettext(m, item->psz_text));
    if (desc && item->psz_longtext != NULL)
        printf(color ? TS_MAGENTA_BOLD "   %s\n" TS_RESET : "   %s\n",
               module_gettext(m, item->psz_longtext));
}

static void print_desc(const char *str, unsigned margin, bool color)
{
    unsigned width = ConsoleWidth() - margin;

    if (color)
        fputs(TS_BLUE_BOLD, stdout);

    const char *word = str;
    size_t wordlen = 0;
    int wordwidth = 0;
    unsigned offset = 0;
    bool newline = true;

    while (str[0])
    {
        uint32_t cp;
        ssize_t charlen = vlc_towc(str, &cp);
        if (unlikely(charlen == -1))
            break;

        int charwidth = wcwidth(cp);
        if (charwidth < 0)
            charwidth = 0;

        str += charlen;

        if (iswspace(cp))
        {
            if (!newline)
            {
                putchar(' '); /* insert space */
                charwidth = 1;
            }
            fwrite(word, 1, wordlen, stdout); /* write complete word */
            word = str;
            wordlen = 0;
            wordwidth = 0;
            newline = false;
        }
        else
        {
            wordlen += charlen;
            wordwidth += charwidth;
        }

        offset += charwidth;
        if (offset >= width)
        {
            if (newline)
            {   /* overflow (word wider than line) */
                fwrite(word, 1, wordlen - charlen, stdout);
                word = str - charlen;
                wordlen = charlen;
                wordwidth = charwidth;
            }
            printf("\n%*s", margin, ""); /* new line */
            offset = wordwidth;
            newline = true;
        }
    }

    if (!newline)
        putchar(' ');
    printf(color ? "%s\n" TS_RESET : "%s\n", word);
}

static int vlc_swidth(const char *str)
{
    for (int total = 0;;)
    {
        uint32_t cp;
        ssize_t charlen = vlc_towc(str, &cp);

        if (charlen == 0)
            return total;
        if (charlen == -1)
            return -1;
        str += charlen;

        int w = wcwidth(cp);
        if (w == -1)
            return -1;
        total += w;
    }
}

static void print_item(const module_t *m, const struct vlc_param *param,
                       const module_config_t **section, bool color, bool desc)
{
    const module_config_t *item = &param->item;
#ifndef _WIN32
# define OPTION_VALUE_SEP " "
#else
# define OPTION_VALUE_SEP "="
#endif
    const char *bra = OPTION_VALUE_SEP "<", *type, *ket = ">";
    const char *prefix = NULL, *suffix = NULL;
    char *typebuf = NULL;

    switch (CONFIG_CLASS(item->i_type))
    {
        case 0: // hint class
            switch (item->i_type)
            {
                case CONFIG_HINT_CATEGORY:
                    printf(color ? TS_GREEN_BOLD "\n %s\n" TS_RESET : "\n %s\n",
                           module_gettext(m, item->psz_text));

                    if (desc && item->psz_longtext != NULL)
                        printf(color ? TS_CYAN_BOLD " %s\n" TS_RESET : " %s\n",
                               module_gettext(m, item->psz_longtext));
                    break;

                case CONFIG_SUBCATEGORY:
                    /* We ignore these here, using 'hints' instead */
                    break;

                case CONFIG_SECTION:
                    *section = item;
                    break;
            }
            return;

        case CONFIG_ITEM_STRING:
        {
            type = _("string");

            char **ppsz_values, **ppsz_texts;

            ssize_t i_count = config_GetPszChoices(item->psz_name, &ppsz_values, &ppsz_texts);

            if (i_count > 0)
            {
                size_t len = 0;

                for (size_t i = 0; i < (size_t)i_count; i++)
                    len += strlen(ppsz_values[i]) + 1;

                typebuf = malloc(len);
                if (typebuf == NULL)
                    goto end_string;

                bra = OPTION_VALUE_SEP "{";
                type = typebuf;
                ket = "}";

                *typebuf = 0;
                for (size_t i = 0; i < (size_t)i_count; i++)
                {
                    if (i > 0)
                        strcat(typebuf, ",");
                    strcat(typebuf, ppsz_values[i]);
                }

            end_string:
                for (size_t i = 0; i < (size_t)i_count; i++)
                {
                    free(ppsz_values[i]);
                    free(ppsz_texts[i]);
                }
                free(ppsz_values);
                free(ppsz_texts);
            }

            break;
        }
        case CONFIG_ITEM_INTEGER:
        {
            type = _("integer");

            int64_t *pi_values;
            char **ppsz_texts;

            ssize_t i_count = config_GetIntChoices(item->psz_name, &pi_values, &ppsz_texts);

            if (i_count > 0)
            {
                size_t len = 0;

                for (size_t i = 0; i < (size_t)i_count; i++)
                    len += strlen(ppsz_texts[i])
                           + 4 * sizeof (int64_t) + 5;

                typebuf = malloc(len);
                if (typebuf == NULL)
                    goto end_integer;

                bra = OPTION_VALUE_SEP "{";
                type = typebuf;
                ket = "}";

                *typebuf = 0;
                for (size_t i = 0; i < (size_t)i_count; i++)
                {
                    if (i != 0)
                        strcat(typebuf, ", ");
                    sprintf(typebuf + strlen(typebuf), "%"PRIi64" (%s)",
                            pi_values[i],
                            ppsz_texts[i]);
                }

            end_integer:
                for (size_t i = 0; i < (size_t)i_count; i++)
                    free(ppsz_texts[i]);
                free(pi_values);
                free(ppsz_texts);
            }
            else if (item->min.i != INT64_MIN || item->max.i != INT64_MAX )
            {
                if (asprintf(&typebuf, "%s [%"PRId64" .. %"PRId64"]",
                             type, item->min.i, item->max.i) >= 0)
                    type = typebuf;
                else
                    typebuf = NULL;
            }
            break;
        }
        case CONFIG_ITEM_FLOAT:
            type = _("float");
            if (item->min.f != -FLT_MAX || item->max.f != FLT_MAX)
            {
                if (asprintf(&typebuf, "%s [%f .. %f]", type,
                             item->min.f, item->max.f) >= 0)
                    type = typebuf;
                else
                    typebuf = NULL;
            }
            break;

        case CONFIG_ITEM_BOOL:
            bra = type = ket = "";
            prefix = ", --no-";
            suffix = item->orig.i ? _("(default enabled)")
                                  : _("(default disabled)");
            break;
       default:
            return;
    }

    print_section(m, section, color, desc);

    /* Add short option if any */
    char shortopt[4];
    if (param->shortname != '\0')
        sprintf(shortopt, "-%c,", param->shortname);
    else
        strcpy(shortopt, "   ");

    if (CONFIG_CLASS(item->i_type) == CONFIG_ITEM_BOOL)
        printf(color ? TS_BOLD "  %s --%s"      "%s%s%s%s%s " TS_RESET
                     : "  %s --%s%s%s%s%s%s ", shortopt, item->psz_name,
               prefix, item->psz_name, bra, type, ket);
    else
        printf(color ? TS_BOLD "  %s --%s" TS_YELLOW_BOLD "%s%s%s%s%s " TS_RESET
                     : "  %s --%s%s%s%s%s%s ", shortopt, item->psz_name,
               "", "",  /* XXX */      bra, type, ket);

    /* Wrap description */
    int offset = PADDING_SPACES - strlen(item->psz_name)
               - strlen(bra) - vlc_swidth(type) - strlen(ket) - 1;
    if (CONFIG_CLASS(item->i_type) == CONFIG_ITEM_BOOL)
        offset -= strlen(item->psz_name) + vlc_swidth(prefix);
    if (offset < 0)
    {
        putchar('\n');
        offset = PADDING_SPACES + LINE_START;
    }

    printf("%*s", offset, "");
    print_desc(module_gettext(m, item->psz_text),
               PADDING_SPACES + LINE_START, color);

    if (suffix != NULL)
    {
        printf("%*s", PADDING_SPACES + LINE_START, "");
        print_desc(suffix, PADDING_SPACES + LINE_START, color);
    }

    if (desc && (item->psz_longtext != NULL && item->psz_longtext[0]))
    {   /* Wrap long description */
        printf("%*s", LINE_START + 2, "");
        print_desc(module_gettext(m, item->psz_longtext),
                   LINE_START + 2, false);
    }

    free(typebuf);
}

static bool module_match(const module_t *m, const char *pattern, bool strict)
{
    if (pattern == NULL)
        return true;

    const char *objname = module_get_object(m);

    if (strict ? (strcmp(objname, pattern) == 0)
               : (strstr(objname, pattern) != NULL))
        return true;

    for (unsigned i = 0; i < m->i_shortcuts; i++)
    {
        const char *shortcut = m->pp_shortcuts[i];

        if (strict ? (strcmp(shortcut, pattern) == 0)
                   : (strstr(shortcut, pattern) != NULL))
            return true;
    }
    return false;
}

static bool plugin_show(const vlc_plugin_t *plugin)
{
    for (size_t i = 0; i < plugin->conf.size; i++)
    {
        const struct vlc_param *param = plugin->conf.params + i;
        const module_config_t *item = &param->item;

        if (!CONFIG_ITEM(item->i_type))
            continue;
        if (param->obsolete)
            continue;
        return true;
    }
    return false;
}

static void Usage (libvlc_int_t *p_this, char const *psz_search)
{
    bool found = false;
    bool strict = false;
    if (psz_search != NULL && psz_search[0] == '=')
    {
        strict = true;
        psz_search++;
    }

    bool color = false;
#ifdef HAVE_ISATTY
    if (isatty(STDOUT_FILENO))
        color = var_InheritBool(p_this, "color");
#endif

    const bool desc = var_InheritBool(p_this, "help-verbose");

    /* Enumerate the config for each module */
    for (const vlc_plugin_t *p = vlc_plugins; p != NULL; p = p->next)
    {
        const module_t *m = p->module;
        const module_config_t *section = NULL;
        const char *objname = module_get_object(m);

        if (psz_search == NULL && p->conf.count == 0)
            continue; /* Ignore modules without config options */
        if (!module_match(m, psz_search, strict))
            continue;
        found = true;

        if (psz_search == NULL && !plugin_show(p))
            continue;

        /* Print name of module */
        printf(color ? "\n " TS_GREEN_BOLD "%s" TS_RESET " (%s)\n" : "\n %s (%s)\n",
               module_gettext(m, m->psz_longname), objname);
        if (m->psz_help != NULL)
            printf(color ? TS_CYAN_BOLD" %s\n" TS_RESET : " %s\n",
                   module_gettext(m, m->psz_help));

        if (psz_search != NULL && p->conf.count == 0)
            printf("  %s\n", _("This module has no options."));

        /* Print module options */
        for (size_t j = 0; j < p->conf.size; j++)
        {
            const struct vlc_param *param = p->conf.params + j;

            if (param->obsolete)
                continue; /* Skip removed options */

            print_item(m, param, &section, color, desc);
        }
    }

    if (!found)
        printf(color ? "\n" TS_BOLD "%s" TS_RESET "\n" : "\n%s\n",
               _("No matching module found. Use --list or "
                 "--list-verbose to list available modules."));
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules (libvlc_int_t *p_this, bool b_verbose)
{
    bool color = false;

    ShowConsole();
#ifdef HAVE_ISATTY
    if (isatty(STDOUT_FILENO))
        color = var_InheritBool(p_this, "color");
#else
    (void) p_this;
#endif

    /* List all modules */
    size_t count;
    module_t **list = module_list_get (&count);

    /* Enumerate each module */
    for (size_t j = 0; j < count; j++)
    {
        module_t *p_parser = list[j];
        const char *objname = module_get_object (p_parser);
        printf(color ? TS_GREEN_BOLD "  %-22s " TS_RESET_BOLD "%s\n" TS_RESET : "  %-22s %s\n",
               objname, module_gettext(p_parser, p_parser->psz_longname));

        if( b_verbose )
        {
            const char *const *pp_shortcuts = p_parser->pp_shortcuts;
            for( unsigned i = 0; i < p_parser->i_shortcuts; i++ )
                if( strcmp( pp_shortcuts[i], objname ) )
                    printf(color ? TS_CYAN_BOLD "   s %s\n" TS_RESET : "   s %s\n",
                           pp_shortcuts[i]);
            if (p_parser->psz_capability != NULL)
                printf(color ? TS_MAGENTA_BOLD "   c %s (%d)\n" TS_RESET : "   c %s (%d)\n",
                       p_parser->psz_capability, p_parser->i_score);
        }
    }
    module_list_free (list);
    PauseConsole();
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
    ShowConsole();
    printf(_("VLC version %s (%s)\n"), VERSION_MESSAGE, psz_vlc_changeset);
    printf(_("Compiled by %s on %s (%s)\n"), VLC_CompileBy(),
           VLC_CompileHost(), __DATE__" "__TIME__ );
    printf(_("Compiler: %s\n"), VLC_Compiler());
    fputs(LICENSE_MSG, stdout);
    PauseConsole();
}

#if defined( _WIN32 ) && !defined(VLC_WINSTORE_APP)
/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
static void ShowConsole( void )
{
    if( getenv( "PWD" ) ) return; /* Cygwin shell or Wine */

    if( !AllocConsole() ) return;

    /* Use the ANSI code page (e.g. Windows-1252) as expected by the LibVLC
     * Unicode/locale subsystem. By default, we have the obsolecent OEM code
     * page (e.g. CP437 or CP850). */
    SetConsoleOutputCP (GetACP ());
    SetConsoleTitle (TEXT("VLC media player version ") TEXT(PACKAGE_VERSION));

    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );

    if( freopen( "vlc-help.txt", "wt", stdout ) != NULL )
    {
        fputs( "\xEF\xBB\xBF", stdout );
        fprintf( stderr, _("\nDumped content to vlc-help.txt file.\n") );
    }
    else
        freopen( "CONOUT$", "w", stdout );
}

/*****************************************************************************
 * PauseConsole: On Win32, wait for a key press before closing the console
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
static void PauseConsole( void )
{
    if( getenv( "PWD" ) ) return; /* Cygwin shell or Wine */

    utf8_fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
    getchar();
    fclose( stdout );
}
#endif
