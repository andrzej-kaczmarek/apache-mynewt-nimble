/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <errno.h>
#include <syscfg/syscfg.h>
#include <os/os.h>
#include <hal/hal_gpio.h>
#include <bsp/bsp.h>
#include <config/config.h>
#include <lc3.h>

#include "app_priv.h"

static int parseerrno;

struct app_cfg g_app_cfg;
struct app_data g_app_data;

static int
parse_int_allowed(const char *val, int *allowed, int dflt)
{
    char *endptr;
    int intval;

    intval = strtol(val, &endptr, 0);
    if (*endptr) {
        parseerrno = EINVAL;
        return dflt;
    }

    for (; allowed && *allowed >= 0; allowed++) {
        if (intval == *allowed) {
            parseerrno = 0;
            return intval;
        }
    }

    parseerrno = ENOTSUP;

    return dflt;
}

static int
parse_int_minmax(const char *val, int min, int max, int dflt)
{
    char *endptr;
    int intval;

    intval = strtol(val, &endptr, 0);
    if (*endptr) {
        parseerrno = EINVAL;
        return dflt;
    }

    if (intval < min || intval > max) {
        parseerrno = ERANGE;
        return dflt;
    }

    parseerrno = 0;

    return intval;
}

static void
parse_str_long(int argc, char **argv, char *out, size_t out_size)
{
    size_t len;
    int i;

    memset(out, 0, out_size);

    for (i = 0; i < argc; i++) {
        len = out_size - strlen(out) - 1;
        if (len <= 0) {
            break;
        }
        strncat(out, argv[i], len);
        out[strlen(out)] = ' ';
    }

    out[strlen(out) - 1] = '\0';
    out[out_size - 1] = '\0';
}

static void
strxcpy(char *dst, const char *src, size_t size)
{
    strncpy(dst, src, size);
    dst[size - 1] = '\0';
}

static int
on_ch_set_ext(int argc, char **argv, char *val, void *arg)
{
#if MYNEWT_VAL(STATIC_CONFIG)
    return 0;
#endif

    if (argc != 1) {
        return 0;
    }

    config_set(argv[0], 1, &val);

    return 0;
}

static int
on_ch_commit_ext(void *arg)
{
    g_app_data.lc3_afpdt = LC3_SAMPLING_FREQ * LC3_FRAME_DURATION / 1000000;
    g_app_data.lc3_frame_bytes = lc3_frame_bytes(LC3_FRAME_DURATION, LC3_BITRATE);
    g_app_data.big_num_bis = MIN(AUDIO_CHANNELS, g_app_cfg.big_num_bis);
    g_app_data.big_sdu = g_app_data.lc3_frame_bytes *
        (1 + ((AUDIO_CHANNELS == 2) && (BIG_NUM_BIS == 1)));

    return 0;
}

static struct conf_handler g_conf_handler = {
    .ch_name = "app",
    .ch_ext = 1,
    .ch_set_ext = on_ch_set_ext,
    .ch_commit_ext = on_ch_commit_ext,
};

static char *
to_str(unsigned val)
{
    static char valstr[11];

    sprintf(valstr, "%u", val);

    return valstr;
}

static char *
to_hex(unsigned val)
{
    static char valstr[11];

    sprintf(valstr, "0x%08x", val);

    return valstr;
}

int
config_set(const char *name, int argc, char **argv)
{
    parseerrno = 0;

    if (!strcmp(name, "lc3.dt")) {
        g_app_cfg.lc3_frame_duration =
            parse_int_allowed(argv[0], (int[]){10000, -1},
                              g_app_cfg.lc3_frame_duration);
    } else if (!strcmp(name, "lc3.sr")) {
        g_app_cfg.lc3_sampling_freq =
            parse_int_allowed(argv[0], (int[]){16000, 24000,
#ifdef NRF53_SERIES
                                               32000, 48000,
#endif
                                               -1},
                              g_app_cfg.lc3_sampling_freq);
    } else if (!strcmp(name, "lc3.br")) {
        g_app_cfg.lc3_bitrate =
            parse_int_minmax(argv[0], LC3_MIN_BITRATE, LC3_MAX_BITRATE,
                             (int)g_app_cfg.lc3_bitrate);
    } else if (!strcmp(name, "big.phy")) {
        g_app_cfg.big_phy = parse_int_minmax(argv[0], 1, 2, g_app_cfg.big_phy);
    } else if (!strcmp(name, "big.num_bis")) {
        g_app_cfg.big_num_bis = parse_int_minmax(argv[0], 1, 2,
                                                 g_app_cfg.big_num_bis);
    } else if (!strcmp(name, "big.bn")) {
        g_app_cfg.big_bn = parse_int_minmax(argv[0], 1, 3, g_app_cfg.big_bn);
    } else if (!strcmp(name, "big.irc")) {
        g_app_cfg.big_irc = parse_int_minmax(argv[0], 1, 5, g_app_cfg.big_irc);
    } else if (!strcmp(name, "big.pto")) {
        g_app_cfg.big_pto = parse_int_minmax(argv[0], 1, 3, g_app_cfg.big_pto);
    } else if (!strcmp(name, "big.pt")) {
        g_app_cfg.big_pt = parse_int_minmax(argv[0], 0, 4, g_app_cfg.big_pt);
    } else if (!strcmp(name, "big.packing")) {
        g_app_cfg.big_packing = parse_int_minmax(argv[0], 0, 1,
                                                 g_app_cfg.big_packing);
    } else if (!strcmp(name, "big.encrypt")) {
        g_app_cfg.big_encryption = parse_int_minmax(argv[0], 0, 1,
                                                    g_app_cfg.big_encryption);
    } else if (!strcmp(name, "gap.appearance")) {
        g_app_cfg.gap_appearance = parse_int_minmax(argv[0], 0x0000, 0xffff,
                                                    g_app_cfg.gap_appearance);
    } else if (!strcmp(name, "gap.name")) {
        parse_str_long(argc, argv, g_app_cfg.gap_local_name,
                       sizeof(g_app_cfg.gap_local_name));
    } else if (!strcmp(name, "bcast.id")) {
        g_app_cfg.broadcast_id = parse_int_minmax(argv[0], 0x000000, 0xffffff,
                                                  (int)g_app_cfg.broadcast_id);
    } else if (!strcmp(name, "bcast.name")) {
        parse_str_long(argc, argv, g_app_cfg.broadcast_name,
                       sizeof(g_app_cfg.broadcast_name));
    } else if (!strcmp(name, "bcast.code")) {
        parse_str_long(argc, argv, g_app_cfg.broadcast_code,
                       sizeof(g_app_cfg.broadcast_code));
    } else {
        return -ENOENT;
    }

    return -parseerrno;
}

