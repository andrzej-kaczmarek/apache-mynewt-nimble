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
#include <stdint.h>
#include <syscfg/syscfg.h>
#include <nimble/ble.h>
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <controller/ble_ll.h>
#include <controller/ble_ll_adv.h>
#include <controller/ble_ll_crypto.h>
#include <controller/ble_ll_hci.h>
#include <controller/ble_ll_isoal.h>
#include <controller/ble_ll_iso_big.h>
#include <controller/ble_ll_pdu.h>
#include <controller/ble_ll_sched.h>
#include <controller/ble_ll_rfmgmt.h>
#include <controller/ble_ll_tmr.h>
#include <controller/ble_ll_utils.h>
#include <controller/ble_ll_whitelist.h>
#include "ble_ll_priv.h"
#include "controller/ble_ll_sync.h"
#include "ble_ll_iso_big_priv.h"

#if MYNEWT_VAL(BLE_LL_ISO_BROADCAST_RECEIVER)

struct big_sync_params {
    struct ble_ll_iso_big *big;
    struct ble_ll_sync_sm *syncsm;
    bool encryption;
    uint8_t broadcast_code[16];
    uint8_t mse;
    uint16_t sync_timeout;
    uint32_t bis_mask;
};

static struct big_sync_params sync_state;
static struct ble_ll_iso_big *big_active;

void ble_ll_iso_big_sync_hci_evt_established(uint8_t status);

static void
biginfo_func(struct ble_ll_sync_sm *syncsm, uint32_t sync_ticks,
             uint8_t sync_rem_us,const uint8_t *data, uint8_t len, void *arg)
{
    struct big_sync_params *bsp = &sync_state;
    struct ble_ll_iso_big *big = bsp->big;
    struct ble_ll_iso_bis *bis;
    uint32_t u32;
    bool encrypted;
    uint8_t bis_cnt;
    uint8_t phy;
    static uint32_t big_offset_us;
    int rc;

    BLE_LL_ASSERT(big);

    encrypted = (len == 57);
    u32 = get_le32(&data[0]);
    bis_cnt = (u32 >> 27) & 0x1F;
    phy = (data[27] >> 5) & 0x07;

    if (bsp->encryption != encrypted) {
        ble_ll_iso_big_sync_hci_evt_established(BLE_ERR_ENCRYPTION_MODE);
        ble_ll_sync_biginfo_cb_set(bsp->syncsm, NULL, NULL);
        return;
    }

    if (__builtin_popcount(bsp->bis_mask) > bis_cnt) {
        ble_ll_iso_big_sync_hci_evt_established(BLE_ERR_UNSUPPORTED);
        ble_ll_sync_biginfo_cb_set(bsp->syncsm, NULL, NULL);
        return;
    }

    switch (phy) {
    case 0:
        phy = BLE_PHY_1M;
        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    case 1:
        phy = BLE_PHY_2M;
        break;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case 2:
        phy = BLE_PHY_CODED;
        break;
#endif
    default:
        ble_ll_iso_big_sync_hci_evt_established(BLE_ERR_UNSUPPORTED);
        ble_ll_sync_biginfo_cb_set(bsp->syncsm, NULL, NULL);
        return;
    }

    u32 = get_le32(&data[0]);
    big_offset_us = (u32 & 0x3FFF) * ((u32 & 0x4000) ? 300 : 30);
    big->iso_interval = (u32 >> 15) & 0x0FFF;
//    big->num_bis = (u32 >> 27) & 0x1F;

    u32 = get_le32(&data[4]);
    big->nse = u32 & 0x1F;
    big->bn = (u32 >> 5) & 0x07;
    big->pto = (u32 >> 28) & 0x0F;

    u32 = get_le32(&data[8]);
    big->bis_spacing = u32 & 0x0FFFFF;
    big->irc = (u32 >> 20) & 0x0F;
    big->max_pdu = (u32 >> 24) & 0xFF;

    uint32_t seed_aa;
    seed_aa = get_le32(&data[13]);
    (void)seed_aa;

    u32 = get_le32(&data[17]);
    big->sdu_interval = u32 & 0x0FFFFF;
    big->max_sdu = (u32 >> 20) & 0x0FFF;

    big->crc_init = get_le16(&data[21]);
    memcpy(big->chan_map, &data[23], BLE_LL_CHAN_MAP_LEN);
    big->chan_map[4] &= 0x1f;
    big->chan_map_used = ble_ll_utils_chan_map_used_get(big->chan_map);

    big->phy = phy;

    big->bis_counter = ((uint64_t)(data[32] & 0x7F) << 32) | get_le32(&data[28]);
    big->big_counter = big->bis_counter / big->bn;

    big->framed = (data[32] >> 7) & 0x01;

    if (encrypted) {
        big->encrypted = 1;
        memcpy(big->giv, &data[33], 8);
        memcpy(big->gskd, &data[41], 16);
    }

    big->ctrl_aa = ble_ll_utils_calc_big_aa(seed_aa, 0);

    STAILQ_INIT(&big->bis_q);
    for (int i = 0; i < bis_cnt; i++) {
        bis = ble_ll_iso_big_alloc_bis(big);
        bis->crc_init = (big->crc_init << 8) | (bis->num);
        bis->aa = ble_ll_utils_calc_big_aa(seed_aa, bis->num);
        bis->chan_id = bis->aa ^ (bis->aa >> 16);
    }

    big->sch.start_time = sync_ticks;
    big->sch.remainder = sync_rem_us;

    ble_ll_tmr_add(&big->sch.start_time, &big->sch.remainder, big_offset_us);
    big->sch.end_time = big->sch.start_time + ble_ll_tmr_u2t_up(big->sync_delay) + 1;
    big->sch.start_time -= g_ble_ll_sched_offset_ticks;

    rc = ble_ll_sched_iso_big_sync(&big->sch);
    BLE_LL_ASSERT(rc == 0);

    /* XXX schedule instead */
    ble_ll_iso_big_sync_hci_evt_established(0);
    ble_ll_sync_biginfo_cb_set(bsp->syncsm, NULL, NULL);
}

