#ifndef NP_NETPASS_H
#define NP_NETPASS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NP_MAX_BOXES 16
#define NP_PATH_MAX 512
#define NP_GAMECOIN_MAGIC 0x4F00u
#define NP_GAMECOIN_SIZE  20u

typedef struct {
    char title_id[16];
    char title_name[48];
    uint32_t message_count;
    size_t archive_bytes;
} np_cecd_box_t;

typedef struct {
    char sd_root[NP_PATH_MAX];
    char gamecoin_path[NP_PATH_MAX + 64];
    uint16_t play_coins;          /* 0..300 */
    uint32_t total_purchased;
    uint32_t step_count_today;
    uint32_t total_pedometer_steps;
    int box_count;
    np_cecd_box_t boxes[NP_MAX_BOXES];
} np_state_t;

/* Initialize NetPass & Play Coin Vault state (creates default 20-byte 0x4F00 gamecoin.dat if absent) */
bool np_init(np_state_t *st, const char *sd_root);

/* Read 20-byte 3DS `gamecoin.dat` structure (`+0x00` magic `0x4F00`, `+0x04` coins `u16`) */
bool np_load_gamecoin(const char *path, uint16_t *out_coins, uint32_t *out_total_steps);

/* Safely set Play Coins (`0..300`), writing `<path>.bak` first and committing via `<path>.tmp` -> `rename()` */
bool np_set_play_coins(np_state_t *st, uint16_t coins);

/* Export a StreetPass CECD box archive to `<sd_root>/3ds/netpass/boxes/<title_id>/inbox.cecd` */
bool np_archive_streetpass_box(np_state_t *st, const char *title_id, const char *title_name, uint32_t msg_count);

#endif /* NP_NETPASS_H */
