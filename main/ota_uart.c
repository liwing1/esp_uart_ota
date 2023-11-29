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

size_t read_proto_msg(char* buf)
{
    uint32_t size_msg = 0;
    fread(&size_msg, sizeof(uint32_t), 1, stdin);

    for(size_t i = 0; i < size_msg; i++) {
       buf[i] = fgetc(stdin);
    }
    return size_msg;
}

const char* TAG = "MAIN";

void send_proto_msg(char* buf, size_t len)
{
    size_t added_size = sizeof(size_t);
    size_t final_len = len + added_size;

    char* tx_buf = calloc(1, final_len);

    memcpy(tx_buf, &len, added_size); // Adiciona o tamanho
    memcpy(tx_buf + added_size, buf, len);

    // ESP_LOG_BUFFER_HEXDUMP(TAG, tx_buf, final_len, ESP_LOG_INFO); //TEM QUE SER LOG INFO PARA STDOUT
    fwrite(tx_buf, final_len, 1, stdout);
    

    free(tx_buf);
}

size_t ota_uart_proto_get_image_size(void)
{
    // Prepara buffer que recebera proto msg
    void* rx_buf = calloc(1, DEFAULT_OTA_BUF_SIZE);

    // Le a mensagem (funcao bloqueante CUIDADO!!)
    size_t rx_len = read_proto_msg(rx_buf);

    // Realiza a deserealizacao
    FirmUpdateStart* rx_msg = firm_update_start__unpack(NULL, rx_len, rx_buf);
    if(rx_msg == NULL) {
        printf("ERRO DESEREAELIZE\n");
    }

    // Libera memoria alocada!
    free(rx_buf);
    firm_update_start__free_unpacked(rx_msg, NULL);

    printf("START OK\n");

    return rx_msg->image_size;
}

void ota_uart_proto_req_bin_chunks(int32_t chunk_size)
{
    FirmPktReq msg = FIRM_PKT_REQ__INIT;
    msg.has_numbytes = 1;
    msg.numbytes = chunk_size;
    msg.has_advanceaddress = 1;
    msg.advanceaddress = 1;

    size_t len = firm_pkt_req__get_packed_size(&msg);
    void *buf = calloc(1, len);
    firm_pkt_req__pack(&msg, buf);

    // ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len, ESP_LOG_INFO); //TEM QUE SER LOG INFO PARA STDOUT
    send_proto_msg(buf, len);

    free(buf);
}

void ota_uart_proto_rcv_bin_chunks(void)
{
    /* Recebe o chunk */
    void *rx_buf = calloc(1, 2*DEFAULT_OTA_BUF_SIZE);

    // Le a mensagem (funcao bloqueante CUIDADO!!)
    size_t rx_len = read_proto_msg(rx_buf);

    // Realiza a deserealizacao
    FirmPktRes* rx_msg = firm_pkt_res__unpack(NULL, rx_len, rx_buf);
    if(rx_msg == NULL) {
        printf("ERRO DESEREAELIZE\n");
    }

    // for(uint32_t i = 0; i < rx_msg->pkt->len; i++) {
    //     printf("chunk: 0x%02X\n", rx_msg->pkt->data[i]);
    // }

    // Libera memoria alocada!
    free(rx_buf);
    firm_pkt_res__free_unpacked(rx_msg, NULL);
}


void app_main() {
    jtag_serial_init();

    // vTaskDelay(pdMS_TO_TICKS(15000));
    ESP_LOGI(TAG, "START");
    ESP_LOG_BUFFER_HEXDUMP(TAG, TAG, strlen(TAG), ESP_LOG_INFO); //TEM QUE SER LOG INFO PARA STDOUT

    while(1) 
    {
        ota_uart_proto_req_bin_chunks(3);

        // ota_uart_proto_rcv_bin_chunks();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // esp_uart_ota_start();
}