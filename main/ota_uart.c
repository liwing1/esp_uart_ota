#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <fcntl.h>

#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"

#define IMAGE_HEADER_SIZE sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1
#define DEFAULT_OTA_BUF_SIZE IMAGE_HEADER_SIZE
#define DEFAULT_REQUEST_SIZE (64 * 1024)
#define IMAGE_LEN  194768

#define ESP_ERR_UART_OTA_BASE            (0x9000)
#define ESP_ERR_UART_OTA_IN_PROGRESS     (ESP_ERR_UART_OTA_BASE + 1)  /* OTA operation in progress */
#define ESP_ERR_UART_OTA_IMG_DOWNLOADED  (ESP_ERR_UART_OTA_BASE + 2)  /* OTA finish donwloaded */

#define JTAG_SERIAL_BUFF_SIZE 1024
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


void jtag_serial_init(void)
{
    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable non-blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

    usb_serial_jtag_driver_config_t usb_serial_jtag_config;
    usb_serial_jtag_config.rx_buffer_size = JTAG_SERIAL_BUFF_SIZE;
    usb_serial_jtag_config.tx_buffer_size = JTAG_SERIAL_BUFF_SIZE;

    esp_err_t ret = ESP_OK;
    /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK) {
        return;
    }

    /* Tell vfs to use usb-serial-jtag driver */
    esp_vfs_usb_serial_jtag_use_driver();
}


static esp_err_t _ota_write(esp_ota_uart_config_t *uart_ota_handle, const void *buffer, size_t buf_len)
{
    if (buffer == NULL || uart_ota_handle == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_ota_write(uart_ota_handle->update_handle, buffer, buf_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
    } else {
        uart_ota_handle->binary_file_len += buf_len;
        ESP_LOGD(TAG, "Written image length %d", uart_ota_handle->binary_file_len);

        if(uart_ota_handle->binary_file_len < DEFAULT_OTA_BUF_SIZE){
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
    config->image_length = IMAGE_LEN; //194768
    config->max_uart_request_size = DEFAULT_REQUEST_SIZE;
    config->state = ESP_UART_OTA_BEGIN;

    // ESP_LOGW(TAG, "Got partition: %s", config->update_partition->label);
    // ESP_LOGW(TAG, "Got upgrade buf size: %d", config->ota_upgrade_buf_size);
    // ESP_LOGW(TAG, "Got binary file len: %d", config->binary_file_len);
    // ESP_LOGW(TAG, "Got image len: %d", config->image_length);
    // ESP_LOGW(TAG, "Got max request size: %d", config->max_uart_request_size);
    // ESP_LOGW(TAG, "Got bulk flash erase: %d", config->bulk_flash_erase);
    // ESP_LOGW(TAG, "Got partial: %d", config->partial_uart_download);

    const int erase_size = config->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
    err = esp_ota_begin(config->update_partition, erase_size, &config->update_handle);

failure:
    return err;
}


esp_err_t esp_uart_ota_perform(esp_ota_uart_config_t *handle)
{
    int bin_read_size = MIN(DEFAULT_OTA_BUF_SIZE, IMAGE_LEN - handle->binary_file_len);

    printf("request: %d\n", bin_read_size);

    memset(handle->ota_upgrade_buf, 0, DEFAULT_OTA_BUF_SIZE);
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

    return err;
}

void ota_task(void* p)
{
    esp_ota_uart_config_t cfg = {0};

    esp_err_t ret = esp_uart_ota(&cfg);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    jtag_serial_init();

    char ota_buffer[DEFAULT_OTA_BUF_SIZE] = {0};
    size_t downloaded = 0;

    vTaskDelay(pdMS_TO_TICKS(15000));

    xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);

    // while(1)
    // {
    //     printf("request: %d\n", DEFAULT_OTA_BUF_SIZE);
    //     fread(ota_buffer, MIN(DEFAULT_OTA_BUF_SIZE, IMAGE_LEN - downloaded), 1, stdin);

    //     // ESP_LOG_BUFFER_HEXDUMP(TAG, ota_buffer, DEFAULT_OTA_BUF_SIZE, ESP_LOG_WARN);
    //     downloaded += DEFAULT_OTA_BUF_SIZE;


    //     if(downloaded >= IMAGE_LEN){
    //         ESP_LOG_BUFFER_HEXDUMP(TAG, ota_buffer, DEFAULT_OTA_BUF_SIZE, ESP_LOG_WARN);
    //         break;
    //     }

    //     memset(ota_buffer, 0, DEFAULT_OTA_BUF_SIZE);
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

}