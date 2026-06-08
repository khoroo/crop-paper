// SPDX-License-Identifier: MIT
// crop-paper — crop images to a specified aspect ratio

#define _POSIX_C_SOURCE 200809L

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fontconfig/fontconfig.h>

#define MIN_CROP 16

typedef struct { int key; int val; } LastAction;

// ── palette ─────────────────────────────────────────────────────────
static const Color CROP_OUTLINE = { 0xcc, 0x24, 0x1d, 255 };
static const Color HINT_COLOR   = { 200, 200, 200, 255 };


// ── geometry ────────────────────────────────────────────────────────

typedef struct {
    int x, y, w, h;
} Rect;

static Rect init_crop_rect(int img_w, int img_h, int aspect_w, int aspect_h)
{
    Rect r;
    if ((float)img_w / img_h > (float)aspect_w / aspect_h) {
        r.h = img_h;
        r.w = (int)roundf((float)img_h * aspect_w / aspect_h);
    } else {
        r.w = img_w;
        r.h = (int)roundf((float)img_w * aspect_h / aspect_w);
    }
    r.x = (img_w - r.w) / 2;
    r.y = (img_h - r.h) / 2;
    return r;
}

static void clamp_rect(Rect *r, int img_w, int img_h)
{
    if (r->x < 0) r->x = 0;
    if (r->y < 0) r->y = 0;
    if (r->x + r->w > img_w) r->x = img_w - r->w;
    if (r->y + r->h > img_h) r->y = img_h - r->h;
}

static void nudge(Rect *r, int img_w, int img_h, int dx, int dy)
{
    r->x += dx;
    r->y += dy;
    clamp_rect(r, img_w, img_h);
}

// ── proportional centre-based scaling ───────────────────────────────

static void scale_from_centre(Rect *r, int img_w, int img_h,
                              int aw, int ah, int dir, int inc)
{
    int nw = r->w + dir * inc;
    if (nw < MIN_CROP) return;
    int nh = (int)roundf((float)nw * ah / aw);
    if (nh < MIN_CROP) return;
    if (nw > img_w || nh > img_h) return;

    int cx = r->x + r->w / 2;
    int cy = r->y + r->h / 2;
    r->x = cx - nw / 2;
    r->y = cy - nh / 2;
    r->w = nw;
    r->h = nh;
    clamp_rect(r, img_w, img_h);
}

// ── drawing ─────────────────────────────────────────────────────────

static void draw_overlay(int sw, int sh, Rect const *r,
                         float scale, int ox, int oy)
{
    int cx = ox + (int)(r->x * scale);
    int cy = oy + (int)(r->y * scale);
    int cw = (int)(r->w * scale);
    int ch = (int)(r->h * scale);

    Color dim = { 0, 0, 0, 100 };

    DrawRectangle(0, 0, sw, cy, dim);
    DrawRectangle(0, cy + ch, sw, sh - cy - ch, dim);
    DrawRectangle(0, cy, cx, ch, dim);
    DrawRectangle(cx + cw, cy, sw - cx - cw, ch, dim);

    DrawRectangleLinesEx(
        (Rectangle) { (float)cx, (float)cy, (float)cw, (float)ch },
        2, CROP_OUTLINE);
}

#define BAR_FS 28
#define BAR_BH (BAR_FS + 16)

static void draw_info_bar(Font font, int sw, int sh, Rect const *r,
                          int aw, int ah, int count,
                          bool crop_view)
{
    int bh = BAR_BH;
    int y_base = sh - bh;

    DrawRectangle(0, y_base, sw, bh, BLACK);
    int y = y_base + (bh - BAR_FS) / 2;

    DrawTextEx(font, "? help", (Vector2){ 8, y }, BAR_FS, 1, HINT_COLOR);

    char info[80];
    if (count > 0)
        snprintf(info, sizeof info, "%d  %dx%d  |  %d:%d",
                 count, r->w, r->h, aw, ah);
    else
        snprintf(info, sizeof info, "%dx%d  |  %d:%d",
                 r->w, r->h, aw, ah);
    if (crop_view)
        strcat(info, "  [focus]");

    float tw = MeasureTextEx(font, info, BAR_FS, 1).x;
    DrawTextEx(font, info, (Vector2){ sw - 8 - tw, y }, BAR_FS, 1, WHITE);
}

