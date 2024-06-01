/*****************************************************************************
 * slaves.c: test libvlc_media_t and libvlc_media_player_t slaves API
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "test.h"
#include "media_utils.h"

#define SLAVES_DIR SRCDIR "/samples/slaves"

static char *path_to_mrl(const char *psz_path)
{
    libvlc_media_t *p_m = libvlc_media_new_path(psz_path);
    char *psz_mrl = libvlc_media_get_mrl(p_m);
    libvlc_media_release(p_m);
    return psz_mrl;
}

static void
test_expected_slaves(libvlc_media_t *p_m,
                     libvlc_media_slave_t *p_expected_slaves,
                     unsigned int i_expected_slaves)
{
    printf("Check if slaves are correclty attached to media\n");

    libvlc_media_slave_t **pp_slaves;
    unsigned int i_slave_count = libvlc_media_slaves_get(p_m, &pp_slaves);
    assert(i_expected_slaves == i_slave_count);

    unsigned i_found_slaves = 0;
    bool *p_found_list = calloc(i_expected_slaves, sizeof(bool));
    assert(p_found_list != NULL);
    for (unsigned int i = 0; i < i_slave_count; ++i)
    {
        libvlc_media_slave_t *p_slave1 = pp_slaves[i];
        for (unsigned int j = 0; j < i_expected_slaves; ++j)
        {
            libvlc_media_slave_t *p_slave2 = &p_expected_slaves[j];
            if (strcmp(p_slave1->psz_uri, p_slave2->psz_uri) == 0)
            {
                assert(p_found_list[j] == false);
                assert(p_slave1->i_type == p_slave2->i_type);
                assert(p_slave1->i_priority == p_slave2->i_priority);
                p_found_list[j] = true;
                i_found_slaves++;
                break;
            }
        }
    }
    assert(i_expected_slaves == i_found_slaves);
    for (unsigned int i = 0; i < i_expected_slaves; ++i)
    {
        printf("Check if slaves[%d] is found\n", i);
        assert(p_found_list[i]);
    }
    free(p_found_list);

    libvlc_media_slaves_release(pp_slaves, i_slave_count);
}

static void
test_media_has_slaves_from_parent(libvlc_instance_t *p_vlc,
                                  const char *psz_main_media,
                                  libvlc_media_slave_t *p_expected_slaves,
                                  unsigned i_expected_slaves)
{
    libvlc_media_t *p_m = libvlc_media_new_path(SLAVES_DIR);
    assert(p_m != NULL);

    printf("Parse media dir to get subitems\n");
    libvlc_media_parse_sync(p_vlc, p_m, libvlc_media_parse_local, -1);

    char *psz_main_media_mrl = path_to_mrl(psz_main_media);
    assert(psz_main_media_mrl != NULL);
    printf("Main media mrl: '%s'\n", psz_main_media_mrl);

    printf("Fetch main media from subitems\n");
    libvlc_media_list_t *p_ml = libvlc_media_subitems(p_m);
    assert(p_ml != NULL);
    libvlc_media_list_lock(p_ml);
    int i_count = libvlc_media_list_count(p_ml);
    assert(i_count > 0);
    libvlc_media_t *p_subm = NULL;
    for (int i = 0; i < i_count; ++i)
    {
        p_subm = libvlc_media_list_item_at_index(p_ml, i);
        assert(p_subm != NULL);
        char *psz_mrl = libvlc_media_get_mrl(p_subm);
        assert(psz_mrl != NULL);
        if (strcmp(psz_main_media_mrl, psz_mrl) == 0)
        {
            printf("Found main media\n");
            free(psz_mrl);
            break;
        }
        free(psz_mrl);
        libvlc_media_release(p_subm);
        p_subm = NULL;
    }
    free(psz_main_media_mrl);
    libvlc_media_list_unlock(p_ml);
    libvlc_media_list_release(p_ml);

    assert(p_subm != NULL);
    test_expected_slaves(p_subm, p_expected_slaves, i_expected_slaves);
    libvlc_media_release(p_subm);

    libvlc_media_release(p_m);
}

int
main (void)
{
    test_init();

    const char *pp_slave_paths[] = {
        SLAVES_DIR "/test.aac",
        SLAVES_DIR "/test.rt.srt",
        SLAVES_DIR "/lt-test.srt",
        SLAVES_DIR "/nomatch.srt",
    };

    libvlc_media_slave_t p_expected_slaves[] = {
        { NULL, libvlc_media_slave_type_generic, 3 /* all */ },
        { NULL, libvlc_media_slave_type_subtitle, 2 /* right */ },
        { NULL, libvlc_media_slave_type_subtitle, 1 /* left */ },
        { NULL, libvlc_media_slave_type_subtitle, 0 /* none */ },
    };

    static_assert(ARRAY_SIZE(pp_slave_paths) == ARRAY_SIZE(p_expected_slaves),
                  "pp_slave_paths and p_expected_slaves mismatch");

    const char *pp_args[] = {
        "-v", "--sub-autodetect-fuzzy", "1",
        "--no-video", "--no-audio",
        "--codec", "none", /* to ensure we don't depend on codec modules */
        NULL /* "sub-autodetect-file" place holder */
    };

    libvlc_instance_t *p_vlc = libvlc_new(ARRAY_SIZE(pp_args) - 1, pp_args);
    assert(p_vlc != NULL);

    /* Fill p_expected_slaves with correct VLC mrls */
    for (unsigned int i = 0; i < ARRAY_SIZE(p_expected_slaves); ++i)
    {
        p_expected_slaves[i].psz_uri = path_to_mrl(pp_slave_paths[i]);
        assert(p_expected_slaves[i].psz_uri != NULL);
    }

    printf("== Testing --sub-autodetect-fuzzy 1 (everything) ==\n");
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/test.mp4",
                                      p_expected_slaves,
                                      ARRAY_SIZE(p_expected_slaves));
    libvlc_release(p_vlc);

    printf("== Testing --sub-autodetect-fuzzy 2 (full, left, and right match) ==\n");
    pp_args[2] = "2";
    p_vlc = libvlc_new(ARRAY_SIZE(pp_args) - 1, pp_args);
    assert(p_vlc != NULL);
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/test.mp4",
                                      p_expected_slaves, 3);

    printf("== Testing if the matching is not too permissive  ==\n");
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/t.mp4",
                                      NULL, 0);
    libvlc_release(p_vlc);

    printf("== Testing --sub-autodetect-fuzzy 3 (full and left match) ==\n");
    pp_args[2] = "3";
    p_vlc = libvlc_new(ARRAY_SIZE(pp_args) - 1, pp_args);
    assert(p_vlc != NULL);
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/test.mp4",
                                      p_expected_slaves, 2);
    libvlc_release(p_vlc);

    printf("== Testing --sub-autodetect-fuzzy 4 (full match) ==\n");
    pp_args[2] = "4";
    p_vlc = libvlc_new(ARRAY_SIZE(pp_args) - 1, pp_args);
    assert(p_vlc != NULL);
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/test.mp4",
                                      p_expected_slaves, 1);
    libvlc_release(p_vlc);

    printf("== Testing  --no-sub-autodetect-file (no match) ==\n");
    pp_args[ARRAY_SIZE(pp_args) - 1] = "--no-sub-autodetect-file";
    p_vlc = libvlc_new(ARRAY_SIZE(pp_args), pp_args);
    assert(p_vlc != NULL);
    test_media_has_slaves_from_parent(p_vlc, SLAVES_DIR "/test.mp4", NULL, 0);
    libvlc_release(p_vlc);

    for (unsigned int i = 0; i < ARRAY_SIZE(p_expected_slaves); ++i)
        free(p_expected_slaves[i].psz_uri);

    return 0;
}
