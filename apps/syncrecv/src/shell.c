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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <nimble/hci_common.h>
#include <host/ble_gap.h>
#include <host/util/util.h>
#include <shell/shell.h>
#include <imgmgr/imgmgr.h>
#include "app_priv.h"
#include "img_mgmt/image.h"

struct ble_gap_event_listener gap_listener;

extern int ble_hs_hci_cmd_tx(uint16_t opcode,
                             const void *cmd, uint8_t cmd_len,
                             void *rsp, uint8_t rsp_len);

//#if !MYNEWT_VAL(STATIC_CONFIG)
//struct csc_preset {
//    const char *name;
//    uint32_t sampling_freq;
//    uint32_t frame_duration;
//    uint32_t  bitrate;
//};
//
//static const struct csc_preset presets[] = {
//    { "16_2", 16000, 10000,  32000 },
//    { "24_2", 24000, 10000,  48000 },
//    { "32_2", 32000, 10000,  64000 },
//    { "48_2", 48000, 10000,  80000 },
//    { "48_4", 48000, 10000,  96000 },
//    { "48_6", 48000, 10000, 124000 },
//};
//#endif
//
//static int
//cmd_start(int argc, char **argv)
//{
//    return 0;
//}
//
//static int
//cmd_info(int argc, char **argv)
//{
//    struct image_version ver;
//    int rc;
//
//    printf("NimBLE ISO Broadcaster\n");
//
//    rc = imgr_my_version(&ver);
//    if (rc) {
//        printf("| Version: <unknown>\n");
//    } else if (ver.iv_build_num){
//        printf("| Version: 20%02d.%04d.%ld\n", ver.iv_minor, ver.iv_revision,
//               ver.iv_build_num);
//    } else {
//        printf("| Version: 20%02d.%04d\n", ver.iv_minor, ver.iv_revision);
//    }
//
//    printf("| Broadcast ID: 0x%06lx\n", g_app_data.broadcast_id);
//    printf("| Broadcast Name: %s\n", g_app_cfg.broadcast_name);
//    if (g_app_cfg.big_encryption) {
//        printf("| Broadcast Code: %s\n", g_app_cfg.broadcast_code);
//    }
//    printf("| LC3: %dkHz @ %ldkbps (%dms)\n", g_app_cfg.lc3_sampling_freq / 1000,
//           g_app_cfg.lc3_bitrate / 1000, g_app_cfg.lc3_frame_duration / 1000);
//    printf("| BIG: %dM BIS=%d (%s) NSE=%d BN=%d IRC=%d PTO=%d ISO_Interval=%dms SDU=%d\n",
//           g_app_cfg.big_phy, BIG_NUM_BIS,
//           AUDIO_CHANNELS == 1 ? "mono" : BIG_NUM_BIS == 1 ? "FL+FR" : g_app_cfg.big_packing ? "interleaved" : "sequential",
//           g_app_cfg.big_bn * (g_app_cfg.big_irc + g_app_cfg.big_pt),
//           g_app_cfg.big_bn, g_app_cfg.big_irc, g_app_cfg.big_pto,
//           g_app_cfg.lc3_frame_duration * g_app_cfg.big_bn / 1000,
//           g_app_data.big_sdu);
//
//    return 0;
//}
//
//static int
//cmd_get(int argc, char **argv)
//{
//#if MYNEWT_VAL(STATIC_CONFIG)
//    printf("* static configuration\n");
//#else
//    if (g_app_data.modified) {
//        printf("* configuration is modified, commit and reboot to apply\n");
//    }
//#endif
//
//    printf("configurable settings:\n");
//    printf("| lc3.dt = %d [us]\n", g_app_cfg.lc3_frame_duration);
//    printf("| lc3.sr = %d [Hz]\n", g_app_cfg.lc3_sampling_freq);
//    printf("| lc3.br = %ld [bps]\n", g_app_cfg.lc3_bitrate);
//    printf("| big.phy = %d\n", g_app_cfg.big_phy);
//    printf("| big.num_bis = %d\n", g_app_cfg.big_num_bis);
//    printf("| big.bn = %d\n", g_app_cfg.big_bn);
//    printf("| big.irc = %d\n", g_app_cfg.big_irc);
//    printf("| big.pto = %d\n", g_app_cfg.big_pto);
//    printf("| big.pt = %d\n", g_app_cfg.big_pt);
//    printf("| big.packing = %d\n", g_app_cfg.big_packing);
//    printf("| big.encrypt = %d\n", g_app_cfg.big_encryption);
//    printf("| gap.appearance = 0x%04x\n", g_app_cfg.gap_appearance);
//    printf("| gap.name = %s\n", g_app_cfg.gap_local_name);
//    printf("| bcast.id = 0x%06lx\n", g_app_cfg.broadcast_id);
//    printf("| bcast.name = %s\n", g_app_cfg.broadcast_name);
//    printf("| bcast.code = %s\n", g_app_cfg.broadcast_code);
//    printf("derived settings:\n");
//    printf("| lc3.flen = %ld [octets]\n", g_app_data.lc3_frame_bytes);
//    printf("| big.num_bis = %d\n", BIG_NUM_BIS);
//    printf("| big.sdu = %d\n", g_app_data.big_sdu);
//    printf("| bcast.id = 0x%06lx\n", g_app_data.broadcast_id);
//
//    return 0;
//}
//
//static int
//cmd_set(int argc, char **argv)
//{
//    int rc;
//
//#if MYNEWT_VAL(STATIC_CONFIG)
//    printf("error: no supported (static configuration)\n");
//    return 0;
//#endif
//
//    if (argc < 3) {
//        printf("error: name and value required\n");
//        return 0;
//    }
//
//    rc = config_set(argv[1], argc - 2, argv + 2);
//
//    switch (rc) {
//    case 0:
//        printf("ok, commit and reboot to apply\n");
//        g_app_data.modified = 1;
//        break;
//    case -ENOENT:
//        printf("error: invalid setting name: %s\n", argv[1]);
//        break;
//    case -EINVAL:
//        printf("error: invalid value: %s\n", argv[2]);
//        break;
//    case -ENOTSUP:
//        printf("error: unsupported value: %s\n", argv[2]);
//        break;
//    case -ERANGE:
//        printf("error: value out of range: %s\n", argv[2]);
//        break;
//    default:
//        printf("error: unknown\n");
//        break;
//    }
//
//    return 0;
//}
//
//static int
//cmd_commit(int argc, char **argv)
//{
//#if MYNEWT_VAL(STATIC_CONFIG)
//    printf("error: no supported (static configuration)\n");
//#else
//    os_sr_t sr;
//
//    OS_ENTER_CRITICAL(sr);
//    config_commit();
//    OS_EXIT_CRITICAL(sr);
//    printf("ok, reboot to apply\n");
//#endif
//
//    return 0;
//}
//
//static int
//cmd_default(int argc, char **argv)
//{
//#if MYNEWT_VAL(STATIC_CONFIG)
//    printf("error: no supported (static configuration)\n");
//#else
//    config_reset_to_defaults();
//    printf("ok, commit and reboot to apply\n");
//#endif
//
//    return 0;
//}
//
//static int
//cmd_reboot(int argc, char **argv)
//{
//    hal_system_reset();
//
//    return 0;
//}
//
//static int
//cmd_preset(int argc, char **argv)
//{
//#if MYNEWT_VAL(STATIC_CONFIG)
//    printf("error: no supported (static configuration)\n");
//#else
//    int i;
//
//    if (argc == 1) {
//        printf("supported presets:\n");
//        printf("%s | %s | %s | %s | %s\n", "name", "sr [Hz]", "dt [us]",
//               "br [bps]", "octets");
//        for (i = 0; i < ARRAY_SIZE(presets); i++) {
//            printf("%4s | %7ld | %7ld | %8ld | %6d\n", presets[i].name,
//                   presets[i].sampling_freq, presets[i].frame_duration,
//                   presets[i].bitrate,
//                   lc3_frame_bytes(presets[i].frame_duration,
//                                   presets[i].bitrate));
//        }
//    } else {
//        for (i = 0; i < ARRAY_SIZE(presets); i++) {
//            if (!strcmp(argv[1], presets[i].name)) {
//                g_app_cfg.lc3_sampling_freq = presets[i].sampling_freq;
//                g_app_cfg.lc3_frame_duration = presets[i].frame_duration;
//                g_app_cfg.lc3_bitrate = presets[i].bitrate;
//                g_app_data.modified = 1;
//                printf("ok, commit and reboot to apply\n");
//                break;
//            }
//        }
//
//        if (i == ARRAY_SIZE(presets)) {
//            printf("error: invalid preset (%s)\n", argv[1]);
//        }
//    }
//#endif
//
//    return 0;
//}

