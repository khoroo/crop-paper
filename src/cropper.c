// SPDX-License-Identifier: MIT
// cropper — crop images to a specified aspect ratio

#define _POSIX_C_SOURCE 200809L

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN_CROP 16

// ── Gruvbox dark palette ────────────────────────────────────────────
static const Color GB_BG0    = { 0x1d, 0x20, 0x21, 255 };
static const Color GB_FG0    = { 0xeb, 0xdb, 0xb2, 255 };
static const Color GB_FG1    = { 0xd5, 0xc4, 0xa1, 255 };
static const Color GB_RED    = { 0xcc, 0x24, 0x1d, 255 };
static const Color GB_YELLOW = { 0xd7, 0x99, 0x21, 255 };


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
        2, GB_RED);
}

static int bar_height(float ts)
{
    int fs = (int)(24 * ts);
    return fs + 16;
}

static void draw_info_bar(Font font, int sw, int sh, Rect const *r,
                          int aw, int ah, float ts, int y_off)
{
    int fs = (int)(24 * ts);
    int bh = bar_height(ts);
    int y_base = sh - bh - y_off;

    char info[80];
    snprintf(info, sizeof info,
             "%dx%d  |  %d:%d", r->w, r->h, aw, ah);

    DrawRectangle(0, y_base, sw, bh, GB_BG0);
    int y = y_base + (bh - fs) / 2;
    float tw = MeasureTextEx(font, info, fs, 1).x;
    DrawTextEx(font, info, (Vector2){ sw - 8 - tw, y }, fs, 1, GB_FG0);
}

static void draw_help_bar(Font font, int sw, int sh, float ts)
{
    int fs = (int)(24 * ts);
    int bh = bar_height(ts);
    int y_base = sh - bh;

    DrawRectangle(0, y_base, sw, bh, GB_BG0);

    char left[160];
    snprintf(left, sizeof left,
             "h/j/k/l nudge  |  a/s scale  |  Enter save  |"
             "  Esc/q quit  |  [] txt  |  ? help");

    int y = y_base + (bh - fs) / 2;
    DrawTextEx(font, left, (Vector2){ 8, y }, fs, 1, GB_FG1);
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

// ── main ────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    // ---- arg parse ----
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <W:H> <image> --output <path>"
            " [--nowarn]"
            " [--warn-width W] [--warn-height H]\n",
            argv[0]);
        return 1;
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
    char const *out_path = nullptr;
    int warn_w = 0, warn_h = 0;
    bool warn_custom = false;
    bool no_warn = false;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            out_path = argv[++i];
        else if (strcmp(argv[i], "--nowarn") == 0)
            no_warn = true;
        else if (strcmp(argv[i], "--warn-width") == 0 && i + 1 < argc) {
            warn_w = atoi(argv[++i]);
            warn_custom = true;
        } else if (strcmp(argv[i], "--warn-height") == 0 && i + 1 < argc) {
            warn_h = atoi(argv[++i]);
            warn_custom = true;
        }
    }

    if (!out_path) {
        fprintf(stderr, "error: --output is required\n");
        return 1;
    }

    // ---- load image ----
    Image img = LoadImage(img_path);
    if (img.data == nullptr) {
        fprintf(stderr, "error: could not load '%s'\n", img_path);
        return 1;
    }

    // ---- init crop state ----
    Rect crop = init_crop_rect(img.width, img.height, aspect_w, aspect_h);

    // ---- init window ----
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitWindow(img.width, img.height, "cropper");
    SetExitKey(0);

    Texture2D tex = LoadTextureFromImage(img);

    // ---- load monospace font ----
    Font font;
#ifdef FONT_PATH
    font = LoadFontEx(FONT_PATH, 48, 0, 0);
    if (font.texture.id == 0)
        font = GetFontDefault();
#else
    font = GetFontDefault();
