#include "np_netpass.h"
#include "pk_platform.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    np_state_t state;
    int selected_box;
    int scroll_box;
    char status_msg[128];
} np_app_t;

static void sync_np_scroll(np_app_t *app) {
    int vis = 4;
    if (app->state.box_count <= vis) {
        app->scroll_box = 0;
        return;
    }
    if (app->selected_box < app->scroll_box) app->scroll_box = app->selected_box;
    else if (app->selected_box >= app->scroll_box + vis) app->scroll_box = app->selected_box - vis + 1;
}

static void np_app_init(np_app_t *app, const char *sd_root) {
    memset(app, 0, sizeof(*app));
    np_init(&app->state, sd_root != NULL ? sd_root : pk_platform_default_sd_root());
    snprintf(app->status_msg, sizeof(app->status_msg),
             "PLAY COINS: %u/300 | %d STREETPASS BOXES READY",
             (unsigned)app->state.play_coins, app->state.box_count);
    pk_log_event(app->state.sd_root, "netpass", "INFO", app->status_msg);
}

static void np_app_step(np_app_t *app, const pk_input_t *in) {
    if ((in->down & PK_BTN_DUP) && app->state.box_count > 0) {
        app->selected_box = (app->selected_box > 0) ? (app->selected_box - 1) : (app->state.box_count - 1);
    }
    if ((in->down & PK_BTN_DDOWN) && app->state.box_count > 0) {
        app->selected_box = (app->selected_box + 1 < app->state.box_count) ? (app->selected_box + 1) : 0;
    }
    sync_np_scroll(app);
    bool act_max = (in->down & PK_BTN_A) != 0;
    bool act_add = (in->down & PK_BTN_X) != 0;
    bool act_arc = (in->down & PK_BTN_Y) != 0;
    if (in->touch_just_pressed && in->touch_x >= 16 && in->touch_x <= 304) {
        if (in->touch_y >= 26 && in->touch_y <= 60) act_max = true;
        else if (in->touch_y >= 68 && in->touch_y <= 102) act_add = true;
        else if (in->touch_y >= 110 && in->touch_y <= 144) act_arc = true;
    }
    if (act_max) {
        if (np_set_play_coins(&app->state, 300u)) {
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "MAXED PLAY COINS -> 300/300 (GAMECOIN.DAT.BAK SAVED)");
            pk_log_event(app->state.sd_root, "netpass", "INFO", app->status_msg);
        }
    }
    if (act_add) {
        uint16_t next = (uint16_t)((app->state.play_coins + 50u) <= 300u ? (app->state.play_coins + 50u) : 300u);
        if (np_set_play_coins(&app->state, next)) {
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "ADDED +50 PLAY COINS -> %u/300", (unsigned)app->state.play_coins);
            pk_log_event(app->state.sd_root, "netpass", "INFO", app->status_msg);
        }
    }
    if (act_arc && app->state.box_count > 0) {
        np_cecd_box_t *b = &app->state.boxes[app->selected_box];
        if (np_archive_streetpass_box(&app->state, b->title_id, b->title_name, b->message_count + 1u)) {
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "ARCHIVED CECD BOX %s (%u TAGS)",
                     b->title_id, (unsigned)app->state.boxes[app->selected_box].message_count);
            pk_log_event(app->state.sd_root, "netpass", "INFO", app->status_msg);
        }
    }
}

