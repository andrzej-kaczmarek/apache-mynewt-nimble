#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <syscfg/syscfg.h>
#include <hal/hal_gpio.h>
#include <bsp/bsp.h>

#include <os/os.h>
#include <common/tusb_fifo.h>
#include <class/audio/audio_device.h>
#include <usb_audio.h>

#include <lc3.h>
#include <nrf_clock.h>

#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include "app_priv.h"
#include "host/ble_gap.h"


//#ifdef BSP_nordic_pca10059
//#ifdef BSP_nordic_thingy53
#if 1
#define TP_PIN_DATA_ISR     0
#define TP_PIN_DATA_CB      0
#define TP_PIN_DATA_IN      0
#define TP_PIN_RESAMPLE     0
#define TP_PIN_ENCODE       0
#define TP_PIN_IPC_TX       0
#define TP_PIN_IPC_DROP     0
#else
#define TP_PIN_DATA_ISR     ARDUINO_PIN_D1
#define TP_PIN_DATA_CB      0
#define TP_PIN_DATA_IN      ARDUINO_PIN_D2
#define TP_PIN_RESAMPLE     0
#define TP_PIN_ENCODE       ARDUINO_PIN_D3
#define TP_PIN_IPC_TX       0
#define TP_PIN_IPC_DROP     0
#endif

#define TP_INIT(_pin)       (TP_PIN_ ## _pin ? \
                             hal_gpio_init_out(TP_PIN_ ## _pin, 0) : (void)0)
#define TP_1(_pin)          (TP_PIN_ ## _pin ? \
                             hal_gpio_write(TP_PIN_ ## _pin, 1) : (void)0)
#define TP_0(_pin)          (TP_PIN_ ## _pin ? \
                             hal_gpio_write(TP_PIN_ ## _pin, 0) : (void)0)

static uint8_t g_usb_enabled;
//static uint32_t g_usb_sample_rate;

static void usb_itf_changed_func(struct os_event *ev);
static void usb_data_func(struct os_event *ev);

static struct os_event usb_itf_changed_ev[2] = {
    { .ev_cb = usb_itf_changed_func, },
    { .ev_cb = usb_itf_changed_func, },
};

static struct os_event usb_data_ev = {
    .ev_cb = usb_data_func,
};

static void
usb_itf_changed_func(struct os_event *ev)
{
    uint8_t alt = POINTER_TO_UINT(ev->ev_arg);

    if (alt == 0) {
//        oble_state_usb_active_set(0);
    } else {
//        oble_state_usb_active_set(1);
    }
}


//static int16_t samples[LC3_FRAMES_PER_DT * AUDIO_CHANNELS_COUNT * 2];
static int16_t samples[48000 / 100 * 2 * 2];
static int samples_idx = 0;

struct chan {
    void *encoder;
    uint16_t handle;
    struct os_mbuf *om;
};

static struct ble_gap_event_listener gap_listener;

static struct chan chans[AUDIO_CHANNELS];

static int
gap_listener_func(struct ble_gap_event *event, void *arg)
{
    const struct ble_hci_ev_le_meta *ev;
    const struct ble_hci_ev_le_subev_create_big_complete *subev;
    int i;

    if ((event->type != BLE_GAP_EVENT_UNHANDLED_HCI_EVENT) ||
        !event->unhandled_hci.is_le_meta) {
        return 0;
    }

    ev = event->unhandled_hci.ev;
    if (ev->subevent != BLE_HCI_LE_SUBEV_CREATE_BIG_COMPLETE) {
        return 0;
    }

    subev = event->unhandled_hci.ev;

    assert(subev->status == 0);
    assert(subev->num_bis <= ARRAY_SIZE(chans));

    for (i = 0; i < subev->num_bis; i++) {
        chans[i].handle = subev->conn_handle[i];
    }

    return 0;
}

static uint32_t pkt_counter = 0;

static void
usb_data_func(struct os_event *ev)
{
    struct os_mbuf *om;
    int ch_idx;
    unsigned int num_bytes;
    unsigned int num_samples;
    unsigned int num_frames;
    unsigned int frames_count;
    int read;
    int skip;
    uint8_t *dptr;

    if (!g_usb_enabled) {
        tud_audio_clear_ep_out_ff();
        return;
    }

    TP_1(DATA_CB);

    while ((num_bytes = tud_audio_available()) > 0) {
        num_samples = num_bytes / AUDIO_SAMPLE_SIZE;
        num_frames = num_samples / AUDIO_CHANNELS;
        num_bytes = num_frames * AUDIO_SAMPLE_SIZE * AUDIO_CHANNELS;

        assert(samples_idx + num_samples < ARRAY_SIZE(samples));

        TP_1(DATA_IN);
        read = tud_audio_read(&samples[samples_idx], num_bytes);
        TP_0(DATA_IN);

        assert(read == num_bytes);
        assert(samples[ARRAY_SIZE(samples) - 1] = 0xaaaa);

        samples_idx += num_samples;
        assert((samples_idx & 0x80000000) == 0);
        frames_count = samples_idx / AUDIO_CHANNELS;

        if (frames_count >= LC3_FPDT) {
            pkt_counter++;
            skip = 0;

            for (ch_idx = 0; ch_idx < BIG_NUM_BIS; ch_idx++) {
                if (chans[ch_idx].handle == 0) {
                    skip = 1;
                    continue;
                }

                if (chans[ch_idx].om == NULL) {
                    om = os_msys_get_pkthdr(8 + g_app_data.big_sdu, 0);
                    if (om == NULL) {
                        skip = 1;
                        break;
                    }
                    chans[ch_idx].om = om;
                }
            }

            if (!skip) {
                om = chans[0].om;
                chans[0].om = NULL;
                put_le16(&om->om_data[0], BLE_HCI_ISO_HANDLE(chans[0].handle, BLE_HCI_ISO_PB_COMPLETE, 0));
                put_le16(&om->om_data[2], g_app_data.big_sdu + 4);
                put_le16(&om->om_data[4], 0);
                put_le16(&om->om_data[6], g_app_data.big_sdu);
                dptr = &om->om_data[8];

                TP_1(ENCODE);
                lc3_encode(chans[0].encoder, LC3_PCM_FORMAT_S16,
                           samples + 0, AUDIO_CHANNELS,
                           (int)g_app_data.lc3_frame_bytes, dptr);
                TP_0(ENCODE);

                if (AUDIO_CHANNELS == 2) {
                    if (BIG_NUM_BIS == 1) {
                        dptr += g_app_data.lc3_frame_bytes;
                    } else {
                        os_mbuf_extend(om, 8 + g_app_data.big_sdu);
                        ble_transport_to_ll_iso(om);

                        om = chans[1].om;
                        chans[1].om = NULL;
                        put_le16(&om->om_data[0], BLE_HCI_ISO_HANDLE(chans[1].handle, BLE_HCI_ISO_PB_COMPLETE, 0));
                        put_le16(&om->om_data[2], g_app_data.big_sdu + 4);
                        put_le16(&om->om_data[4], 0);
                        put_le16(&om->om_data[6], g_app_data.big_sdu);
                        dptr = &om->om_data[8];
                    }

                    TP_1(ENCODE);
                    lc3_encode(chans[1].encoder, LC3_PCM_FORMAT_S16,
                               samples + 1, AUDIO_CHANNELS,
                               (int)g_app_data.lc3_frame_bytes, dptr);
                    TP_0(ENCODE);
                }

                os_mbuf_extend(om, 8 + g_app_data.big_sdu);
                ble_transport_to_ll_iso(om);
            }

            if (frames_count > LC3_FPDT) {
                int old_samples_idx = samples_idx;
                samples_idx -= LC3_FPDT * AUDIO_CHANNELS;
                memmove(samples, &samples[old_samples_idx],
                        (old_samples_idx - samples_idx) * AUDIO_SAMPLE_SIZE);
            } else {
                samples_idx = 0;
            }
        }
    }

    TP_0(DATA_CB);
}

bool
tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    uint8_t alt = tu_u16_low(p_request->wValue);

    (void)rhport;

    usb_itf_changed_ev[alt].ev_arg = (void  *)(uint32_t)alt;
    os_eventq_put(os_eventq_dflt_get(), &usb_itf_changed_ev[alt]);

    return true;
}

bool
tud_audio_rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received,
                               uint8_t func_id, uint8_t ep_out,
                               uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)n_bytes_received;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    TP_1(DATA_ISR);

    if (!usb_data_ev.ev_queued) {
        os_eventq_put(os_eventq_dflt_get(), &usb_data_ev);
    }

    TP_0(DATA_ISR);

    return true;
}