#endif

    // ---- warning thresholds ----
    int mon = GetCurrentMonitor();
    int mon_w = GetMonitorWidth(mon);
    int mon_h = GetMonitorHeight(mon);
    if (warn_custom) {
        if (warn_w <= 0) warn_w = mon_w;
        if (warn_h <= 0) warn_h = mon_h;
    } else {
        warn_w = mon_w;
        warn_h = mon_h;
    }

    SetTargetFPS(60);

    float text_scale  = 1.0f;
    bool  show_help    = false;
    bool confirm_warn = false;
    bool save = false;
    bool quit = false;

    KeyRep kr_a = {0}, kr_s = {0};
    KeyRep kr_h = {0}, kr_j = {0}, kr_k = {0}, kr_l = {0};

    // ---- main loop ----
    while (!quit && !WindowShouldClose()) {
        // --- input ---
        if (confirm_warn) {
            if (IsKeyPressed(KEY_Y)) {
                save = true;
                quit = true;
            } else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) {
                confirm_warn = false;
            }
        } else {
            if (IsKeyPressed(KEY_LEFT_BRACKET)) {
                text_scale = fmaxf(0.5f, text_scale - 0.25f);
            } else if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
                text_scale = fminf(3.0f, text_scale + 0.25f);

            } else if (IsKeyPressed(KEY_ENTER)) {
                if (no_warn) {
                    save = true;
                    quit = true;
                } else if (crop.w < warn_w || crop.h < warn_h) {
                    confirm_warn = true;
                } else {
                    save = true;
                    quit = true;
                }
            } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
                quit = true;

            } else if (IsKeyPressed(KEY_SLASH)
                       && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) {
                show_help = !show_help;

            } else if (key_repeat(&kr_a, KEY_A)) {
                int s = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                scale_from_centre(&crop, img.width, img.height,
                                  aspect_w, aspect_h, 1, s);
            } else if (key_repeat(&kr_s, KEY_S)) {
                int s = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                scale_from_centre(&crop, img.width, img.height,
                                  aspect_w, aspect_h, -1, s);

            } else if (key_repeat(&kr_h, KEY_H)) {
                int n = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                nudge(&crop, img.width, img.height, -n, 0);
            } else if (key_repeat(&kr_j, KEY_J)) {
                int n = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                nudge(&crop, img.width, img.height, 0, n);
            } else if (key_repeat(&kr_k, KEY_K)) {
                int n = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                nudge(&crop, img.width, img.height, 0, -n);
            } else if (key_repeat(&kr_l, KEY_L)) {
                int n = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 10 : 1;
                nudge(&crop, img.width, img.height, n, 0);
            }
        }

        // --- recalc layout (window may have been resized) ---
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float scale = fminf((float)sw / img.width, (float)sh / img.height);
        if (scale > 1.0f) scale = 1.0f;
        int ox = (int)((sw - img.width  * scale) / 2);
        int oy = (int)((sh - img.height * scale) / 2);

        // --- draw ---
        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(tex,
            (Rectangle){ 0, 0, (float)img.width, (float)img.height },
            (Rectangle){ (float)ox, (float)oy,
                         img.width * scale, img.height * scale },
            (Vector2){ 0, 0 }, 0, WHITE);

        draw_overlay(sw, sh, &crop, scale, ox, oy);

        int help_h = show_help ? bar_height(text_scale) : 0;
        draw_info_bar(font, sw, sh, &crop,
                      aspect_w, aspect_h, text_scale, help_h);
        if (show_help) {
            draw_help_bar(font, sw, sh, text_scale);
        } else {
            int fs = (int)(24 * text_scale);
            int bh = bar_height(text_scale);
            DrawTextEx(font, "? help",
                       (Vector2){ 8, sh - bh + (bh - fs) / 2 },
                       fs, 1, GB_FG1);
        }

        if (confirm_warn) {
            DrawRectangle(0, sh / 2 - 40, sw, 80,
                          (Color){ 0, 0, 0, 200 });
            char const *msg =
                "Cropped image is smaller than desktop dimensions."
                " Proceed? (y/n)";
            int fs = (int)(40 * text_scale);
            float tw = MeasureTextEx(font, msg, fs, 1).x;
            DrawTextEx(font, msg,
                       (Vector2){ sw / 2 - tw / 2, sh / 2 - fs / 2 },
                       fs, 1, GB_YELLOW);
        }

        EndDrawing();
    }

    UnloadTexture(tex);

    // ---- crop & save ----
    if (save) {
        Rectangle rect = { (float)crop.x, (float)crop.y,
                           (float)crop.w, (float)crop.h };
        ImageCrop(&img, rect);
        ExportImage(img, out_path);
        printf("saved: %s (%d x %d)\n", out_path, crop.w, crop.h);
    }

    UnloadImage(img);
    if (font.texture.id != 0)
        UnloadFont(font);
    CloseWindow();
    return save ? 0 : 1;
}
