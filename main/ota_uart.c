#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <fcntl.h>
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"

#include "esp_uart_ota.h"

#define JTAG_SERIAL_BUFF_SIZE 1024

void jtag_serial_init(void)
{
    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_LF);

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

uint32_t wtd = 60;

void task_wtd(void* p)
{
    while(1) {
        if(wtd-- == 0) esp_restart();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main() {
    jtag_serial_init();

    xTaskCreate(task_wtd, "task_wtd", 2048, NULL, 2, NULL);
    esp_uart_ota_start();

    while(1) 
    {
        wtd = 15;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}