static void
ble_ll_iso_big_sync_event_done(struct ble_ll_iso_big *big)
{
    int rc;

    ble_ll_rfmgmt_release();

    /* Use last rxd PDU as anchor */
    big->sch.start_time = big->event_start;
    big->sch.remainder = big->event_start_us;

    do {
        big->big_counter++;
        big->bis_counter += big->bn;

        /* XXX precalculate some data here? */

        ble_ll_tmr_add(&big->sch.start_time, &big->sch.remainder,
                       big->iso_interval * 1250);
        big->sch.end_time = big->sch.start_time +
                            ble_ll_tmr_u2t_up(big->sync_delay) + 1;
        big->sch.start_time -= g_ble_ll_sched_offset_ticks;

//        if (big->control_active) {
//            /* XXX fixme */
//            big->sch.end_time += 10;
//        }

        rc = ble_ll_sched_iso_big_sync(&big->sch);

        big->event_start = big->sch.start_time + g_ble_ll_sched_offset_ticks;
        big->event_start_us = big->sch.remainder;
    } while (rc < 0);
}

static void
ble_ll_iso_big_sync_event_done_ev(struct ble_npl_event *ev)
{
    struct ble_ll_iso_big *big;

    big = ble_npl_event_get_arg(ev);

    ble_ll_iso_big_sync_event_done(big);
}

static void
ble_ll_iso_big_sync_event_done_to_ll(struct ble_ll_iso_big *big)
{
    big_active = NULL;
    ble_ll_state_set(BLE_LL_STATE_STANDBY);
    ble_npl_eventq_put(&g_ble_ll_data.ll_evq, &big->event_done);
}

static int
ble_ll_iso_big_sync_subevent_rx(struct ble_ll_iso_big *big)
{
    struct ble_ll_iso_bis *bis;
    uint16_t chan_idx;
//    int to_tx;
//    int rc;

    bis = big->tx.bis;

    if (bis->tx.subevent_num == 1) {
        chan_idx = ble_ll_utils_dci_iso_event(big->big_counter, bis->chan_id,
                                              &bis->tx.prn_sub_lu,
                                              big->chan_map_used,
                                              big->chan_map,
                                              &bis->tx.remap_idx);
    } else {
        chan_idx = ble_ll_utils_dci_iso_subevent(bis->chan_id,
                                                 &bis->tx.prn_sub_lu,
                                                 big->chan_map_used,
                                                 big->chan_map,
                                                 &bis->tx.remap_idx);
    }

    ble_phy_setchan(chan_idx, bis->aa, bis->crc_init);

//    to_tx = (big->tx.subevents_rem > 1) || big->cstf;
//
//    rc = ble_phy_tx(ble_ll_iso_big_subevent_pdu_cb, big,
//                    to_tx ? BLE_PHY_TRANSITION_TX_TX
//                          : BLE_PHY_TRANSITION_NONE);
    return 0;
}

