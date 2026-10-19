// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_LIBRARY_H
#define HM_LIBRARY_H

#include "home_view.h"

/*
 * The games on the card (HOM-REQ-001, -002, -008): every .iso / .cso in ms0:/ISO and its folders,
 * and every EBOOT.PBP in ms0:/PSP/GAME/<folder>/ (homebrew, PS1 games), with the title and ID
 * from its PARAM.SFO, what Pocketwatch knows of it (play time, save states), and - loaded by a
 * thread, nearest to the chosen one first - its ICON0 and, once the choice settles, its PIC1 as
 * a background. PSP games first, then PS1 games, then homebrew, each by title.
 */

enum { HM_KIND_ISO, HM_KIND_PSN, HM_KIND_PS1, HM_KIND_APP };

#define HM_GAMES_MAX 96

typedef struct {
    int kind;       /* HM_KIND_* */
    char path[192]; /* the image, or the EBOOT.PBP */
    char id[16];
    char title[64];
    char meta[96];
    unsigned int play_s;
    unsigned long long last;    /* when it was last played (play-time file), 0: never */
    char version[8], sysver[8]; /* PARAM.SFO DISC_VERSION, PSP_SYSTEM_VER (Information) */
    unsigned int size_kb;
    int states;
    unsigned int *volatile icon; /* HM_ICON_W x HM_ICON_H, set by the loader */
    volatile int icon_tried;
} hm_game;

int hm_library_scan(void); /* returns how many games */
/* The order: by name (0) or the last played first (1); again now (the loader paused). */
void hm_library_sort(int recent);
int hm_library_count(void);
int hm_library_has(const char *image, const char *inner); /* the image has that file */
hm_game *hm_library_game(int i);

/* Start the loader thread; tell it which game is chosen. */
void hm_library_start(void);
void hm_library_focus(int sel);

/* The chosen game's background (HM_W x HM_H, stride 512) once loaded for that game, or NULL. */
const unsigned int *hm_library_background(int sel);

/* USB (HOM-REQ-011): stop the loader's card reads (returns once it is idle) or let it go on. */
void hm_library_pause(int on);
/* After USB: the card may have changed - list it again (loader paused). Returns the count. */
int hm_library_rescan(void);

/* Photos (HOM-REQ-016): ms0:/PICTURE, ms0:/DCIM and the screenshots of ms0:/PSP/SCREENSHOT,
   two folder levels, in albums by folder (HOM-REQ-025), newest first. */
#define HM_PHOTOS_MAX 400
#define HM_ALBUMS_MAX 64

typedef struct {
    char path[192];
    char name[64];
    char meta[48];                /* its folder (a game's ID for screenshots) and date */
    unsigned int *volatile thumb; /* HM_ICON_W x HM_ICON_H, kept in shape */
    volatile int tried;
    int album;         /* its album (1..): 0 is All Photos */
    volatile int w, h; /* its size, once the loader has decoded it (0 before) */
    unsigned long long when;
} hm_photo;

/* An album: the pictures of one folder - a game's screenshots under the game's title. */
typedef struct {
    char folder[48]; /* the folder's name ("" for All Photos) */
    char title[64];  /* the game's title when the folder is a disc ID, else the folder */
    int count;
    int cover; /* its newest picture */
    int game;  /* the game (hm_library_game) whose screenshots these are, or -1 */
} hm_album;

int hm_library_photo_count(void);
hm_photo *hm_library_photo(int i);
int hm_library_album_count(void); /* All Photos (0) and the folders */
hm_album *hm_library_album(int i);
/* The pictures of album a, newest first, into out (at most cap): their indices. Returns how many.
 */
int hm_library_album_photos(int a, int *out, int cap);
/* A picture gone (deleted): out of the lists. */
void hm_library_photo_removed(int i);
/* A new picture (an edited copy): into the lists. Returns its index or < 0. */
int hm_library_photo_added(const char *path);

/* What the loader works for: the games (0), the photos (1), a photo shown whole (2); and which.
   For the photos, the pictures on screen in their order (thumbnails near pos first). */
void hm_library_mode(int mode, int sel);
void hm_library_photo_view(const int *list, int n, int pos);

/* Pictures the loader makes when it has nothing nearer to do (a game's saved data): each job's
   PNG into an icon-sized texture, nearest to focus first. */
typedef struct {
    char path[128];
    unsigned int *volatile icon;
    volatile int tried;
} hm_pic_job;
void hm_library_pics(hm_pic_job *jobs, int n, int focus);
void hm_library_pics_focus(int focus);
/* The cover of a track (HOM-REQ-027): its tag's picture (pic_len bytes at pic_off of the MP3),
   else cover.jpg / folder.jpg beside it - a HM_COVER square and a blurred background, made by
   the loader. NULL until made, or when there is none. */
void hm_library_cover(const char *mp3, unsigned int pic_off, unsigned int pic_len);
const unsigned int *hm_library_cover_pixels(void);
const unsigned int *hm_library_cover_background(void);

/* A file as the background (a save's PIC1): blurred, or sharp (Classic); NULL until made. */
void hm_library_file_bg(const char *path);
const unsigned int *hm_library_file_background(void);
/* The background the loader made for the chosen photo (blurred, or whole when viewing), or NULL. */
const unsigned int *hm_library_photo_background(int sel, int whole);

/* The games' backgrounds as the style wants them: sharp, as the XMB shows a PIC1 (Classic), or
   blurred (1 / 0); those made already are made again. */
void hm_library_style(int sharp);

/* A picture file fitted as Home's background (blurred, darkened) into out (512 x HM_H). */
int hm_library_wallpaper(const char *path, unsigned int *out);

/* The card's free space in MB, counted by the loader (-1 until then); count it again. */
int hm_library_free_mb(void);
void hm_library_count_free(void);

#endif
