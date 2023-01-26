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

#include <sysinit/sysinit.h>
#include <syscfg/syscfg.h>
#include <os/os.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include "app_priv.h"

static void
on_ble_reset(int reason)
{
}

static void
on_ble_sync(void)
{
    ble_hs_util_ensure_addr(0);

    if (g_app_cfg.broadcast_id) {
        g_app_data.broadcast_id = g_app_cfg.broadcast_id;
    } else {
        ble_hs_hci_rand(&g_app_data.broadcast_id, 4);
        g_app_data.broadcast_id &= 0x00ffffff;
    }

    big_start();
}

int
mynewt_main(int argc, char **argv)
{
    sysinit();

    ble_hs_cfg.reset_cb = on_ble_reset;
    ble_hs_cfg.sync_cb = on_ble_sync;

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);

    return 0;
}