static void draw_help_bar(Font font, int sw, int sh)
{
    int y_base = sh - BAR_BH * 2;

    DrawRectangle(0, y_base, sw, BAR_BH, BLACK);

    char left[160];
    snprintf(left, sizeof left,
             "h/j/k/l nudge  |  a/s scale  |  f focus  |"
             "  Enter save  |  Esc/q quit  |  . repeat  |  ? help");

    int y = y_base + (BAR_BH - BAR_FS) / 2;
    DrawTextEx(font, left, (Vector2){ 8, y }, BAR_FS, 1, WHITE);
}

// ── key repeat (rollover) ──────────────────────────────────────────

#define REPEAT_DELAY 0.15f
#define REPEAT_RATE  0.015f

typedef struct { float t; } KeyRep;

static bool key_repeat(KeyRep *kr, int key)
{
    if (IsKeyPressed(key)) { kr->t = 0; return true; }
    if (!IsKeyDown(key))   { kr->t = 0; return false; }
    kr->t += GetFrameTime();
    if (kr->t < REPEAT_DELAY) return false;
    float elapsed = kr->t - REPEAT_DELAY;
    int n = (int)(elapsed / REPEAT_RATE);
    if (n > 0) { kr->t -= n * REPEAT_RATE; return true; }
    return false;
}

typedef enum { KR_A, KR_S, KR_H, KR_J, KR_K, KR_L, KR_DOT, KR_COUNT } KRIdx;

// ── font loading ────────────────────────────────────────────────────

static Font load_mono_font(int font_size)
{
    if (!FcInit()) return GetFontDefault();

    FcPattern *pat = FcNameParse((const FcChar8 *)"Iosevka");
    if (!pat) { FcFini(); return GetFontDefault(); }

    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    FcPattern *match = FcFontMatch(NULL, pat, &result);
    FcPatternDestroy(pat);

    if (!match) { FcFini(); return GetFontDefault(); }

    FcChar8 *file;
    Font font = GetFontDefault();
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
        font = LoadFontEx((const char *)file, font_size, 0, 0);
        if (font.texture.id == 0)
            font = GetFontDefault();
    }

    FcPatternDestroy(match);
    FcFini();
    return font;
}

// ── helpers ─────────────────────────────────────────────────────────

static inline int step_val(int count)
{
    return count > 0 ? count
         : (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 10 : 1);
}

// ── input dispatch ──────────────────────────────────────────────────

static void dispatch_action(Rect *r, int w, int h, int aw, int ah,
                            int key, int val)
{
    switch (key) {
    case KEY_A: scale_from_centre(r, w, h, aw, ah,  1, val); break;
    case KEY_S: scale_from_centre(r, w, h, aw, ah, -1, val); break;
    case KEY_H: nudge(r, w, h, -val, 0); break;
    case KEY_J: nudge(r, w, h, 0, val); break;
    case KEY_K: nudge(r, w, h, 0, -val); break;
    case KEY_L: nudge(r, w, h, val, 0); break;
    }
}

static void handle_input(Rect *crop, int img_w, int img_h,
                         int aw, int ah,
                         int *count, bool *show_help, bool *crop_view,
                         bool *save, bool *quit,
                         KeyRep *kr, LastAction *last)
{
    // ---- count prefix ----
    for (int k = KEY_ZERO; k <= KEY_NINE; k++) {
        if (IsKeyPressed(k)) {
            *count = *count * 10 + (k - KEY_ZERO);
            return;
        }
    }

    // ---- actions ----
    if (IsKeyPressed(KEY_ENTER)) {
        *count = 0; *save = true; *quit = true;
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        *count = 0; *quit = true;
        return;
    }
    if (IsKeyPressed(KEY_SLASH)
        && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
    {
        *count = 0; *show_help = !*show_help;
        return;
    }
    if (IsKeyPressed(KEY_F)) {
        *count = 0; *crop_view = !*crop_view;
        return;
    }

    // ---- repeatable actions ----
    if (key_repeat(&kr[KR_A], KEY_A)) {
        int s = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_A, s);
        *last = (LastAction){KEY_A, s};
        return;
    }
    if (key_repeat(&kr[KR_S], KEY_S)) {
        int s = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_S, s);
        *last = (LastAction){KEY_S, s};
        return;
    }
    if (key_repeat(&kr[KR_H], KEY_H)) {
        int n = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_H, n);
        *last = (LastAction){KEY_H, n};
        return;
    }
    if (key_repeat(&kr[KR_J], KEY_J)) {
        int n = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_J, n);
        *last = (LastAction){KEY_J, n};
        return;
    }
    if (key_repeat(&kr[KR_K], KEY_K)) {
        int n = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_K, n);
        *last = (LastAction){KEY_K, n};
        return;
    }
    if (key_repeat(&kr[KR_L], KEY_L)) {
        int n = step_val(*count); *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, KEY_L, n);
        *last = (LastAction){KEY_L, n};
        return;
    }

    // ---- repeat last action ----
    if (key_repeat(&kr[KR_DOT], KEY_PERIOD) && last->key) {
        *count = 0;
        dispatch_action(crop, img_w, img_h, aw, ah, last->key, last->val);
        return;
    }
}

