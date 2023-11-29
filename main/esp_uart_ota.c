#include "esp_uart_ota.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


#define ESP_ERR_UART_OTA_BASE            (0x9000)
#define ESP_ERR_UART_OTA_IN_PROGRESS     (ESP_ERR_UART_OTA_BASE + 1)  /* OTA operation in progress */
#define ESP_ERR_UART_OTA_IMG_DOWNLOADED  (ESP_ERR_UART_OTA_BASE + 2)  /* OTA finish donwloaded */

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

const char* TAG = "OTA_UART";

typedef enum {
    ESP_UART_OTA_INIT,
    ESP_UART_OTA_BEGIN,
    ESP_UART_OTA_IN_PROGRESS,
    ESP_UART_OTA_SUCCESS,
} esp_uart_ota_state;

typedef struct {
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition;
    // esp_http_client_handle_t http_client;
    char *ota_upgrade_buf;
    size_t ota_upgrade_buf_size;
    int binary_file_len;
    int image_length;
    int max_uart_request_size;
    esp_uart_ota_state state;
    bool bulk_flash_erase;
    bool partial_uart_download;
} esp_ota_uart_config_t;




static esp_err_t _ota_write(esp_ota_uart_config_t *uart_ota_handle, const void *buffer, size_t buf_len)
{
    static size_t num_ota_writes = 0;
    static size_t total_len = 0;

    if (buffer == NULL || uart_ota_handle == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_ota_write(uart_ota_handle->update_handle, buffer, buf_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
    } else {
        uart_ota_handle->binary_file_len += buf_len;
        ESP_LOGD(TAG, "Written image length %d", uart_ota_handle->binary_file_len);
        ESP_LOGW(TAG, "num_ota_writes: %d, total_len: %d", num_ota_writes++, total_len+=buf_len);

        if(num_ota_writes >= 673) {
            ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, buf_len, ESP_LOG_WARN);
        }

        if(buf_len < DEFAULT_OTA_BUF_SIZE){
            err = ESP_OK;
        } else {
            err = ESP_ERR_UART_OTA_IN_PROGRESS;
        }
    }
    return err;
}


esp_err_t esp_uart_ota_begin(esp_ota_uart_config_t *config)
{
    esp_err_t err = ESP_FAIL;

    // https_ota_handle->image_length = esp_http_client_get_content_length(https_ota_handle->http_client);

    config->update_partition = esp_ota_get_next_update_partition(NULL);
    if (config->update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        err = ESP_FAIL;
        goto failure;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
        config->update_partition->subtype, config->update_partition->address);

    const int alloc_size = DEFAULT_OTA_BUF_SIZE;
    config->ota_upgrade_buf = (char *)malloc(alloc_size);
    if (!config->ota_upgrade_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");
        err = ESP_ERR_NO_MEM;
        goto failure;
    }

    config->ota_upgrade_buf_size = alloc_size;
    config->image_length = 194768; //194768
    config->max_uart_request_size = DEFAULT_REQUEST_SIZE;
    config->state = ESP_UART_OTA_BEGIN;

    const int erase_size = config->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
    err = esp_ota_begin(config->update_partition, erase_size, &config->update_handle);

failure:
    return err;
}


esp_err_t esp_uart_ota_perform(esp_ota_uart_config_t *handle)
{
    int bin_read_size = MIN(DEFAULT_OTA_BUF_SIZE, handle->image_length - handle->binary_file_len);

    memset(handle->ota_upgrade_buf, 0, DEFAULT_OTA_BUF_SIZE);

    printf("request: %d\n", bin_read_size);
    fread(handle->ota_upgrade_buf, bin_read_size, 1, stdin);
    // ESP_LOG_BUFFER_HEXDUMP(TAG, ota_buffer, DEFAULT_OTA_BUF_SIZE, ESP_LOG_WARN);

    return _ota_write(handle, (const void *)handle->ota_upgrade_buf, bin_read_size);
}


esp_err_t esp_uart_ota(esp_ota_uart_config_t *config)
{
    esp_err_t err = esp_uart_ota_begin(config);

    while (1) {
        err = esp_uart_ota_perform(config);
        if (err != ESP_ERR_UART_OTA_IN_PROGRESS) {
            ESP_LOGI(TAG, "finish download\n");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    err = esp_ota_end(config->update_handle);
    if(err == ESP_OK) {
        err = esp_ota_set_boot_partition(config->update_partition);
    }

    return err;
}

void ota_task(void* p)
{
    esp_ota_uart_config_t cfg = {0};

    esp_err_t ret = esp_uart_ota(&cfg);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "esp_uart_ota -> OK");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }
}

void esp_uart_ota_start(void)
{
    xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
}