static const char *
addr2str(const ble_addr_t *addr)
{
    static char s[20];

    sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X/%d", addr->val[5], addr->val[4],
            addr->val[3], addr->val[2], addr->val[1], addr->val[0],
            addr->type % 10);

    return s;
}

static int
gap_listener_func(struct ble_gap_event *event, void *arg)
{
    const struct ble_hci_ev_le_meta *ev_meta;
    const struct ble_hci_ev_le_subev_big_sync_established *ev_est;
    const struct ble_hci_ev_le_subev_big_sync_lost *ev_lost;

    if ((event->type != BLE_GAP_EVENT_UNHANDLED_HCI_EVENT) ||
        !event->unhandled_hci.is_le_meta) {
        return 0;
    }

    ev_meta = event->unhandled_hci.ev;

    switch (ev_meta->subevent) {
    case BLE_HCI_LE_SUBEV_BIG_SYNC_ESTABLISHED:
        ev_est = event->unhandled_hci.ev;
        printf("HCI LE BIG Sync Established: status=%d big_handle=%d\n",
               ev_est->status, ev_est->big_handle);
        break;
    case BLE_HCI_LE_SUBEV_BIG_SYNC_LOST:
        ev_lost = event->unhandled_hci.ev;
        printf("HCI LE BIG Sync Lost: big_handle=%d reason=%d\n",
               ev_lost->big_handle, ev_lost->reason);
        break;
    default:
        printf("Unhandled HCI LE Meta event (0x%02x)\n", ev_meta->subevent);
        break;
    }

    return 0;
}

