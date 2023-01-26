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

#include <stdint.h>
#include <string.h>
#include <os/os_mbuf.h>

typedef int (om_ltv_group_func_t)(struct os_mbuf *om, void *arg);

int
om_ltv_group(struct os_mbuf *om, uint8_t type, om_ltv_group_func_t *func, void *arg)
{
    uint8_t *data;
    int len;
    int rc;

    data = os_mbuf_extend(om, type == 0xff ? 1 : 2);
    len = os_mbuf_len(om);

    rc = func(om, arg);
    if (rc) {
        return rc;
    }

    data[0] = os_mbuf_len(om) - len;
    if (type != 0xff) {
        data[0] += 1;
        data[1] = type;
    }

    return 0;
}

int
om_put_ltv_u8(struct os_mbuf *om, uint8_t type, uint8_t val)
{
    return os_mbuf_append(om, (uint8_t[]){2, type, val}, 3);
}

int
om_put_ltv_u16(struct os_mbuf *om, uint8_t type, uint16_t val)
{
    return os_mbuf_append(om, (uint8_t[]){3, type, val, val >> 8}, 4);
}

int
om_put_ltv_u24(struct os_mbuf *om, uint8_t type, uint32_t val)
{
    return os_mbuf_append(om, (uint8_t[]){4, type, val, val >> 8,
                                          val >> 16}, 5);
}

int
om_put_ltv_u32(struct os_mbuf *om, uint8_t type, uint32_t val)
{
    return os_mbuf_append(om, (uint8_t[]){5, type, val, val >> 8,
                                          val >> 16, val >> 24}, 6);
}

int
om_put_ltv_utf8(struct os_mbuf *om, uint8_t type, const char *str)
{
    os_mbuf_append(om, (uint8_t[]){1 + strlen(str), type}, 2);
    os_mbuf_append(om, str, strlen(str));

    return 0;
}

int
om_put_u8(struct os_mbuf *om, uint8_t val)
{
    return os_mbuf_append(om, (uint8_t[]){val}, 1);
}

int
om_put_u16(struct os_mbuf *om, uint16_t val)
{
    return os_mbuf_append(om, (uint8_t[]){val, val >> 8}, 2);
}

int
om_put_u24(struct os_mbuf *om, uint32_t val)
{
    return os_mbuf_append(om, (uint8_t[]){val, val >> 8, val >> 16}, 3);
}

int
om_put_u32(struct os_mbuf *om, uint32_t val)
{
    return os_mbuf_append(om, (uint8_t[]){val, val >> 8, val >> 16, val >> 24}, 4);
}
