// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The state slots on the Memory Stick (complications/states/store.c) against a fake engine and a
 * fake game; ms0:/ maps to $PWTEST_TMPDIR/ms0 (platform_host.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "complications/states/engine.h"
#include "complications/states/savedata_hash.h"
#include "complications/states/store.h"
#include "core/crc32.h"
#include "core/game.h"
#include "core/log.h"
#include "core/mem.h"
#include "pwtest.h"

/* ---- fakes ---- */

#define FAKE_HEADER 256
#define FAKE_IMAGE  4096
#define FAKE_ARENA  (3 * 1024 * 1024 + 123) /* more than one 1 MB piece, not a multiple */

static pw_game g_fake_game;
static struct {
    int has;
    unsigned char header[FAKE_HEADER];
    unsigned char image[FAKE_IMAGE];
    unsigned char *arena;
    unsigned int arena_used;
    unsigned int env_build;
    int staged; /* the real engine's staging block (and partition sizes) exist */
    unsigned int user[PW_ENGINE_USER_WORDS];
    int imports, adopts, env_logs;
} E;

const pw_game *pw_game_get(void)
{
    return &g_fake_game;
}

int pw_mem_alloc(pw_mem_block *b, const char *name, unsigned int size)
{
    (void)name;
    b->ptr = calloc(1, size);
    b->size = size;
    b->block = b->ptr ? 1 : -1;
    return b->ptr ? 0 : -1;
}

int pw_mem_alloc_high(pw_mem_block *b, int partition, const char *name, unsigned int size)
{
    (void)partition;
    return pw_mem_alloc(b, name, size);
}

void pw_mem_free(pw_mem_block *b)
{
    free(b->ptr);
    b->ptr = NULL;
    b->block = -1;
}

int pw_savedata_hash_init(void)
{
    return 0;
}

int pw_savedata_hash_sfo(unsigned char *sfo, unsigned int len, unsigned int params)
{
    (void)sfo;
    (void)len;
    (void)params;
    return 0;
}

void pw_savedata_hash_done(void)
{
}

int pw_engine_has_state(void)
{
    return E.has;
}

unsigned int *pw_engine_user_area(void)
{
    return E.has ? E.user : NULL;
}

void pw_engine_set_env_extra(int index, unsigned int value)
{
    (void)index;
    (void)value;
}

/* Like the real one, part of the environment exists only once the engine has staged: a fresh
   boot that compared before staging refused every state from the card (device, 2026-10-17). */
void pw_engine_env_now(pw_engine_env *e)
{
    memset(e, 0, sizeof(*e));
    e->layout = 0x1234;
    e->build = E.env_build;
    e->stage = E.staged ? 0x8A000000u : 0;
}

int pw_engine_env_for_load(pw_engine_env *e)
{
    E.staged = 1;
    pw_engine_env_now(e);
    return 0;
}

void pw_engine_env_log(const char *who, const pw_engine_env *saved, const pw_engine_env *now)
{
    (void)who;
    (void)saved;
    (void)now;
    E.env_logs++;
}

int pw_engine_export(pw_state_parts *p)
{
    if (!E.has)
        return -1;
    p->header = E.header;
    p->header_len = FAKE_HEADER;
    p->image = E.image;
    p->image_len = FAKE_IMAGE;
    p->arena = E.arena;
    p->arena_len = E.arena_used;
    return 0;
}

int pw_engine_import(pw_state_parts *p)
{
    E.imports++;
    E.has = 0;
    memset(E.header, 0, sizeof(E.header));
    memset(E.image, 0, sizeof(E.image));
    memset(E.arena, 0, FAKE_ARENA);
    p->header = E.header;
    p->header_len = FAKE_HEADER;
    p->image = E.image;
    p->image_len = FAKE_IMAGE;
    p->arena = E.arena;
    p->arena_len = FAKE_ARENA;
    return 0;
}

/* The real adopt checks the header's own checksums; the fake keeps them in its first words. */
static void seal(void)
{
    unsigned int sums[2] = {pw_sum32(E.image, FAKE_IMAGE), pw_sum32(E.arena, E.arena_used)};

    memcpy(E.header, sums, sizeof(sums));
    memcpy(E.header + 8, &E.arena_used, 4);
}

int pw_engine_adopt(void)
{
    unsigned int sums[2], used;

    E.adopts++;
    memcpy(sums, E.header, sizeof(sums));
    memcpy(&used, E.header + 8, 4);
    if (used > FAKE_ARENA || sums[0] != pw_sum32(E.image, FAKE_IMAGE) ||
        sums[1] != pw_sum32(E.arena, used))
        return PW_ENGINE_E_DAMAGED;
    E.arena_used = used;
    E.has = 1;
    return 0;
}

/* ---- helpers ---- */

static void fresh_card(void)
{
    char cmd[600];
    const char *tmp = getenv("PWTEST_TMPDIR");

    snprintf(cmd, sizeof(cmd), "rm -rf '%s/ms0' && mkdir -p '%s/ms0/PSP'", tmp ? tmp : ".",
             tmp ? tmp : ".");
    CHECK_EQ(system(cmd), 0);
}

static void setup(void)
{
    fresh_card();
    memset(&g_fake_game, 0, sizeof(g_fake_game));
    g_fake_game.ok = 1;
    strcpy(g_fake_game.id, "ULUS10277");
    strcpy(g_fake_game.title, "Test Game");
    strcpy(g_fake_game.iso, "ms0:/ISO/TEST.ISO");
    g_fake_game.iso_size = 123456789;
    g_fake_game.parental = 1;
    if (E.arena == NULL)
        E.arena = malloc(FAKE_ARENA);
    E.has = 0;
    E.staged = 0;
    E.imports = E.adopts = E.env_logs = 0;
    E.env_build = 7;
    CHECK_EQ(pw_store_init(), 0);
}

static void make_state(unsigned int seed)
{
    for (unsigned int i = 0; i < FAKE_IMAGE; i++)
        E.image[i] = (unsigned char)(i * 7 + seed);
    E.arena_used = FAKE_ARENA - 1000;
    for (unsigned int i = 0; i < E.arena_used; i++)
        E.arena[i] = (unsigned char)((i >> 3) ^ seed);
    seal();
    E.has = 1;
    E.staged = 1; /* a state in RAM sits in the staging block */
}

static unsigned int frame[272 * 512];

static void make_frame(void)
{
    for (int y = 0; y < 272; y++)
        for (int x = 0; x < 512; x++)
            frame[y * 512 + x] = 0xFF000000u | (unsigned)(x & 0xFF) | (unsigned)(y & 0xFF) << 8;
}

static int file_exists(const char *ms_path)
{
    char p[600];
    struct stat st;
    const char *tmp = getenv("PWTEST_TMPDIR");

    snprintf(p, sizeof(p), "%s/ms0/%s", tmp ? tmp : ".", ms_path + 5);
    return stat(p, &st) == 0;
}

static void corrupt(const char *ms_path, long offset)
{
    char p[600];
    const char *tmp = getenv("PWTEST_TMPDIR");
    FILE *f;
    int c;

    snprintf(p, sizeof(p), "%s/ms0/%s", tmp ? tmp : ".", ms_path + 5);
    f = fopen(p, "r+b");
    CHECK(f != NULL);
    fseek(f, offset, SEEK_SET);
    c = fgetc(f);
    fseek(f, offset, SEEK_SET);
    fputc(c ^ 0x5A, f);
    fclose(f);
}

static int g_progress_last, g_progress_calls;

static void progress(int percent, void *ctx)
{
    (void)ctx;
    CHECK(percent >= g_progress_last);
    g_progress_last = percent;
    g_progress_calls++;
}

/* ---- tests ---- */

PW_TEST(store_save_writes_native_savedata_folder)
{
    pw_slot_info info[PW_STORE_SLOTS];
    static unsigned int icons[PW_STORE_SLOTS * PW_STORE_ICON_W * PW_STORE_ICON_H];

    setup();
    make_frame();
    make_state(1);
    g_progress_last = g_progress_calls = 0;
    CHECK_EQ(pw_store_save(1, frame, 512, progress, NULL), 0);
    CHECK_EQ(g_progress_last, 100);
    CHECK(g_progress_calls >= 4);
    CHECK(file_exists("ms0:/PSP/SAVEDATA/PWULUS1027702/PARAM.SFO"));
    CHECK(file_exists("ms0:/PSP/SAVEDATA/PWULUS1027702/ICON0.PNG"));
    CHECK(file_exists("ms0:/PSP/SAVEDATA/PWULUS1027702/PIC1.PNG"));
    CHECK(file_exists("ms0:/PSP/SAVEDATA/PWULUS1027702/DATA.BIN"));
    CHECK(file_exists("ms0:/PSP/SAVEDATA/PWULUS1027702/STATE.BIN"));
    CHECK_EQ(pw_store_held_slot(), 1);

    pw_store_scan(info, icons);
    for (int i = 0; i < PW_STORE_SLOTS; i++)
        CHECK_EQ(info[i].exists, i == 1);
    CHECK_EQ(info[1].size, FAKE_HEADER + FAKE_IMAGE + E.arena_used);
    CHECK_EQ(info[1].has_icon, 1);
    CHECK(info[1].saved.year >= 2024);
    /* the icon is the frame scaled down: red follows x, green follows y */
    CHECK((info[1].icon[0] & 0xFF) < 8);
    CHECK((info[1].icon[PW_STORE_ICON_W - 1] & 0xFF) > 200);
    pw_store_shutdown();
}

PW_TEST(store_load_reads_back_the_same_state)
{
    unsigned char *want = malloc(FAKE_ARENA);
    unsigned int used;

    setup();
    make_state(9);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    /* still in RAM: no read */
    CHECK_EQ(pw_store_load(0, NULL, NULL), 0);
    CHECK_EQ(E.imports, 0);

    memcpy(want, E.arena, FAKE_ARENA);
    used = E.arena_used;
    E.has = 0; /* the engine dropped it (after a load) */
    CHECK_EQ(pw_store_held_slot(), -1);
    CHECK_EQ(pw_store_load(0, NULL, NULL), 0);
    CHECK_EQ(E.imports, 1);
    CHECK_EQ(E.adopts, 1);
    CHECK_EQ(E.arena_used, used);
    CHECK(memcmp(E.arena, want, used) == 0);
    CHECK_EQ(pw_store_held_slot(), 0);
    free(want);
    pw_store_shutdown();
}

PW_TEST(store_interrupted_save_leaves_an_empty_slot)
{
    pw_slot_info info[PW_STORE_SLOTS];
    char zero[64] = {0};

    setup();
    make_state(3);
    CHECK_EQ(pw_store_save(2, NULL, 0, NULL, NULL), 0);
    /* a save that died before its last step left STATE.BIN zeroed */
    CHECK(pw_file_write("ms0:/PSP/SAVEDATA/PWULUS1027703/STATE.BIN", zero, sizeof(zero), 0) > 0);
    pw_store_scan(info, NULL);
    CHECK_EQ(info[2].exists, 0);
    E.has = 0;
    CHECK_EQ(pw_store_load(2, NULL, NULL), PW_STORE_E_EMPTY);
    CHECK_EQ(E.imports, 0); /* nothing was dropped for it */
    pw_store_shutdown();
}

PW_TEST(store_refuses_another_game_iso_or_build)
{
    pw_slot_info info[PW_STORE_SLOTS];

    setup();
    make_state(4);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    E.has = 0;

    strcpy(g_fake_game.iso, "ms0:/ISO/OTHER.ISO");
    CHECK_EQ(pw_store_load(0, NULL, NULL), PW_STORE_E_OTHER_GAME);
    strcpy(g_fake_game.iso, "ms0:/ISO/TEST.ISO");
    g_fake_game.iso_size++;
    CHECK_EQ(pw_store_load(0, NULL, NULL), PW_STORE_E_OTHER_GAME);
    g_fake_game.iso_size--;
    CHECK_EQ(E.imports, 0);

    /* another build or memory layout: refused before the state in RAM is dropped */
    pw_store_init(); /* forgets which slot RAM holds */
    E.has = 1;
    E.env_build = 8;
    CHECK_EQ(pw_store_load(0, NULL, NULL), PW_STORE_E_OTHER_SETUP);
    CHECK_EQ(E.imports, 0);
    CHECK_EQ(E.has, 1);
    E.env_build = 7;
    E.has = 0;

    /* another game's folder name never matches; its slots look empty */
    strcpy(g_fake_game.id, "NPJH50000");
    pw_store_init();
    pw_store_scan(info, NULL);
    CHECK_EQ(info[0].exists, 0);
    pw_store_shutdown();
}

PW_TEST(store_detects_damaged_files)
{
    setup();
    make_state(5);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    E.has = 0;
    corrupt("ms0:/PSP/SAVEDATA/PWULUS1027701/DATA.BIN", 100); /* in the header */
    CHECK_EQ(pw_store_load(0, NULL, NULL), PW_STORE_E_DAMAGED);
    CHECK_EQ(pw_store_held_slot(), -1);

    make_state(5);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    E.has = 0;
    corrupt("ms0:/PSP/SAVEDATA/PWULUS1027701/DATA.BIN", FAKE_HEADER + FAKE_IMAGE + 2000000);
    CHECK_EQ(pw_store_load(0, NULL, NULL), PW_STORE_E_DAMAGED);
    CHECK_EQ(E.has, 0);
    pw_store_shutdown();
}

PW_TEST(store_delete_removes_the_folder)
{
    pw_slot_info info[PW_STORE_SLOTS];

    setup();
    make_frame();
    make_state(6);
    CHECK_EQ(pw_store_save(4, frame, 512, NULL, NULL), 0);
    CHECK_EQ(pw_store_delete(4), 0);
    CHECK(!file_exists("ms0:/PSP/SAVEDATA/PWULUS1027705"));
    CHECK_EQ(pw_store_held_slot(), -1);
    pw_store_scan(info, NULL);
    CHECK_EQ(info[4].exists, 0);
    E.has = 0;
    CHECK_EQ(pw_store_load(4, NULL, NULL), PW_STORE_E_EMPTY);
    pw_store_shutdown();
}

PW_TEST(store_without_game_id_stays_off)
{
    pw_slot_info info[PW_STORE_SLOTS];

    setup();
    g_fake_game.ok = 0;
    g_fake_game.id[0] = 0;
    CHECK_EQ(pw_store_init(), PW_STORE_E_NO_GAME);
    CHECK_EQ(pw_store_available(), 0);
    make_state(7);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), PW_STORE_E_NO_GAME);
    pw_store_scan(info, NULL);
    CHECK_EQ(info[0].exists, 0);
    pw_store_shutdown();
}

