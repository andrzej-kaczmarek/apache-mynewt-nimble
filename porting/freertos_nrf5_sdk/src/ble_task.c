#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "nimble_port.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/ias/ble_svc_ias.h"

#if LEDS_NUMBER < 4
#error "Board is not equipped with enough amount of LEDs"
#endif

static const char gap_name[] = "freertos";

static TaskHandle_t task_h;
static TimerHandle_t led_tmr_h;

static void start_advertise(void);

static int
ias_event_cb(uint8_t alert_level)
{
    switch (alert_level) {
    case BLE_SVC_IAS_ALERT_LEVEL_NO_ALERT:
        xTimerStop(led_tmr_h, portMAX_DELAY);
        bsp_board_leds_off();
        break;
    case BLE_SVC_IAS_ALERT_LEVEL_MILD_ALERT:
        bsp_board_led_on(BSP_BOARD_LED_0);
        bsp_board_led_off(BSP_BOARD_LED_1);
        bsp_board_led_off(BSP_BOARD_LED_2);
        bsp_board_led_on(BSP_BOARD_LED_3);
        xTimerStart(led_tmr_h, portMAX_DELAY);
        break;
    case BLE_SVC_IAS_ALERT_LEVEL_HIGH_ALERT:
        bsp_board_leds_on();
        xTimerStart(led_tmr_h, portMAX_DELAY);
        break;
    }

    return 0;
}

static void
put_ad(uint8_t ad_type, uint8_t ad_len, const void *ad, uint8_t *buf,
       uint8_t *len)
{
    buf[(*len)++] = ad_len + 1;
    buf[(*len)++] = ad_type;

    memcpy(&buf[*len], ad, ad_len);

    *len += ad_len;
}

static void
update_ad(void)
{
    uint8_t ad[BLE_HS_ADV_MAX_SZ];
    uint8_t ad_len = 0;
    uint8_t ad_flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    uint16_t ad_uuid = htole16(BLE_SVC_IAS_UUID16);

    put_ad(BLE_HS_ADV_TYPE_FLAGS, 1, &ad_flags, ad, &ad_len);
    put_ad(BLE_HS_ADV_TYPE_COMP_NAME, sizeof(gap_name), gap_name, ad, &ad_len);
    put_ad(BLE_HS_ADV_TYPE_COMP_UUIDS16, sizeof(ad_uuid), &ad_uuid, ad, &ad_len);

    ble_gap_adv_set_data(ad, ad_len);
}

static int
gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) {
            start_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        start_advertise();
        break;
    }

    return 0;
}

static void
start_advertise(void)
{
    struct ble_gap_adv_params advp;
    int rc;

    update_ad();

    memset(&advp, 0, sizeof advp);
    advp.conn_mode = BLE_GAP_CONN_MODE_UND;
    advp.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &advp, gap_event_cb, NULL);
    assert(rc == 0);
}

static void
on_sync_cb(void)
{
    start_advertise();
}

static void
task_func(void *param)
{
    int rc;

    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
    rc = hal_timer_init(5, NULL);
    assert(rc == 0);
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);

    nimble_init();

    ble_hs_cfg.sync_cb = on_sync_cb;

    ble_svc_gap_device_name_set(gap_name);
    ble_svc_ias_set_cb(ias_event_cb);

    nimble_run();
}

static void led_tmr_func(void *param)
{
    bsp_board_led_invert(BSP_BOARD_LED_0);
    bsp_board_led_invert(BSP_BOARD_LED_1);
    bsp_board_led_invert(BSP_BOARD_LED_2);
    bsp_board_led_invert(BSP_BOARD_LED_3);
}

void start_nimble(void)
{
    void ble_ll_task(void *arg);

    led_tmr_h = xTimerCreate("led", 500, pdTRUE, NULL, led_tmr_func);

    xTaskCreate(task_func, "nimble", configMINIMAL_STACK_SIZE + 400,
                NULL, 2, &task_h);

    xTaskCreate(ble_ll_task, "ll", configMINIMAL_STACK_SIZE + 100,
                NULL, 0, &task_h);
}