// ── main ────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    // ---- arg parse ----
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <W:H> <image> --output <path> [-v]\n",
            argv[0]);
        return 1;
    }

    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
    }

    int aspect_w, aspect_h;
    if (sscanf(argv[1], "%d:%d", &aspect_w, &aspect_h) != 2
        || aspect_w <= 0 || aspect_h <= 0)
    {
        fprintf(stderr, "error: invalid aspect ratio '%s' (expected W:H)\n",
                argv[1]);
        return 1;
    }

    char const *img_path = argv[2];
    char const *out_path = NULL;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            out_path = argv[++i];
    }

    if (!out_path) {
        fprintf(stderr, "error: --output is required\n");
        return 1;
    }

    // ---- trace log level ----
    if (!verbose)
        SetTraceLogLevel(LOG_ERROR);

    // ---- load image ----
    Image img = LoadImage(img_path);
    if (img.data == NULL) {
        fprintf(stderr, "error: could not load '%s'\n", img_path);
        return 1;
    }

    // ---- init crop state ----
    Rect crop = init_crop_rect(img.width, img.height, aspect_w, aspect_h);

    // ---- init window ----
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitWindow(img.width, img.height, "crop-paper");
    SetExitKey(0);

    Texture2D tex = LoadTextureFromImage(img);

    // ---- load monospace font (resolve via fontconfig) ----
    Font font = load_mono_font(BAR_FS);

    SetTargetFPS(60);

    int  count    = 0;
    bool  show_help = false;
    bool  crop_view = false;
    bool save = false;
    bool quit = false;

    KeyRep kr[KR_COUNT] = {0};
    LastAction last = {0};

    // ---- main loop ----
    while (!quit && !WindowShouldClose()) {
        handle_input(&crop, img.width, img.height,
                     aspect_w, aspect_h,
                     &count, &show_help, &crop_view,
                     &save, &quit,
                     kr, &last);

        // --- recalc layout (window may have been resized) ---

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float scale = fminf((float)sw / img.width, (float)sh / img.height);
        if (scale > 1.0f) scale = 1.0f;
        int ox = (int)((sw - img.width  * scale) / 2);
        int oy = (int)((sh - img.height * scale) / 2);

        float focus_scale = fminf((float)sw / crop.w, (float)sh / crop.h);
        if (focus_scale > 1.0f) focus_scale = 1.0f;
        int fx = (int)((sw - crop.w * focus_scale) / 2);
        int fy = (int)((sh - crop.h * focus_scale) / 2);

        // --- draw ---
        BeginDrawing();
        ClearBackground(BLACK);

        if (crop_view) {
            DrawTexturePro(tex,
                (Rectangle){ (float)crop.x, (float)crop.y,
                             (float)crop.w, (float)crop.h },
                (Rectangle){ (float)fx, (float)fy,
                             crop.w * focus_scale, crop.h * focus_scale },
                (Vector2){ 0, 0 }, 0, WHITE);
            DrawRectangleLinesEx(
                (Rectangle){ (float)fx, (float)fy,
                             crop.w * focus_scale, crop.h * focus_scale },
                1, CROP_OUTLINE);
        } else {
            DrawTexturePro(tex,
                (Rectangle){ 0, 0, (float)img.width, (float)img.height },
                (Rectangle){ (float)ox, (float)oy,
                             img.width * scale, img.height * scale },
                (Vector2){ 0, 0 }, 0, WHITE);

            draw_overlay(sw, sh, &crop, scale, ox, oy);
        }

        draw_info_bar(font, sw, sh, &crop,
                      aspect_w, aspect_h, count, crop_view);
        if (show_help)
            draw_help_bar(font, sw, sh);

        EndDrawing();
    }

    UnloadTexture(tex);

    // ---- crop & save ----
    if (save) {
        Rectangle rect = { (float)crop.x, (float)crop.y,
                           (float)crop.w, (float)crop.h };
        ImageCrop(&img, rect);
        ExportImage(img, out_path);
        if (verbose)
            printf("saved: %s (%d x %d)\n", out_path, crop.w, crop.h);
    }

    UnloadImage(img);
    if (font.texture.id != 0)
        UnloadFont(font);
    CloseWindow();
    return save ? 0 : 1;
}