void
config_reset_to_defaults(void)
{
    g_app_cfg.lc3_frame_duration = MYNEWT_VAL(LC3_FRAME_DURATION);
    g_app_cfg.lc3_sampling_freq = MYNEWT_VAL(LC3_SAMPLING_FREQ);
    g_app_cfg.lc3_bitrate = MYNEWT_VAL(LC3_BITRATE);

    g_app_cfg.big_phy = MYNEWT_VAL(BIG_PHY);
    g_app_cfg.big_num_bis = MYNEWT_VAL(BIG_NUM_BIS);
    g_app_cfg.big_bn = MYNEWT_VAL(BIG_BN);
    g_app_cfg.big_irc = MYNEWT_VAL(BIG_IRC);
    g_app_cfg.big_pto = MYNEWT_VAL(BIG_PTO);
    g_app_cfg.big_pt = MYNEWT_VAL(BIG_PT);
    g_app_cfg.big_packing = MYNEWT_VAL(BIG_PACKING);
    g_app_cfg.big_encryption = MYNEWT_VAL(BIG_ENCRYPTION);

    g_app_cfg.gap_appearance = MYNEWT_VAL(GAP_APPEARANCE);
    strxcpy(g_app_cfg.gap_local_name, MYNEWT_VAL(GAP_LOCAL_NAME),
            sizeof(g_app_cfg.gap_local_name));

    g_app_cfg.broadcast_id = MYNEWT_VAL(BROADCAST_ID);
    strxcpy(g_app_cfg.broadcast_name, MYNEWT_VAL(BROADCAST_NAME),
            sizeof(g_app_cfg.broadcast_name));
    strxcpy(g_app_cfg.broadcast_code, MYNEWT_VAL(BROADCAST_CODE),
            sizeof(g_app_cfg.broadcast_code));
}

void
config_commit(void)
{
    conf_save_one("app/lc3.dt", to_str(g_app_cfg.lc3_frame_duration));
    conf_save_one("app/lc3.sr", to_str(g_app_cfg.lc3_sampling_freq));
    conf_save_one("app/lc3.br", to_str(g_app_cfg.lc3_bitrate));
    conf_save_one("app/big.phy", to_str(g_app_cfg.big_phy));
    conf_save_one("app/big.num_bis", to_str(g_app_cfg.big_num_bis));
    conf_save_one("app/big.bn", to_str(g_app_cfg.big_bn));
    conf_save_one("app/big.irc", to_str(g_app_cfg.big_irc));
    conf_save_one("app/big.pto", to_str(g_app_cfg.big_pto));
    conf_save_one("app/big.pt", to_str(g_app_cfg.big_pt));
    conf_save_one("app/big.packing", to_str(g_app_cfg.big_packing));
    conf_save_one("app/big.encrypt", to_str(g_app_cfg.big_encryption));
    conf_save_one("app/gap.appearance", to_hex(g_app_cfg.gap_appearance));
    conf_save_one("app/gap.name", g_app_cfg.gap_local_name);
    conf_save_one("app/bcast.id", to_hex(g_app_cfg.broadcast_id));
    conf_save_one("app/bcast.name", g_app_cfg.broadcast_name);
    conf_save_one("app/bcast.code", g_app_cfg.broadcast_code);
}

void
config_init(void)
{
    config_reset_to_defaults();

    hal_gpio_init_in(BUTTON_1, HAL_GPIO_PULL_UP);
    os_time_delay(OS_TICKS_PER_SEC);
    if (!hal_gpio_read(BUTTON_1)) {
        config_commit();
    }

    conf_register(&g_conf_handler);
    conf_load();
}