static int
ble_ll_iso_big_sync_event_sched_cb(struct ble_ll_sched_item *sch)
{
    struct ble_ll_iso_big *big;
    struct ble_ll_iso_bis *bis;
#if MYNEWT_VAL(BLE_LL_PHY)
    uint8_t phy_mode;
#endif
    int rc;

    big = sch->cb_arg;

    ble_ll_state_set(BLE_LL_STATE_BIG_SYNC);
    big_active = big;

    ble_ll_whitelist_disable();
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
    ble_phy_resolv_list_disable();
#endif
#if MYNEWT_VAL(BLE_LL_PHY)
    phy_mode = ble_ll_phy_to_phy_mode(big->phy, 0);
    ble_phy_mode_set(phy_mode, phy_mode);
#endif

    ble_ll_tx_power_set(g_ble_ll_tx_power);

    BLE_LL_ASSERT(!big->framed);

    /* XXX calculate this in advance at the end of previous event? */
    big->tx.subevents_rem = big->num_bis * big->nse;
    STAILQ_FOREACH(bis, &big->bis_q, bis_q_next) {
//        ble_ll_isoal_mux_event_start(&bis->mux, (uint64_t)big->event_start *
//                                                1000000 / 32768 +
//                                                big->event_start_us);

        bis->tx.subevent_num = 0;
        bis->tx.n = 0;
        bis->tx.g = 0;
    }

    /* Select 1st BIS for reception */
    big->tx.bis = STAILQ_FIRST(&big->bis_q);
    big->tx.bis->tx.subevent_num = 1;

    if (big->interleaved) {
        ble_phy_tifs_txtx_set(big->bis_spacing, 0);
    } else {
        ble_phy_tifs_txtx_set(big->sub_interval, 0);
    }

    if (big->encrypted) {
        ble_phy_encrypt_enable(big->gsk);
    } else {
        ble_phy_encrypt_disable();
    }

    rc = ble_phy_rx_set_start_time(sch->start_time + g_ble_ll_sched_offset_ticks,
                                   sch->remainder);
    if (rc) {
        ble_ll_iso_big_sync_event_done_to_ll(big);
        return BLE_LL_SCHED_STATE_DONE;
    }

    ble_phy_wfr_enable(100);

//    rc = ble_ll_iso_big_subevent_tx(big);
//    if (rc) {
//        ble_phy_disable();
//        ble_ll_iso_big_event_done_to_ll(big);
//        return BLE_LL_SCHED_STATE_DONE;
//    }

    ble_ll_iso_big_sync_subevent_rx(big);

    return BLE_LL_SCHED_STATE_RUNNING;
}

static int
ble_ll_iso_big_sync_create(uint8_t big_handle, uint16_t sync_handle,
                           struct big_sync_params *bsp)
{
    struct ble_ll_iso_big *big;
    struct ble_ll_sync_sm *syncsm;
    int rc;

    big = big_ll_iso_big_find(big_handle);
    if (big) {
        return -EALREADY;
    }

    big = ble_ll_iso_big_alloc(big_handle);
    if (!big) {
        return -ENOMEM;
    }

    syncsm = ble_ll_sync_get(sync_handle);
    if (!syncsm) {
        return -ENOENT;
    }

    big->sch.sched_type = BLE_LL_SCHED_TYPE_BIG_SYNC;
    big->sch.sched_cb = ble_ll_iso_big_sync_event_sched_cb;
    big->sch.cb_arg = big;
    ble_npl_event_init(&big->event_done, ble_ll_iso_big_sync_event_done_ev, big);