//static void
//sample_rate_func(uint32_t sample_rate)
//{
//    printf("USB sample rate set to %d\n", (int)sample_rate);
//
//    g_usb_sample_rate = sample_rate;
//
////    audio_sample_rate_set(sample_rate);
//}

void
audio_usb_init(void)
{
    /* Need to reference those explicitly, so they are always pulled by linker
     * instead of weak symbols in tinyusb.
     */
    (void)tud_audio_set_itf_cb;
    (void)tud_audio_rx_done_post_read_cb;

    TP_INIT(DATA_ISR);
    TP_INIT(DATA_CB);
    TP_INIT(DATA_IN);
    TP_INIT(RESAMPLE);
    TP_INIT(ENCODE);
    TP_INIT(IPC_TX);
    TP_INIT(IPC_DROP);

    ble_gap_event_listener_register(&gap_listener, gap_listener_func, NULL);

    usb_desc_sample_rate_set(AUDIO_PCM_SAMPLE_RATE);

    assert(LC3_FPDT == lc3_frame_samples(LC3_FRAME_DURATION,
                                         AUDIO_PCM_SAMPLE_RATE));

    memset(samples, 0xaa, sizeof(samples));

    unsigned esize = lc3_encoder_size(LC3_FRAME_DURATION,
                                      AUDIO_PCM_SAMPLE_RATE);
    for (int i = 0; i < AUDIO_CHANNELS; i++) {
        chans[i].encoder = calloc(1, esize);
        lc3_setup_encoder(LC3_FRAME_DURATION, LC3_SAMPLING_FREQ,
                          AUDIO_PCM_SAMPLE_RATE,
                          chans[i].encoder);
    }



//    sample_rate_func(usb_audio_sample_rate_get());
//    usb_audio_sample_rate_cb_set(sample_rate_func);

#ifdef NRF53_SERIES
    nrf_clock_hfclk_div_set(NRF_CLOCK, NRF_CLOCK_HFCLK_DIV_1);
#endif

    g_usb_enabled = 1;
}

void
audio_usb_enable(uint8_t enabled)
{
    g_usb_enabled = enabled;

    if (enabled) {
//        audio_sample_rate_set(g_usb_sample_rate);
    }
}
