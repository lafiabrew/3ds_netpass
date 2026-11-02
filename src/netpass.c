#include "np_netpass.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __3DS__
#include <3ds.h>
#endif

static void ensure_dir_p(const char *dir_path) {
    char tmp[NP_PATH_MAX + 96];
    snprintf(tmp, sizeof(tmp), "%s", dir_path);
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

bool np_load_gamecoin(const char *path, uint16_t *out_coins, uint32_t *out_total_steps) {
    if (path == NULL) return false;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return false;
    uint8_t raw[NP_GAMECOIN_SIZE];
    size_t n = fread(raw, 1, sizeof(raw), fp);
    fclose(fp);
    if (n != NP_GAMECOIN_SIZE || read_u16_le(raw + 0x00) != NP_GAMECOIN_MAGIC) {
        return false;
    }
    uint16_t c = read_u16_le(raw + 0x04);
    if (c > 300u) c = 300u;
    if (out_coins != NULL) *out_coins = c;
    if (out_total_steps != NULL) *out_total_steps = read_u32_le(raw + 0x0C);
    return true;
}

bool np_set_play_coins(np_state_t *st, uint16_t coins) {
    if (st == NULL || st->gamecoin_path[0] == '\0') return false;
    if (coins > 300u) coins = 300u;

    uint8_t raw[NP_GAMECOIN_SIZE];
    memset(raw, 0, sizeof(raw));

    FILE *orig = fopen(st->gamecoin_path, "rb");
    if (orig != NULL) {
        size_t r = fread(raw, 1, sizeof(raw), orig);
        fclose(orig);
        if (r == sizeof(raw)) {
            char bak_path[NP_PATH_MAX + 80];
            snprintf(bak_path, sizeof(bak_path), "%s.bak", st->gamecoin_path);
            FILE *bfp = fopen(bak_path, "wb");
            if (bfp != NULL) {
                fwrite(raw, 1, sizeof(raw), bfp);
                fflush(bfp);
                fclose(bfp);
            }
        }
    }

    write_u16_le(raw + 0x00, NP_GAMECOIN_MAGIC);
    write_u16_le(raw + 0x04, coins);
    write_u32_le(raw + 0x08, st->total_purchased + (uint32_t)coins);
    write_u32_le(raw + 0x0C, st->total_pedometer_steps);
    /* Date fields: 2026-11-02 */
    write_u16_le(raw + 0x10, 2026u);
    raw[0x12] = 11u;
    raw[0x13] = 2u;

    char tmp_path[NP_PATH_MAX + 80];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", st->gamecoin_path);
    FILE *tfp = fopen(tmp_path, "wb");
    if (tfp == NULL) return false;
    size_t w = fwrite(raw, 1, sizeof(raw), tfp);
    fflush(tfp);
    fclose(tfp);
    if (w != sizeof(raw) || rename(tmp_path, st->gamecoin_path) != 0) {
        remove(tmp_path);
        return false;
    }

    st->play_coins = coins;
    return true;
}

bool np_archive_streetpass_box(np_state_t *st, const char *title_id, const char *title_name, uint32_t msg_count) {
    if (st == NULL || title_id == NULL || title_id[0] == '\0') return false;
    char box_dir[NP_PATH_MAX + 64];
    snprintf(box_dir, sizeof(box_dir), "%s/3ds/netpass/boxes/%s", st->sd_root, title_id);
    ensure_dir_p(box_dir);

    char final_path[NP_PATH_MAX + 96];
    char tmp_path[NP_PATH_MAX + 112];
    snprintf(final_path, sizeof(final_path), "%s/inbox.cecd", box_dir);
    snprintf(tmp_path, sizeof(tmp_path), "%s/inbox.cecd.tmp", box_dir);

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL) return false;
    fprintf(fp, "CECD_BOX_V1|TID=%s|NAME=%s|MSGS=%u\n",
            title_id, title_name != NULL ? title_name : "3DS_TITLE", (unsigned)msg_count);
    fflush(fp);
    long sz = ftell(fp);
    fclose(fp);
    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        return false;
    }

    int idx = -1;
    for (int i = 0; i < st->box_count; i++) {
        if (strcmp(st->boxes[i].title_id, title_id) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && st->box_count < NP_MAX_BOXES) {
        idx = st->box_count++;
    }
    if (idx >= 0) {
        np_cecd_box_t box;
        memset(&box, 0, sizeof(box));
        snprintf(box.title_id, sizeof(box.title_id), "%.15s", title_id);
        snprintf(box.title_name, sizeof(box.title_name), "%.47s",
                 title_name != NULL ? title_name : "3DS STREETPASS BOX");
        box.message_count = msg_count;
        box.archive_bytes = sz > 0 ? (size_t)sz : 64u;
        st->boxes[idx] = box;
    }
    return true;
}

bool np_init(np_state_t *st, const char *sd_root) {
    if (st == NULL) return false;
    memset(st, 0, sizeof(*st));
    snprintf(st->sd_root, sizeof(st->sd_root), "%s", sd_root != NULL ? sd_root : "./sdmc");

    char base_dir[NP_PATH_MAX + 32];
    snprintf(base_dir, sizeof(base_dir), "%s/3ds/netpass", st->sd_root);
    ensure_dir_p(base_dir);
    snprintf(st->gamecoin_path, sizeof(st->gamecoin_path), "%s/gamecoin.dat", base_dir);

    st->step_count_today = 2450u;
    st->total_pedometer_steps = 48210u;

#ifdef __3DS__
    if (R_SUCCEEDED(ptmuInit())) {
        u32 total_steps = 0;
        if (R_SUCCEEDED(PTMU_GetTotalStepCount(&total_steps))) {
            st->total_pedometer_steps = total_steps;
        }
        ptmuExit();
    }
#endif

    if (!np_load_gamecoin(st->gamecoin_path, &st->play_coins, &st->total_pedometer_steps)) {
        st->play_coins = 45u;
        np_set_play_coins(st, st->play_coins);
    }

    /* Seed default StreetPass boxes if none archived yet */
    np_archive_streetpass_box(st, "00020800", "STREETPASS MII PLAZA", 8u);
    np_archive_streetpass_box(st, "00055D00", "POKEMON X / YHOLO CASTER", 3u);
    np_archive_streetpass_box(st, "0011C400", "POKEMON OR/ASBUZZ NAV (EON TICKET)", 5u);
    np_archive_streetpass_box(st, "001B5000", "POKEMON US/UM FESTIVAL PLAZA", 4u);
    return true;
}
