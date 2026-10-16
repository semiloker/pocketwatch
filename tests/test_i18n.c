// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pwtest.h"
#include "ui/i18n.h"

static const char *const g_shipped[] = {"fr", "es", "de", "it", "nl", "pt", "ru", "uk"};

PW_TEST(i18n_english_table_is_complete_and_keys_unique)
{
    pw_i18n_reset();
    for (int i = 0; i < PW_S_COUNT; i++) {
        CHECK(pw_i18n_key(i) != NULL);
        CHECK(pw_i18n_english(i) != NULL && pw_i18n_english(i)[0] != 0);
        CHECK_EQ(strcmp(pw_tr(i), pw_i18n_english(i)), 0);
        for (int j = 0; j < i; j++)
            CHECK(strcmp(pw_i18n_key(i), pw_i18n_key(j)) != 0);
    }
    CHECK_EQ(strcmp(pw_tr(-1), ""), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_COUNT), ""), 0);
}

PW_TEST(i18n_format_check)
{
    CHECK(pw_i18n_same_format("%d of %d", "Занято: %d из %d"));
    CHECK(pw_i18n_same_format("Saving... %d%%", "Guardando... %d%%"));
    CHECK(pw_i18n_same_format("%s   %u.%u MB", "%s   %u,%u Mo"));
    CHECK(!pw_i18n_same_format("Saved at %s", "Saved at %d"));
    CHECK(!pw_i18n_same_format("Save State %d", "Save State"));
    CHECK(!pw_i18n_same_format("%d of %d", "%d"));
    CHECK(!pw_i18n_same_format("100%", "100%"));
}

static int ascii_only(const char *s, void *ctx)
{
    (void)ctx;
    for (; *s; s++)
        if ((unsigned char)*s > 126)
            return 0;
    return 1;
}

PW_TEST(i18n_load_parses_skips_and_falls_back)
{
    char text[] = "\xEF\xBB\xBF# comment = not a key\r\n"
                  "yes =  Oui \r\n"
                  "no=Non\n"
                  "unknown_key = whatever\n"
                  "slot_title = Sauvegarde %s\n" /* wrong conversion: skipped */
                  "slots_used = %d sur %d\n"
                  "\n"
                  "back = Retour";
    int n = pw_i18n_load(text, sizeof(text) - 1, NULL, NULL);

    CHECK_EQ(n, 4);
    CHECK_EQ(strcmp(pw_tr(PW_S_YES), "Oui"), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_NO), "Non"), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_BACK), "Retour"), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_SLOTS_USED), "%d sur %d"), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_SLOT_TITLE), "Save State %d"), 0); /* stayed English */
    CHECK_EQ(strcmp(pw_tr(PW_S_ENTER), "Enter"), 0);
    pw_i18n_reset();
    CHECK_EQ(strcmp(pw_tr(PW_S_YES), "Yes"), 0);
}

PW_TEST(i18n_load_refuses_text_the_font_cannot_draw)
{
    char ru[] = "yes = Да\nno = Нет\n";
    char none[] = "# nothing\nfoo = bar\n";

    CHECK_EQ(pw_i18n_load(ru, sizeof(ru) - 1, ascii_only, NULL), -2);
    CHECK_EQ(strcmp(pw_tr(PW_S_YES), "Yes"), 0);
    CHECK_EQ(strcmp(pw_tr(PW_S_NO), "No"), 0);
    CHECK_EQ(pw_i18n_load(none, sizeof(none) - 1, NULL, NULL), -1);
}

PW_TEST(i18n_refuses_a_file_larger_than_the_pool)
{
    static char big[PW_I18N_POOL * 2];
    int n = 0;

    for (int i = 0; i < PW_S_COUNT && n < (int)sizeof(big) - 200; i++) {
        n += snprintf(big + n, sizeof(big) - (size_t)n, "%s = ", pw_i18n_key(i));
        for (int k = 0; k < 120; k++)
            big[n++] = 'a';
        n += snprintf(big + n, sizeof(big) - (size_t)n, " %s\n", pw_i18n_english(i));
    }
    CHECK_EQ(pw_i18n_load(big, (unsigned int)n, NULL, NULL), -3);
    CHECK_EQ(strcmp(pw_tr(PW_S_YES), "Yes"), 0);
}

PW_TEST(i18n_system_language_codes)
{
    CHECK(pw_i18n_code(0) == NULL); /* Japanese: needs jpn0 */
    CHECK(pw_i18n_code(1) == NULL); /* English: built in */
    CHECK_EQ(strcmp(pw_i18n_code(2), "fr"), 0);
    CHECK_EQ(strcmp(pw_i18n_code(4), "de"), 0);
    CHECK_EQ(strcmp(pw_i18n_code(8), "ru"), 0);
    CHECK(pw_i18n_code(9) == NULL);
    CHECK(pw_i18n_code(-1) == NULL);
    CHECK(pw_i18n_code(99) == NULL);
}

/* Every shipped translation: every key once, no unknown or broken line, formats as in English. */
PW_TEST(i18n_shipped_translations_are_complete)
{
    for (unsigned int k = 0; k < sizeof(g_shipped) / sizeof(g_shipped[0]); k++) {
        char path[128], line[512];
        int seen[PW_S_COUNT] = {0};
        FILE *f;
        long size;
        char *text;

        snprintf(path, sizeof(path), "../plugin/lang/%s.txt", g_shipped[k]);
        f = fopen(path, "rb");
        CHECK(f != NULL);
        if (f == NULL)
            continue;
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '='), *key = line, *end;
            int found = -1;

            if (line[0] == '#' || line[0] == '\n')
                continue;
            CHECK(eq != NULL);
            if (eq == NULL) {
                fprintf(stderr, "  %s: broken line: %s", path, line);
                continue;
            }
            end = eq;
            while (end > key && end[-1] == ' ')
                end--;
            *end = 0;
            for (int i = 0; i < PW_S_COUNT; i++)
                if (strcmp(key, pw_i18n_key(i)) == 0)
                    found = i;
            if (found < 0)
                fprintf(stderr, "  %s: unknown key %s\n", path, key);
            CHECK(found >= 0);
            if (found >= 0) {
                seen[found]++;
                if (!pw_i18n_same_format(eq + 1, pw_i18n_english(found)))
                    fprintf(stderr, "  %s: %s has other conversions than English\n", path, key);
                CHECK(pw_i18n_same_format(eq + 1, pw_i18n_english(found)));
            }
        }
        for (int i = 0; i < PW_S_COUNT; i++) {
            if (seen[i] != 1)
                fprintf(stderr, "  %s: %s appears %d times\n", path, pw_i18n_key(i), seen[i]);
            CHECK_EQ(seen[i], 1);
        }

        /* and the plugin's own loader takes all of it */
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        text = malloc((size_t)size + 1);
        CHECK_EQ(fread(text, 1, (size_t)size, f), (size_t)size);
        CHECK_EQ(pw_i18n_load(text, (unsigned int)size, NULL, NULL), PW_S_COUNT);
        memset(text, 'x', (size_t)size); /* the texts were copied */
        CHECK(strchr(pw_tr(PW_S_YES), 'x') == NULL || strcmp(g_shipped[k], "en") == 0);
        pw_i18n_reset();
        free(text);
        fclose(f);
    }
}