static void np_app_render(const np_app_t *app) {
    pk_render_begin_frame();
    pk_render_begin_target(PK_TARGET_TOP_LEFT, PK_RGBA8(12, 26, 18, 255));
    pk_draw_rect(0, 0, 400, 26, PK_RGBA8(22, 62, 38, 255));
    pk_draw_text(8, 7, 0.55f, PK_RGBA8(115, 255, 145, 255),
                 "3DS NETPASS - STREETPASS & PLAY COIN VAULT");

    char line[160];
    snprintf(line, sizeof(line), "PLAY COINS: %3u / 300   |   PEDOMETER: %u STEPS",
             (unsigned)app->state.play_coins, (unsigned)app->state.total_pedometer_steps);
    pk_draw_text(10, 38, 0.52f, PK_RGBA8(255, 230, 95, 255), line);

    /* Coin progress bar */
    pk_draw_rect(10, 58, 380, 18, PK_RGBA8(24, 42, 32, 255));
    float fill_w = ((float)app->state.play_coins / 300.0f) * 376.0f;
    pk_draw_rect(12, 60, fill_w, 14, PK_RGBA8(255, 215, 75, 255));

    pk_draw_rect(8, 86, 384, 118, PK_RGBA8(18, 36, 26, 255));
    pk_draw_rect_outline(8, 86, 384, 118, 1.0f, PK_RGBA8(75, 195, 115, 255));
    pk_draw_text(16, 94, 0.5f, PK_RGBA8(170, 255, 195, 255),
                 "STREETPASS CECD VAULT ARCHIVES:");

    for (int row = 0; row < 4; row++) {
        int idx = app->scroll_box + row;
        if (idx >= app->state.box_count) break;
        const np_cecd_box_t *b = &app->state.boxes[idx];
        float y = 114.0f + (float)row * 20.0f;
        if (idx == app->selected_box) {
            pk_draw_rect(14.0f, y - 2.0f, 364.0f, 18.0f, PK_RGBA8(36, 86, 54, 255));
        }
        snprintf(line, sizeof(line), "[%s] %-28.28s (%u TAGS)",
                 b->title_id, b->title_name, (unsigned)b->message_count);
        pk_draw_text(18.0f, y + 2.0f, 0.45f, PK_RGBA8(235, 250, 240, 255), line);
    }
    pk_draw_scrollbar(382.0f, 112.0f, 80.0f, app->state.box_count, 4, app->scroll_box,
                      PK_RGBA8(22, 46, 32, 255), PK_RGBA8(115, 255, 145, 255));

    pk_draw_rect(0, 216, 400, 24, PK_RGBA8(18, 44, 28, 255));
    pk_draw_text(8, 222, 0.48f, PK_RGBA8(120, 255, 165, 255), app->status_msg);

    pk_render_begin_target(PK_TARGET_BOTTOM, PK_RGBA8(10, 20, 14, 255));
    pk_draw_rect(12, 18, 296, 204, PK_RGBA8(20, 38, 28, 255));
    pk_draw_text(22, 34, 0.52f, PK_RGBA8(255, 230, 100, 255),
                 "A: MAX PLAY COINS (300/300)");
    pk_draw_text(22, 62, 0.52f, PK_RGBA8(130, 250, 175, 255),
                 "X: ADD +50 PLAY COINS");
    pk_draw_text(22, 90, 0.52f, PK_RGBA8(140, 225, 255, 255),
                 "Y: ARCHIVE / SYNC CECD BOX");
    pk_draw_text(22, 134, 0.46f, PK_RGBA8(200, 225, 210, 255),
                 "ALL EDITS CREATE GAMECOIN.DAT.BAK");
    pk_draw_text(22, 154, 0.46f, PK_RGBA8(200, 225, 210, 255),
                 "AND COMMIT ATOMICALLY VIA RENAME()");
    pk_draw_text(22, 190, 0.48f, PK_RGBA8(170, 195, 180, 255),
                 "PRESS START TO EXIT");
    pk_render_end_frame();
}

int main(int argc, char **argv) {
    const char *sd_root = NULL;
    bool one_shot = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sd-root") == 0 && i + 1 < argc) {
            sd_root = argv[++i];
        } else if (strcmp(argv[i], "--once") == 0) {
            one_shot = true;
        }
    }

    if (!pk_platform_init()) return 1;
    np_app_t app;
    np_app_init(&app, sd_root);

    while (pk_platform_main_loop()) {
        pk_input_t in;
        pk_platform_poll_input(&in);
        if (in.down & PK_BTN_START) break;
        np_app_step(&app, &in);
        np_app_render(&app);
        if (one_shot) break;
    }

    pk_platform_shutdown();
    return 0;
}
