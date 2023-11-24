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

#define JTAG_SERIAL_BUFF_SIZE 1024
#define OTA_START_CMD 'U'
#define OTA_STOP_CMD  'S'
#define OTA_DATA_CMD  'D'

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



void print_buffer(char *buffer, size_t buffer_size)
{
    printf("BUFFER DATA (%d): \n", buffer_size);
    for(size_t i = 0; i < buffer_size; i++)
    {
        printf("|0x%02X|", buffer[i]);
    }
    printf("END BUFFER DATA\n");
}

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
        err = ESP_ERR_UART_OTA_IN_PROGRESS;
    }
    return err;
}


esp_err_t esp_uart_ota_begin(esp_ota_uart_config_t *config)
{
    esp_err_t err = ESP_FAIL;

    config->update_partition = esp_ota_get_next_update_partition(NULL);
    if (config->update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        err = ESP_FAIL;
        goto failure;
    }
    config->ota_upgrade_buf_size = DEFAULT_OTA_BUF_SIZE;
    config->image_length = IMAGE_LEN; //194768
    config->max_uart_request_size = DEFAULT_REQUEST_SIZE;
    config->state = ESP_UART_OTA_BEGIN;

    ESP_LOGW(TAG, "Got partition: %s", config->update_partition->label);
    ESP_LOGW(TAG, "Got upgrade buf size: %d", config->ota_upgrade_buf_size);
    ESP_LOGW(TAG, "Got binary file len: %d", config->binary_file_len);
    ESP_LOGW(TAG, "Got image len: %d", config->image_length);
    ESP_LOGW(TAG, "Got max request size: %d", config->max_uart_request_size);
    ESP_LOGW(TAG, "Got bulk flash erase: %d", config->bulk_flash_erase);
    ESP_LOGW(TAG, "Got partial: %d", config->partial_uart_download);

failure:
    return err;
}


esp_err_t esp_uart_ota_perform(esp_ota_uart_config_t *handle)
{
    if(handle == NULL) {
        ESP_LOGE(TAG, "esp_uart_ota_perform: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    int data_read;
    const int erase_size = handle->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
    switch (handle->state)
    {
    case ESP_UART_OTA_BEGIN:
            err = esp_ota_begin(handle->update_partition, erase_size, &handle->update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                return err;
            }
            handle->state = ESP_UART_OTA_IN_PROGRESS;
            /* In case `esp_https_ota_read_img_desc` was invoked first,
               then the image data read there should be written to OTA partition
               */
            int binary_file_len = 0;
            if (handle->binary_file_len) {
                /*
                 * Header length gets added to handle->binary_file_len in _ota_write
                 * Clear handle->binary_file_len to avoid additional 289 bytes in binary_file_len
                 */
                binary_file_len = handle->binary_file_len;
                handle->binary_file_len = 0;
            } else {
                if (read_header(handle) != ESP_OK) {
                    return ESP_FAIL;
                }
                binary_file_len = IMAGE_HEADER_SIZE;
            }
            err = esp_ota_verify_chip_id(handle->ota_upgrade_buf);
            if (err != ESP_OK) {
                return err;
            }
            return _ota_write(handle, (const void *)handle->ota_upgrade_buf, binary_file_len);
        break;
    
    default:
        break;
    }
}


esp_err_t esp_uart_ota(esp_ota_uart_config_t *config)
{
    esp_err_t err = esp_uart_ota_begin(config);

    while (1) {
        err = esp_uart_ota_perform(config);
        if (err != ESP_ERR_UART_OTA_IN_PROGRESS) {
            break;
        }
    }    

    return err;
}

void app_main() {
    jtag_serial_init();

    esp_ota_uart_config_t cfg = {0};
    esp_uart_ota(&cfg);

    while(1)
    {
        ESP_LOGI(TAG, "running ...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}