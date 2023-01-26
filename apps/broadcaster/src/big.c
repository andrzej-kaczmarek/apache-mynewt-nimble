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
#include <stdint.h>
#include <string.h>
#include <nimble/hci_common.h>
#include <host/ble_gap.h>
#include <lc3.h>
#include "app_priv.h"
#include "om.h"

extern int ble_hs_hci_cmd_tx(uint16_t opcode,
                             const void *cmd, uint8_t cmd_len,
                             void *rsp, uint8_t rsp_len);

static int
gap_event(struct ble_gap_event *event, void *arg)
{
     return 0;
}

static void
broadcast_code_to_hci(const char *code, uint8_t *out)
{
    memset(out, 0, 16);
    strncpy((char *)out, code, 16);
}

static int
base_csc_empty(struct os_mbuf *om, void *arg)
{
    uint8_t sr;
    uint8_t dt;
    uint16_t fb;

    sr = LC3_SAMPLING_FREQ == 16000 ? 0x03 :
         LC3_SAMPLING_FREQ == 24000 ? 0x05 :
         LC3_SAMPLING_FREQ == 32000 ? 0x06 :
         LC3_SAMPLING_FREQ == 48000 ? 0x08 : 0x01;
    dt = LC3_FRAME_DURATION == 7500 ? 0x00 : 0x01;
    fb = lc3_frame_bytes(LC3_FRAME_DURATION, LC3_BITRATE);

    om_put_ltv_u8(om, 0x01, sr);
    om_put_ltv_u8(om, 0x02, dt);
    om_put_ltv_u16(om, 0x04, fb);

    return 0;
}

static int
base_csc_audioloc(struct os_mbuf *om, void *arg)
{
    uint32_t loc = POINTER_TO_UINT(arg);

    om_put_ltv_u32(om, 0x03, loc);

    return 0;
}

static int
base_metadata(struct os_mbuf *om, void *arg)
{
    /* Streaming Audio Contexts */
    om_put_ltv_u16(om, 0x02, 0x0004);
    /* Program Info */
    om_put_ltv_utf8(om, 0x03, "something from USB");

    return 0;
}

static int
base_factory(struct os_mbuf *om, void *arg)
{
    /* BAAS UUID */
    om_put_u16(om, 0x1851);
    /* Presentation Delay (20ms) */
    om_put_u24(om, 20000);
    /* Num Subgroups */
    om_put_u8(om, 1);
    /* Num BIS */
    om_put_u8(om, BIG_NUM_BIS);
    /* Codec ID */
    om_put_u8(om, 6);
    om_put_u32(om, 0);
    /* Codec Specific Configuration */
    om_ltv_group(om, 0xff, base_csc_empty, NULL);
    /* Metadata */
    om_ltv_group(om, 0xff, base_metadata, NULL);
    /* Codec Specific Configuration */
    if (MYNEWT_VAL(USB_AUDIO_OUT_CHANNELS) == 1) {
        om_put_u8(om, 0x01);
        om_ltv_group(om, 0xff, base_csc_empty, NULL);
    } else if (BIG_NUM_BIS == 1) {
        om_put_u8(om, 0x01);
        om_ltv_group(om, 0xff, base_csc_audioloc, UINT_TO_POINTER(0x03));
    } else {
        om_put_u8(om, 0x01);
        om_ltv_group(om, 0xff, base_csc_audioloc, UINT_TO_POINTER(0x01));
        om_put_u8(om, 0x02);
        om_ltv_group(om, 0xff, base_csc_audioloc, UINT_TO_POINTER(0x02));
    }

    return 0;
}

static int
baas_factory(struct os_mbuf *om, void *arg)
{
    /* BAAS UUID */
    om_put_u16(om, 0x1852);
    /* Broadcast ID */
    om_put_u24(om, g_app_data.broadcast_id);

    return 0;
}

static int
pbas_factory(struct os_mbuf *om, void *arg)
{
    uint8_t features = 0;

    if (g_app_cfg.big_encryption) {
        features |= 0x01;
    }
    if ((LC3_FRAME_DURATION == 10000) &&
        (((LC3_SAMPLING_FREQ == 16000) && (LC3_BITRATE == 32000)) ||
         ((LC3_SAMPLING_FREQ == 24000) && (LC3_BITRATE == 48000)))) {
        features |= 0x02;
    }
    if (LC3_SAMPLING_FREQ == 48000) {
        features |= 0x04;
    }

    /* PBAS UUID */
    om_put_u16(om, 0x1856);
    /* Broadcast ID */
    om_put_u8(om, features);
    /* Metadata Length */
    om_put_u8(om, 0);

    return 0;
}

