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

#ifndef H_BLE_LL_ISO_BIG_PRIV_
#define H_BLE_LL_ISO_BIG_PRIV_

#ifdef __cplusplus
extern "C" {
#endif

struct ble_ll_iso_big;

struct ble_ll_iso_bis {
    struct ble_ll_iso_big *big;
    uint8_t num;
    uint16_t conn_handle;

    uint32_t aa;
    uint32_t crc_init;
    uint16_t chan_id;
    uint8_t iv[8];

    struct {
        uint16_t prn_sub_lu;
        uint16_t remap_idx;

        uint8_t subevent_num;
        uint8_t n;
        uint8_t g;
    } tx;

    struct ble_ll_isoal_mux mux;
    uint16_t num_completed_pkt;

    STAILQ_ENTRY(ble_ll_iso_bis) bis_q_next;
};

STAILQ_HEAD(ble_ll_iso_bis_q, ble_ll_iso_bis);

struct ble_ll_iso_big {
    struct ble_ll_adv_sm *advsm;

    uint8_t handle;
    uint8_t num_bis;
    uint16_t iso_interval;
    uint16_t bis_spacing;
    uint16_t sub_interval;
    uint8_t phy;
    uint8_t max_pdu;
    uint16_t max_sdu;
    uint16_t mpt;
    uint8_t bn; /* 1-7, mandatory 1 */
    uint8_t pto; /* 0-15, mandatory 0 */
    uint8_t irc; /* 1-15  */
    uint8_t nse; /* 1-31 */
    uint8_t interleaved : 1;
    uint8_t framed : 1;
    uint8_t encrypted : 1;
    uint8_t giv[8];
    uint8_t gskd[16];
    uint8_t gsk[16];
    uint8_t iv[8];
    uint8_t gc;

    uint32_t sdu_interval;

    uint32_t ctrl_aa;
    uint16_t crc_init;
    uint8_t chan_map[BLE_LL_CHAN_MAP_LEN];
    uint8_t chan_map_used;

    uint8_t biginfo[33];

    uint64_t big_counter;
    uint64_t bis_counter;

    uint32_t sync_delay;
    uint32_t event_start;
    uint8_t event_start_us;
    struct ble_ll_sched_item sch;
    struct ble_npl_event event_done;

    struct {
        uint16_t subevents_rem;
        struct ble_ll_iso_bis *bis;
    } tx;

    struct ble_ll_iso_bis_q bis_q;

#if MYNEWT_VAL(BLE_LL_ISO_HCI_FEEDBACK_INTERVAL_MS)
    uint32_t last_feedback;
#endif

    uint8_t cstf : 1;
    uint8_t cssn : 4;
    uint8_t control_active : 3;
    uint16_t control_instant;

    uint8_t chan_map_new_pending : 1;
    uint8_t chan_map_new[BLE_LL_CHAN_MAP_LEN];

    uint8_t term_pending : 1;
    uint8_t term_reason : 7;

    STAILQ_ENTRY(ble_ll_iso_big) big_q_next;
};

struct ble_ll_iso_big *big_ll_iso_big_find(uint8_t big_handle);
struct ble_ll_iso_big *ble_ll_iso_big_alloc(uint8_t big_handle);
struct ble_ll_iso_bis *ble_ll_iso_big_alloc_bis(struct ble_ll_iso_big *big);
void ble_ll_iso_big_free2(struct ble_ll_iso_big *big);

#ifdef __cplusplus
}
#endif

#endif /* H_BLE_LL_ISO_BIG_PRIV_ */
