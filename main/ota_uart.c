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

#include "proto-ota.pb-c.h"
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

void app_main() {
    jtag_serial_init();

    // vTaskDelay(pdMS_TO_TICKS(15000));

    FirmPktReq msg = FIRM_PKT_REQ__INIT;
    void* buf;
    unsigned len;

    msg.has_numbytes = 1;
    msg.numbytes = 10;
    msg.has_advanceaddress = 1;
    msg.advanceaddress = 1;
    len = firm_pkt_req__get_packed_size(&msg);
    buf = malloc(len);
    firm_pkt_req__pack(&msg, buf);
    ESP_LOGI("MAIN", "Writing %d serialized bytes",len); // See the length of message
    ESP_LOG_BUFFER_HEXDUMP("MAIN", buf, len, ESP_LOG_INFO); //TTEM QUE SER LOG INFO PARA STDOUT

    while(1)
    {
        fwrite(buf, len, 1, stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    free(buf);

    // esp_uart_ota_start();
}