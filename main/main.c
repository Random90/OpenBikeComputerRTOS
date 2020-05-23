#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "settings.h"
#include "obc.h"

#include "utils/math.h"

#include "Tasks/core/blinker.task.h"
#include "Tasks/core/rideStatusWatchdog.task.h"
#include "Tasks/core/calcRideParamsOnISR.task.h"
#include "Tasks/screen_pcd8544/screen_pcd8544.h"
#include "Tasks/storage/spiffs_main.h"

#define TAG "OBC_MAIN"

// declare global queues 
xQueueHandle reed_evt_queue = NULL;

// declare OBC global ride params
ride_params_t rideParams = {
    .moving = false, 
    .rotations = 0,
    .prevRotationTickCount = 0,
    .totalRideTimeMs = 0,
    .speed = 0.0,
    .avgSpeed = 0.0,
    .distance = 0.0,
    .maxSpeed = 0.0,
    .totalDistance = 0.0
};

// Handles for the shared tasks create by init.
TaskHandle_t screenRefreshTaskHandle = NULL;
TaskHandle_t spiffsSyncOnStopTaskHandle = NULL;

//IRAM_ATTR - function with this will be moved to RAM in order to execute faster than default from flash
static void IRAM_ATTR vReedISR(void* arg) {
    portTickType xLastReedTickCount = xTaskGetTickCount();
    xQueueSendFromISR(reed_evt_queue, &xLastReedTickCount, NULL);
}

void vInitTasks() {
    xTaskCreate(&vCalcRideParamsOnISRTask, "vCalcRideParamsOnISRTask", 2048, NULL, 6, NULL);  
    xTaskCreate(&vBlinkerTask, "vBlinkerTask", 2048, NULL, 5, NULL);
    xTaskCreate(&vRideStatusWatchdogTask, "vRideStatusIntervalCheckTask", 2048, NULL, 3, NULL);
    xTaskCreate(&vSpiffsSyncOnStopTask, "vSpiffsSyncOnStopTask", 2048, NULL, 3, &spiffsSyncOnStopTaskHandle);
    xTaskCreate(&vScreenRefreshTask, "vScreenRefreshTask", 2048, NULL, 2, &screenRefreshTaskHandle);
}

void vAttachInterrupts() {
    ESP_LOGI(TAG, "Attaching reed switch interrupt");
    // create queue for the reed interrupt
    reed_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // configure reed switch gpio
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL<<REED_IO_NUM;
    io_conf.pull_up_en = 1;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_intr_type(REED_IO_NUM, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(REED_IO_NUM, vReedISR, (void*) REED_IO_NUM);
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing");
    vInitSpiffs();
    vInitPcd8544Screen();
    vAttachInterrupts();
    vInitTasks();
    ESP_LOGI(TAG, "Startup complete");
}
