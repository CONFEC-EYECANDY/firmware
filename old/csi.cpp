#include <cstdio>
#include <cstring>

#include "esp_timer.h"
#include "esp_wifi.h"
#include "driver/uart.h"

#include "../main/config.h"
#include "csi.h"

static char s_line[CSI_LINE_MAX];

static void csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    if (info == NULL || info->buf == NULL || info->len <= 0) return;

    const wifi_pkt_rx_ctrl_t *rx = &info->rx_ctrl;
    int64_t ts = esp_timer_get_time();

    int n = snprintf(s_line, sizeof(s_line),
                     "CSI,%lld,%d,%d,%d",
                     (long long)ts, rx->rssi, rx->channel, info->len);
    if (n < 0 || n >= (int)sizeof(s_line)) return;

    for (int i = 0; i < info->len && n < (int)sizeof(s_line) - 4; i++) {
        n += snprintf(s_line + n, sizeof(s_line) - n, ",%d", info->buf[i]);
    }
    s_line[n++] = '\n';

    uart_write_bytes(CSI_UART, s_line, n);
}

void csi_init(void)
{
    wifi_csi_config_t config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = false,
        .manu_scale        = false,
        .shift             = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
}