static int
create_big_sync(uint16_t sync_handle)
{
    struct ble_hci_le_big_create_sync_cp cp = { 0 };
    uint16_t opcode;
    int rc;

    cp.big_handle = 1;
    cp.sync_handle = le16toh(sync_handle);
    cp.encryption = 0;
    cp.mse = 10;
    cp.sync_timeout = 1000;
    cp.num_bis = 1;
    cp.bis[0] = 1;

    opcode = BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_BIG_CREATE_SYNC);

    rc = ble_hs_hci_cmd_tx(opcode, &cp, sizeof(cp), NULL, 0);
    printf("%s: rc=%d\n", __func__, rc);

    return rc;
}

static int
sync_gap_event_func(struct ble_gap_event *event, void *arg)
{
    const uint8_t *data;

    switch (event->type) {
    case BLE_GAP_EVENT_PERIODIC_SYNC:
        printf("Sync established (%d)\n", event->periodic_sync.status);

        if (event->periodic_sync.status == 0) {
            create_big_sync(event->periodic_sync.sync_handle);
        }

        ble_gap_disc_cancel();
        break;
    case BLE_GAP_EVENT_PERIODIC_REPORT:
        printf("Sync report (%d)\n", event->periodic_report.data_status);
        data = event->periodic_report.data;

        for (int i = 0; i < event->periodic_report.data_length; i++) {
//            uint8_t ad_len = data[i];
//            uint8_t ad_type = data[i + 1];
//            const uint8_t *ad_data = &data[i + 2];
//            printf("%d %d\n", ad_len, ad_type);
//
//            if (ad_type == BLE_HS_ADV_TYPE_INCOMP_NAME) {
//                name = calloc(1, ad_len + 1);
//                memcpy(name, ad_data, ad_len);
//            } else if (ad_type == BLE_HS_ADV_TYPE_COMP_NAME) {
//                name = calloc(1, ad_len + 1);
//                memcpy(name, ad_data, ad_len);
//            } else if (ad_type == BLE_HS_ADV_TYPE_SVC_DATA_UUID16) {
//                if (get_le16(ad_data) == 0x1852) {
//                    broadcast_id = get_le24(&ad_data[2]);
//                }
//            } el
            i += data[i];
        }
        break;
    case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
        printf("Sync lost (%d)\n", event->periodic_sync_lost.reason);
        break;
    default:
        printf("Unknown GAP event for sync: %d\n", event->type);
        break;
    }

    return 0;
}

