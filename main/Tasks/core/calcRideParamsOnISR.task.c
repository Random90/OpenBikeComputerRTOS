
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "calcRideParamsOnISR.task.h"
#include "settings.h"
#include "obc.h"

#define TAG "RIDE_CALC"

bool ignoreReed = false;

// handle server sychronization start event and set flags to block data
// during synchronization, calculations should be blocked
static void vOnSyncStartHandle(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ignoreReed = true;
}

// handle server sychronization start event and set flags to block data
// if synchronization finishes with success, finish the ride, by resseting rideParams, otherwise continue
static void vOnSyncFinishHandle(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {

    bool reset = *((bool*) event_data);

    if (reset){
        rideParams.rotations = 0;
        rideParams.msBetweenRotationTicks = 0;
        rideParams.totalRideTimeMs = 0;
        rideParams.speed = 0.0;
        rideParams.distance = 0.0;
        rideParams.avgSpeed = 0.0;
        rideParams.prevRotationTickCount = 0.0;
    } 

    ignoreReed = false;
}

void vCalcRideParamsOnISRTask(void* data)
{
    ESP_LOGI(TAG, "Init reed switch");
    
    ESP_ERROR_CHECK(esp_event_handler_register_with(obc_events_loop, OBC_EVENTS, SYNC_START_EVENT, vOnSyncStartHandle, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(obc_events_loop, OBC_EVENTS, SYNC_STOP_EVENT, vOnSyncFinishHandle, NULL));

    for(;;) {
        if(xQueueReceive(reed_evt_queue, &rideParams.rotationTickCount, portMAX_DELAY) && !ignoreReed) {
            // TODO create buffer for reed time impulses before calculating time and speed
            // TODO block rideParams while calculating?
            if (rideParams.prevRotationTickCount != 0) {
                if (!rideParams.moving) {
                    esp_event_post_to(obc_events_loop, OBC_EVENTS, RIDE_START_EVENT, NULL, 0, portMAX_DELAY);
                }
                rideParams.moving = true;
                rideParams.rotations++;
                rideParams.msBetweenRotationTicks = ((int) rideParams.rotationTickCount - (int) rideParams.prevRotationTickCount) * (int) portTICK_RATE_MS;
                rideParams.totalRideTimeMs += rideParams.msBetweenRotationTicks;
                // TODO sometimes tick count doesn't update even with queue? prevent speed = inf for now
                if (rideParams.msBetweenRotationTicks > 0.00) {
                    rideParams.speed = ( (float) CIRCUMFERENCE/1000000 ) / ( (float) rideParams.msBetweenRotationTicks / 3600000 ); //km/h
                }
                if (rideParams.speed > rideParams.maxSpeed) {
                    rideParams.maxSpeed = rideParams.speed;
                }
                rideParams.distance += (float)CIRCUMFERENCE/1000000;
                rideParams.totalDistance += (float)CIRCUMFERENCE/1000000;
                rideParams.avgSpeed = rideParams.distance / ( (float) rideParams.totalRideTimeMs / 3600000 );
            }
            rideParams.prevRotationTickCount = rideParams.rotationTickCount;    

            // ESP_LOGI(TAG, "[REED] count: %d, speed: %0.2f, diff: %d, distance: %0.2f", rideParams.rotations, rideParams.speed, rideParams.msBetweenRotationTicks, rideParams.distance);
            // ESP_LOGI(TAG, "[AVGS] speed: %0.2f", rideParams.avgSpeed);
            // inform screen refresh task about movement
            xTaskNotifyGive(screenRefreshTaskHandle);
        }
    }
}