    rc = ble_ll_sync_biginfo_cb_set(syncsm, biginfo_func, NULL);
    if (rc) {
        return -ENOENT;
    }

    sync_state.big = big;
    sync_state.syncsm = syncsm;

    return 0;
}

int
ble_ll_iso_big_sync_rx_isr_start(uint8_t pdu_type, struct ble_mbuf_hdr *rxhdr)
{
    struct ble_ll_iso_big *big;

    BLE_LL_ASSERT(big_active);

    big = big_active;
    big->event_start = rxhdr->beg_cputime;
    big->event_start_us = rxhdr->rem_usecs;

    return 0;
}

int
ble_ll_iso_big_sync_rx_isr_end(uint8_t *rxbuf, struct ble_mbuf_hdr *rxhdr)
{
    struct ble_ll_iso_big *big;
//    uint8_t hdr_byte;
    uint8_t rx_pyld_len;
    struct os_mbuf *rxpdu = NULL;
//    int rc;

    big = big_active;
    BLE_LL_ASSERT(big);

//    hdr_byte = rxbuf[0];
    rx_pyld_len = rxbuf[1];

    rxpdu = ble_ll_rxpdu_alloc(rx_pyld_len + BLE_LL_PDU_HDR_LEN);

    if (rxpdu) {
        ble_phy_rxpdu_copy(rxbuf, rxpdu);
        ble_ll_rx_pdu_in(rxpdu);
    }

    ble_ll_iso_big_sync_event_done_to_ll(big);

    return 0;
}

void
ble_ll_iso_big_sync_rx_pkt_in(struct os_mbuf *rxpdu, struct ble_mbuf_hdr *rxhdr)
{

}

void
ble_ll_iso_big_sync_wfr_timer_exp(void)
{
    if (big_active) {
        ble_ll_iso_big_sync_event_done_to_ll(big_active);
    }
}

void
ble_ll_iso_big_sync_halt(void)
{
    if (big_active) {
        ble_ll_iso_big_sync_event_done_to_ll(big_active);
    }
}

void
ble_ll_iso_big_sync_hci_evt_established(uint8_t status)
{
    struct big_sync_params *bsp = &sync_state;
    struct ble_ll_iso_big *big;
//    struct ble_ll_iso_bis *bis;
    struct ble_hci_ev_le_subev_big_sync_established *evt;
    struct ble_hci_ev *hci_ev;
//    uint8_t idx;

    big = bsp->big;

    hci_ev = ble_transport_alloc_evt(0);
    if (!hci_ev) {
        BLE_LL_ASSERT(0);
        /* XXX should we retry later? */
        return;
    }
    hci_ev->opcode = BLE_HCI_EVCODE_LE_META;
    hci_ev->length = sizeof(*evt) + big->num_bis * sizeof(evt->conn_handle[0]);

    evt = (void *)hci_ev->data;
    memset(evt, 0, hci_ev->length);
    evt->subev_code = BLE_HCI_LE_SUBEV_BIG_SYNC_ESTABLISHED;
    evt->status = status;

    evt->big_handle = big->handle;

    if (status == 0) {
        /* Core 5.3, Vol 6, Part G, 3.2.2 */
        put_le24(evt->transport_latency_big,
                 big->sync_delay +
                 (big->pto * (big->nse / big->bn - big->irc) + 1) * big->iso_interval * 1250 -
                 big->sdu_interval);
        evt->nse = big->nse;
        evt->bn = big->bn;
        evt->pto = big->pto;
        evt->irc = big->irc;
        evt->max_pdu = htole16(big->max_pdu);
        evt->iso_interval = htole16(big->iso_interval);
        evt->num_bis = big->num_bis;
    }

//    idx = 0;
//    STAILQ_FOREACH(bis, &big->bis_q, bis_q_next) {
//        evt->conn_handle[idx] = htole16(bis->conn_handle);
//        idx++;
//    }

    ble_ll_hci_event_send(hci_ev);
}

