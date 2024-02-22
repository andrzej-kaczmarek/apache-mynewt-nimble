#ifndef APP_PRIV_H_
#define APP_PRIV_H_

#include <syscfg/syscfg.h>

struct app_cfg {
    uint16_t lc3_frame_duration;
    uint16_t lc3_sampling_freq;
    uint32_t lc3_bitrate;

    uint8_t big_phy;
    uint8_t big_num_bis;
    uint8_t big_bn;
    uint8_t big_irc;
    uint8_t big_pto;
    uint8_t big_pt;
    uint8_t big_packing;
    uint8_t big_encryption;

    uint16_t gap_appearance;
    char gap_local_name[256];

    uint32_t broadcast_id;
    char broadcast_name[33];
    char broadcast_code[17];
};

struct app_data {
    uint8_t modified;
    uint32_t lc3_afpdt;
    uint32_t lc3_frame_bytes;
    uint8_t big_num_bis;
    uint16_t big_sdu;
    uint32_t broadcast_id;
};

extern struct app_cfg g_app_cfg;
extern struct app_data g_app_data;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define AUDIO_CHANNELS          MYNEWT_VAL(USB_AUDIO_OUT_CHANNELS)
#define AUDIO_SAMPLE_SIZE       sizeof(int16_t)
#define AUDIO_PCM_SAMPLE_RATE   48000

#if MYNEWT_VAL(STATIC_CONFIG)
#define LC3_FRAME_DURATION      (MYNEWT_VAL(LC3_FRAME_DURATION))
#define LC3_SAMPLING_FREQ       (MYNEWT_VAL(LC3_SAMPLING_FREQ))
#define LC3_BITRATE             (MYNEWT_VAL(LC3_BITRATE))
#define LC3_FPDT                (AUDIO_PCM_SAMPLE_RATE * LC3_FRAME_DURATION / 1000000)
#define BIG_NUM_BIS             (MIN(AUDIO_CHANNELS, MYNEWT_VAL(BIG_NUM_BIS)))
#define BIG_BN                  (MYNEWT_VAL(BIG_BN))
#define BIG_IRC                 (MYNEWT_VAL(BIG_IRC))
#define BIG_PTO                 (MYNEWT_VAL(BIG_PTO))
#define BIG_PT                  (MYNEWT_VAL(BIG_PT))
#else
#define LC3_FRAME_DURATION      g_app_cfg.lc3_frame_duration
#define LC3_SAMPLING_FREQ       g_app_cfg.lc3_sampling_freq
#define LC3_BITRATE             g_app_cfg.lc3_bitrate
#define LC3_FPDT                g_app_data.lc3_afpdt
#define BIG_NUM_BIS             g_app_data.big_num_bis
#define BIG_BN                  g_app_cfg.big_bn
#define BIG_IRC                 g_app_cfg.big_irc
#define BIG_PTO                 g_app_cfg.big_pto
#define BIG_PT                  g_app_cfg.big_pt
#endif

int config_set(const char *name, int argc, char **argv);
void config_reset_to_defaults(void);
void config_commit(void);

void usb_desc_sample_rate_set(uint32_t sample_rate);

int big_start(void);

#endif
