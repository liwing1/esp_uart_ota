#ifndef __ESP_UART_OTA_H__
#define __ESP_UART_OTA_H__

#ifdef __cplusplus
extern "C"{
#endif

#include <esp_err.h>
#include "esp_ota_ops.h"


#define IMAGE_HEADER_SIZE sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1
#define DEFAULT_OTA_BUF_SIZE IMAGE_HEADER_SIZE
#define DEFAULT_REQUEST_SIZE (64 * 1024)

void esp_uart_ota_start(void);

#ifdef __cplusplus
}
#endif

#endif //__ESP_UART_OTA_H__