static int
scan_gap_event_func(struct ble_gap_event *event, void *arg)
{
    const uint8_t *data;
    char *name = NULL;
    unsigned broadcast_id = 0;

    if (event->type != BLE_GAP_EVENT_EXT_DISC) {
        return 0;
    }

    if (event->ext_disc.periodic_adv_itvl == 0) {
        return 0;
    }

    data = event->ext_disc.data;

    for (int i = 0; i < event->ext_disc.length_data; i++) {
        uint8_t ad_len = data[i];
        uint8_t ad_type = data[i + 1];
        const uint8_t *ad_data = &data[i + 2];

        if (ad_type == BLE_HS_ADV_TYPE_INCOMP_NAME) {
            name = calloc(1, ad_len + 1);
            memcpy(name, ad_data, ad_len);
        } else if (ad_type == BLE_HS_ADV_TYPE_COMP_NAME) {
            name = calloc(1, ad_len + 1);
            memcpy(name, ad_data, ad_len);
        } else if (ad_type == BLE_HS_ADV_TYPE_SVC_DATA_UUID16) {
            if (get_le16(ad_data) == 0x1852) {
                broadcast_id = get_le24(&ad_data[2]);
            }
        }

        i += data[i];
    }

    printf("%s = %s / %d / %06x\n", addr2str(&event->ext_disc.addr), name,
           event->ext_disc.sid, broadcast_id);

    if (broadcast_id) {
        int rc;

//        rc = ble_gap_disc_cancel();
//        printf("rc = %d\n", rc);

        struct ble_gap_periodic_sync_params params = { 0 };
        params.reports_disabled = 1;
        params.skip = 0;
        params.sync_timeout = 1000;

        rc = ble_gap_periodic_adv_sync_create(&event->ext_disc.addr, event->ext_disc.sid, &params, sync_gap_event_func, NULL);
        printf("rc = %d\n", rc);
    }

    return 0;
}

static int
cmd_scan(int argc, char **argv)
{
    uint8_t own_addr_type;
    struct ble_gap_ext_disc_params params = { 0 };
    int rc;

    rc = ble_hs_id_infer_auto(1, &own_addr_type);
    printf("rc=%d\n", rc);

    params.itvl = BLE_GAP_SCAN_FAST_INTERVAL_MAX;
    params.window = BLE_GAP_SCAN_FAST_WINDOW;
    params.passive = 1;

    rc = ble_gap_ext_disc(own_addr_type, 0, 0, 1, 0, 0, &params, NULL,
                          scan_gap_event_func, NULL);
    printf("rc=%d\n", rc);

    return 0;
}

static const struct shell_cmd app_cmds[] = {
//    { .sc_cmd = "info",      .sc_cmd_func = cmd_info, },
//    { .sc_cmd = "get",       .sc_cmd_func = cmd_get, },
//    { .sc_cmd = "set",       .sc_cmd_func = cmd_set, },
//    { .sc_cmd = "commit",    .sc_cmd_func = cmd_commit, },
//    { .sc_cmd = "default",   .sc_cmd_func = cmd_default, },
//    { .sc_cmd = "reboot",    .sc_cmd_func = cmd_reboot, },
//    { .sc_cmd = "preset",    .sc_cmd_func = cmd_preset, },
    { .sc_cmd = "scan",      .sc_cmd_func = cmd_scan, },
    { 0 },
};

void
xoxo(void)
{
    cmd_scan(0, NULL);
}

void
shell_app_init(void)
{
    shell_register("app", app_cmds);
    shell_register_default_module("app");

    ble_gap_event_listener_register(&gap_listener, gap_listener_func, NULL);

    printf("elo!\n");
}
