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
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "console/console.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include <bs_tracing.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifndef BABBLESIM
#error This app only works on BabbleSim
#endif

#define TESTER_TASK_PRIO 0xf0

#define TESTER_TASK_STACK_SIZE (128)
static struct os_task tester_task;
static os_stack_t tester_task_stack[TESTER_TASK_STACK_SIZE];

static int
gap_event(struct ble_gap_event *event, void *arg)
{
    bs_trace_raw(0, "%s: %d\n", __func__, event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_EXT_DISC:
        bs_trace_raw(0, "%d %d\n", event->ext_disc.prim_phy, event->ext_disc.sec_phy);
    }

    return 0;
}

static int role;
static int fd;
static uint32_t syncword;

static int
is_role1(void)
{
    return role == 1;
}

static void
tester_init(void)
{
    const char *env;
    uint32_t x;

    env = getenv("ROLE");
    if (env) {
        role = atoi(env);
    } else {
        role = 0;
    }

    if ((role != 1) && (role != 2)) {
        perror("invalid");
    }

    if (is_role1()) {
        fd = open("babbletester", O_WRONLY);
        assert(fd >= 0);

        x = 0x00babb1e;
        write(fd, &x, sizeof(x));
    } else {
        fd = open("babbletester", O_RDONLY);
        assert(fd >= 0);

        read(fd, &x, sizeof(x));
        bs_trace_raw(0, "%08x\n", x);
    }
}

static void
tester_synchronize(void)
{
    uint32_t token;
    int num;

    bs_trace_info_time(0, "%s: %08x\n", __func__, syncword);

    if (is_role1()) {
        token = syncword;
        write(fd, &token, sizeof(token));
    } else {
        do {
            os_time_delay(1);
            ioctl(fd, FIONREAD, &num);
        } while (num < sizeof(token));
        read(fd, &token, sizeof(token));
        if (token != syncword) {
            bs_trace_error("syncword mismatch: %08x\n", token);
        } else {
            bs_trace_info_time(0, "syncword match\n");
        }
    }
}

static void
t1_r1(void)
{
    const int inst = 0;
    struct ble_gap_ext_adv_params p;
    int rc;

    memset(&p, 0, sizeof(p));
    p.own_addr_type = BLE_OWN_ADDR_PUBLIC;
    p.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    p.itvl_max = BLE_GAP_ADV_ITVL_MS(100);
    p.primary_phy = BLE_HCI_LE_PHY_1M;
    p.secondary_phy = BLE_HCI_LE_PHY_2M;

    rc = ble_gap_ext_adv_configure(inst, &p, NULL, gap_event, NULL);
    assert(rc == 0);

    rc = ble_gap_ext_adv_start(inst, 0, 0);
    assert (rc == 0);

    os_time_delay(os_time_ms_to_ticks32(10000));

    tester_synchronize();

    rc = ble_gap_ext_adv_stop(inst);
    assert(rc == 0);
}

static void
t1_r2(void)
{
    struct ble_gap_ext_disc_params p;
    int rc;

    memset(&p, 0, sizeof(p));
    p.itvl = BLE_GAP_SCAN_FAST_INTERVAL_MAX;
    p.window = BLE_GAP_SCAN_FAST_WINDOW;
    p.passive = 1;

    rc = ble_gap_ext_disc(BLE_ADDR_PUBLIC, 0, 0, 0, BLE_HCI_SCAN_FILT_NO_WL,
                              0, &p, NULL, gap_event, NULL);
    assert(rc == 0);

    tester_synchronize();
}

static void
t2_r1(void)
{
    const int inst = 0;
    struct ble_gap_ext_adv_params p;
    int rc;

    memset(&p, 0, sizeof(p));
    p.own_addr_type = BLE_OWN_ADDR_PUBLIC;
    p.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    p.itvl_max = BLE_GAP_ADV_ITVL_MS(100);
    p.primary_phy = BLE_HCI_LE_PHY_1M;
    p.secondary_phy = BLE_HCI_LE_PHY_2M;

    rc = ble_gap_ext_adv_configure(inst, &p, NULL, gap_event, NULL);
    assert(rc == 0);

    rc = ble_gap_ext_adv_start(inst, 0, 0);
    assert (rc == 0);

    os_time_delay(os_time_ms_to_ticks32(10000));

    tester_synchronize();

    rc = ble_gap_ext_adv_stop(inst);
    assert(rc == 0);
}

static void
t2_r2(void)
{
    const int inst = 0;
    struct ble_gap_ext_adv_params p;
    int rc;

    memset(&p, 0, sizeof(p));
    p.own_addr_type = BLE_OWN_ADDR_PUBLIC;
    p.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    p.itvl_max = BLE_GAP_ADV_ITVL_MS(100);
    p.primary_phy = BLE_HCI_LE_PHY_1M;
    p.secondary_phy = BLE_HCI_LE_PHY_2M;

    rc = ble_gap_ext_adv_configure(inst, &p, NULL, gap_event, NULL);
    assert(rc == 0);

    rc = ble_gap_ext_adv_start(inst, 0, 0);
    assert (rc == 0);

    os_time_delay(os_time_ms_to_ticks32(10000));

    tester_synchronize();

    rc = ble_gap_ext_adv_stop(inst);
    assert(rc == 0);
}

typedef void (test_fn)(void);

static void
run_test(uint32_t id, test_fn *role1_fn, test_fn *role2_fn)
{
    syncword = id;

    tester_synchronize();

    if (is_role1()) {
        role1_fn();
    } else {
        role2_fn();
    }
}

static void
tester_task_fn(void *arg)
{
    run_test(0x12345678, t1_r1, t1_r2);
    run_test(0x12365678, t2_r1, t2_r2);

    exit(0);
}

static void
on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    rc = os_task_init(&tester_task, "tester", tester_task_fn, NULL,
                      TESTER_TASK_PRIO, OS_WAIT_FOREVER, tester_task_stack,
                      TESTER_TASK_STACK_SIZE);
    assert(rc == 0);
}

static void
on_reset(int reason)
{
    exit(1);
}

static int
main_fn(int argc, char **argv)
{
    sysinit();

    tester_init();

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}

int
main(int argc, char **argv)
{
    extern void bsim_init(int argc, char** argv, void *main_fn);
    bsim_init(argc, argv, main_fn);

    return 0;
}