PW_TEST(store_check_after_load_sees_the_marker)
{
    setup();
    make_state(8);
    CHECK_EQ(pw_store_save(3, NULL, 0, NULL, NULL), 0);

    /* not armed (a state that is not on the card): nothing to check */
    pw_store_arm(-1);
    CHECK_EQ(pw_store_after_load(E.user, PW_ENGINE_USER_WORDS), 0);
    CHECK_EQ(pw_store_check_failed(), 0);

    pw_store_arm(3);
    CHECK_EQ(pw_store_after_load(E.user, PW_ENGINE_USER_WORDS), 0);
    CHECK_EQ(pw_store_check_failed(), 0);
    CHECK_EQ(pw_store_after_load(E.user, PW_ENGINE_USER_WORDS), 0); /* disarmed by the first */

    /* a stale view of the card: the marker of this save is not there */
    pw_store_arm(3);
    {
        char cmd[600];
        const char *tmp = getenv("PWTEST_TMPDIR");

        snprintf(cmd, sizeof(cmd), "rm -f '%s'/ms0/PSP/SAVEDATA/PWULUS1027704/PW*.BIN",
                 tmp ? tmp : ".");
        CHECK_EQ(system(cmd), 0);
    }
    CHECK(pw_store_after_load(E.user, PW_ENGINE_USER_WORDS) < 0);
    CHECK_EQ(pw_store_check_failed(), 1);

    /* a second save of the slot replaces its marker */
    CHECK_EQ(pw_store_save(3, NULL, 0, NULL, NULL), 0);
    pw_store_arm(3);
    CHECK_EQ(pw_store_after_load(E.user, PW_ENGINE_USER_WORDS), 0);
    pw_store_shutdown();
}

PW_TEST(store_refuses_writes_while_the_card_is_blocked)
{
    setup();
    make_state(2);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    pw_log_set_card_blocked(1);
    CHECK_EQ(pw_store_save(1, NULL, 0, NULL, NULL), PW_STORE_E_BLOCKED);
    CHECK_EQ(pw_store_delete(0), PW_STORE_E_BLOCKED);
    pw_log_set_card_blocked(0);
    CHECK_EQ(pw_store_delete(0), 0);
    pw_store_shutdown();
}

PW_TEST(store_loads_a_state_from_an_earlier_boot)
{
    setup();
    make_state(9);
    CHECK_EQ(pw_store_save(0, NULL, 0, NULL, NULL), 0);
    /* Quit Game and start again: nothing in RAM, nothing staged */
    pw_store_shutdown();
    E.has = 0;
    E.staged = 0;
    CHECK_EQ(pw_store_init(), 0);
    CHECK_EQ(pw_store_load(0, NULL, NULL), 0);
    CHECK_EQ(E.imports, 1);
    CHECK_EQ(E.env_logs, 0);
    pw_store_shutdown();
}
