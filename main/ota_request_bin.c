#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TaskHandle_t req_handle = NULL;
TaskHandle_t mon_handle = NULL;

typedef struct
{
    char* buffer;
    size_t buffer_size;
    bool is_req_answered;
} ota_request_t;

void fgets_task(void* p)
{
    ota_request_t* req = ((ota_request_t*) p);
    while(1)
    {
        // fread(req->buffer, req->buffer_size, 1, stdin);
        fgets(req->buffer, req->buffer_size, stdin);
        req->is_req_answered = true;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

esp_err_t ota_request_bin(char* ota_buffer, size_t ota_buffer_size, int timeout_ms)
{
    esp_err_t err = ESP_OK;
    int time = 0;

    ota_request_t req = {
        .buffer = ota_buffer,
        .buffer_size = ota_buffer_size,
        .is_req_answered = false
    };
    
    if(req_handle == NULL) {
        xTaskCreatePinnedToCore(fgets_task, "fgets_task", 4096, (void*)&req, 2, req_handle, APP_CPU_NUM);
    }

    while (req.is_req_answered != true)
    {
        time += 100;
        if(time >= timeout_ms){
            err = ESP_ERR_TIMEOUT;
            vTaskDelete(req_handle);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return err;
}