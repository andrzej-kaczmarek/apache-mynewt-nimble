#ifndef H_USB_AUDIO_
#define H_USB_AUDIO_

#include <stdint.h>

typedef void (* usb_audio_sample_rate_cb_t)(uint32_t);

/* Set default sample rate, should only be used before USB is initialized */
void usb_desc_sample_rate_set(uint32_t sample_rate);

/* Set callback to receive sample rate set by USB host */
void usb_audio_sample_rate_cb_set(usb_audio_sample_rate_cb_t cb);

/* Get current sample rate */
uint32_t usb_audio_sample_rate_get(void);

#endif /* H_USB_AUDIO_ */