int
ble_ll_iso_big_sync_hci_create(const uint8_t *cmdbuf, uint8_t len)
{
    const struct ble_hci_le_big_create_sync_cp *cmd = (void *)cmdbuf;
    uint16_t sync_handle;
    struct big_sync_params *bsp = &sync_state;
    int rc;

    if (len != sizeof(*cmd)) {
        return BLE_ERR_INV_HCI_CMD_PARMS;
    }

    if (sync_state.big) {
        return BLE_ERR_CMD_DISALLOWED;
    }

    sync_handle = le16toh(cmd->sync_handle);

    if (!IN_RANGE(cmd->big_handle, 0x00, 0xef) ||
        !IN_RANGE(sync_handle, 0x00, 0x0eff) ||
        !IN_RANGE(cmd->mse, 0x00, 0x1f) ||
        !IN_RANGE(le16toh(cmd->sync_timeout), 0x000a, 0x4000) ||
        !IN_RANGE(cmd->num_bis, 0x01, 0x1f) ||
        (cmd->encryption) > 1) {
        return BLE_ERR_INV_HCI_CMD_PARMS;
    }

    bsp->encryption = cmd->encryption;
    memcpy(bsp->broadcast_code, cmd->broadcast_code, sizeof(bsp->broadcast_code));
    bsp->sync_timeout = le16toh(cmd->sync_timeout);
    bsp->mse = cmd->mse;
    bsp->bis_mask = 0;

    for (int i = 0; i < cmd->num_bis; i++) {
        uint8_t bis = cmd->bis[i];
        if (!IN_RANGE(bis, 0x01, 0x1f)) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        bsp->bis_mask |= 1 << bis;
    }

    rc = ble_ll_iso_big_sync_create(cmd->big_handle, sync_handle, bsp);
    switch (rc) {
    case 0:
        break;
    case -EINVAL:
        return BLE_ERR_INV_HCI_CMD_PARMS;
    case -EALREADY:
        return BLE_ERR_CMD_DISALLOWED;
    case -ENOMEM:
        return BLE_ERR_CONN_REJ_RESOURCES;
    case -ENOENT:
        return BLE_ERR_UNK_ADV_INDENT;
    default:
        return BLE_ERR_UNSPECIFIED;
    }

    return 0;
}

int
ble_ll_iso_big_sync_hci_terminate(const uint8_t *cmdbuf, uint8_t len)
{
//    const struct ble_hci_le_terminate_big_sync_cp *cmd = (void *)cmdbuf;
//    int err;
//
//    err = ble_ll_iso_big_terminate(cmd->big_handle, cmd->reason);
//    switch (err) {
//    case 0:
//        break;
//    case -ENOENT:
//        return BLE_ERR_UNK_ADV_INDENT;
//    default:
//        return BLE_ERR_UNSPECIFIED;
//    }

    return 0;
}

void
ble_ll_iso_big_sync_init(void)
{
//    struct ble_ll_iso_big *big;
//    struct ble_ll_iso_bis *bis;
//    uint8_t idx;
//
//    memset(big_pool, 0, sizeof(big_pool));
//    memset(bis_pool, 0, sizeof(bis_pool));
//
//    for (idx = 0; idx < BIG_POOL_SIZE; idx++) {
//        big = &big_pool[idx];
//
//        big->handle = BIG_HANDLE_INVALID;
//
//        big->sch.sched_type = BLE_LL_SCHED_TYPE_BIG;
//        big->sch.sched_cb = ble_ll_iso_big_event_sched_cb;
//        big->sch.cb_arg = big;
//
//        ble_npl_event_init(&big->event_done, ble_ll_iso_big_event_done_ev, big);
//    }
//
//    for (idx = 0; idx < BIS_POOL_SIZE; idx++) {
//        bis = &bis_pool[idx];
//        bis->conn_handle = BLE_LL_CONN_HANDLE(BLE_LL_CONN_HANDLE_TYPE_BIS, idx);
//    }
//
//    big_pool_free = ARRAY_SIZE(big_pool);
//    bis_pool_free = ARRAY_SIZE(bis_pool);
}

void
ble_ll_iso_big_sync_reset(void)
{
//    struct ble_ll_iso_big *big;
//    int idx;
//
//    for (idx = 0; idx < BIG_POOL_SIZE; idx++) {
//        big = &big_pool[idx];
//        ble_ll_iso_big_free(big);
//    }
//
    ble_ll_iso_big_sync_init();
}

#endif /* BLE_LL_ISO_BROADCAST_RECEIVER */
