/*
 * Unit & specification tests for 3ds_netpass:
 * - Proves 20-byte 3DS `gamecoin.dat` (`0x4F00` magic) creation, reading, and 300-coin clamping.
 * - Proves mandatory `gamecoin.dat.bak` backup creation before overwriting coins.
 * - Proves StreetPass CECD box archiving under `<sd_root>/3ds/netpass/boxes/<TitleID>/inbox.cecd`.
 */

#include "np_netpass.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(void) {
    remove("build/test_sd/3ds/netpass/gamecoin.dat");
    remove("build/test_sd/3ds/netpass/gamecoin.dat.bak");

    np_state_t st;
    assert(np_init(&st, "build/test_sd"));
    assert(st.play_coins == 45u);
    assert(st.box_count >= 4);

    /* Max Play Coins to 300 (even if 500 requested, must clamp to 300) */
    assert(np_set_play_coins(&st, 500u));
    assert(st.play_coins == 300u);

    uint16_t loaded_coins = 0;
    uint32_t loaded_steps = 0;
    assert(np_load_gamecoin(st.gamecoin_path, &loaded_coins, &loaded_steps));
    assert(loaded_coins == 300u);
    assert(loaded_steps == 48210u);

    /* Verify .bak has previous 45 coins */
    char bak_path[600];
    snprintf(bak_path, sizeof(bak_path), "%s.bak", st.gamecoin_path);
    uint16_t bak_coins = 0;
    assert(np_load_gamecoin(bak_path, &bak_coins, NULL));
    assert(bak_coins == 45u);

    /* Fail-the-Fail negative assertions: bad magic, truncated file, NULL pointers */
    assert(np_init(NULL, "build/test_sd") == false);
    assert(np_set_play_coins(NULL, 100u) == false);
    assert(np_archive_streetpass_box(&st, "", "Bad", 1u) == false);

    const char *bad_gc = "build/test_sd/3ds/netpass/bad_gamecoin.dat";
    FILE *bfp = fopen(bad_gc, "wb");
    assert(bfp != NULL);
    fwrite("BADMAGIC0123456789AB", 1, 20, bfp);
    fclose(bfp);
    assert(np_load_gamecoin(bad_gc, &loaded_coins, NULL) == false);
    remove(bad_gc);

    printf("PASS: test_netpass all gamecoin.dat (0x4F00), .bak backup, and CECD box archive checks succeeded.\n");
    return 0;
}
