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

#ifndef H_OM_
#define H_OM_

#include <stdint.h>
#include <os/os_mbuf.h>

typedef int (om_ltv_group_func_t)(struct os_mbuf *om, void *arg);

int om_ltv_group(struct os_mbuf *om, uint8_t type, om_ltv_group_func_t *func, void *arg);
int om_put_ltv_u8(struct os_mbuf *om, uint8_t type, uint8_t val);
int om_put_ltv_u16(struct os_mbuf *om, uint8_t type, uint16_t val);
int om_put_ltv_u24(struct os_mbuf *om, uint8_t type, uint32_t val);
int om_put_ltv_u32(struct os_mbuf *om, uint8_t type, uint32_t val);
int om_put_ltv_utf8(struct os_mbuf *om, uint8_t type, const char *str);
int om_put_u8(struct os_mbuf *om, uint8_t val);
int om_put_u16(struct os_mbuf *om, uint16_t val);
int om_put_u24(struct os_mbuf *om, uint32_t val);
int om_put_u32(struct os_mbuf *om, uint32_t val);

#endif