static int
create_adv_ext(uint8_t inst)
{
    struct ble_gap_ext_adv_params eap = { 0 };
    struct os_mbuf *om;
    uint8_t own_addr_type;
    int8_t tx_power;
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);

    eap.itvl_min = BLE_GAP_ADV_ITVL_MS(57);
    eap.itvl_max = BLE_GAP_ADV_ITVL_MS(57);
    eap.own_addr_type = own_addr_type;
    eap.primary_phy = BLE_GAP_LE_PHY_1M;
    eap.secondary_phy = BLE_GAP_LE_PHY_2M;
    eap.sid = 1;

    rc = ble_gap_ext_adv_configure(inst, &eap, &tx_power, gap_event, NULL);
    assert(rc == 0);

    om = os_msys_get_pkthdr(0, 0);
    assert(om);

    om_ltv_group(om, 0x16, baas_factory, NULL);
    om_ltv_group(om, 0x16, pbas_factory, NULL);
    om_put_ltv_utf8(om, 0x30, g_app_cfg.broadcast_name);
    if (strlen(g_app_cfg.gap_local_name)) {
        om_put_ltv_utf8(om, 0x09, g_app_cfg.gap_local_name);
    }
    if (g_app_cfg.gap_appearance) {
        om_put_ltv_u16(om, 0x19, g_app_cfg.gap_appearance);
    }

    rc = ble_gap_ext_adv_set_data(inst, om);
    assert(rc == 0);

    return 0;
}

static int
create_adv_sync(uint8_t inst)
{
    struct ble_gap_periodic_adv_params pap = { 0 };
    struct os_mbuf *om;
    int rc;

    pap.itvl_min = BLE_GAP_ADV_ITVL_MS(107);
    pap.itvl_max = BLE_GAP_ADV_ITVL_MS(107);

    rc = ble_gap_periodic_adv_configure(inst, &pap);
    assert(rc == 0);

    om = os_msys_get_pkthdr(0, 0);
    assert(om);

    om_ltv_group(om, 0x16, base_factory, NULL);

    rc = ble_gap_periodic_adv_set_data(inst, om, NULL);
    assert(rc == 0);

    return 0;
}

static int
create_big(uint8_t inst)
{
    struct ble_hci_le_create_big_test_cp cp = { 0 };
    uint16_t opcode;
    int rc;

    cp.big_handle = 1;
    cp.adv_handle = inst;
    cp.num_bis = BIG_NUM_BIS;
    put_le24(&cp.sdu_interval, LC3_FRAME_DURATION);
    cp.iso_interval = BLE_GAP_CONN_ITVL_MS(LC3_FRAME_DURATION / 1000.0f) * BIG_BN;
    cp.nse = BIG_BN * (BIG_IRC + BIG_PT);
    cp.max_sdu = g_app_data.big_sdu;
    cp.max_pdu = cp.max_sdu;
    cp.phy = 1 << (g_app_cfg.big_phy - 1);
    cp.packing = g_app_cfg.big_packing;
    cp.framing = 0;
    cp.bn = BIG_BN;
    cp.irc = BIG_IRC;
    cp.pto = BIG_PTO;
    cp.encryption = g_app_cfg.big_encryption;
    if (cp.encryption) {
        broadcast_code_to_hci(g_app_cfg.broadcast_code, cp.broadcast_code);
    } else {
        broadcast_code_to_hci("", cp.broadcast_code);
    }

    opcode = BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_CREATE_BIG_TEST);
    rc = ble_hs_hci_cmd_tx(opcode, &cp, sizeof(cp), NULL, 0);
    assert(rc == 0);

    return 0;
}

static int
start(int inst)
{
    int rc;

    rc = ble_gap_ext_adv_start(inst, 0, 0);
    assert(rc == 0);

    rc = ble_gap_periodic_adv_start(inst, NULL);
    assert (rc == 0);

    return 0;
}

int
big_start(void)
{
    static const uint8_t inst = 1;
    int rc;

    rc = create_adv_ext(inst);
    assert(rc == 0);

    rc = create_adv_sync(inst);
    assert(rc == 0);

    rc = create_big(inst);
    assert(rc == 0);

    rc = start(inst);
    assert(rc == 0);

    return 0;
}
