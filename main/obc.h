#ifndef OBC_CORE
#define OBC_CORE

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_event.h"

// OBC specific global structs
 typedef struct ride_params_t {
    bool moving;
    uint32_t rotations;
    portTickType prevRotationTickCount;
    portTickType rotationTickCount;
    uint16_t msBetweenRotationTicks;
    uint32_t totalRideTimeMs;
    float speed;
    float avgSpeed;
    float distance;
    float maxSpeed;
    float totalDistance;
 } ride_params_t;

extern ride_params_t rideParams;

// obc event loop
ESP_EVENT_DECLARE_BASE(OBC_EVENTS);
extern esp_event_loop_handle_t obc_events_loop;

// obc events

enum {
    RIDE_START_EVENT,                    // raised after ride start
    RIDE_STOP_EVENT,                     // raised when not moving state is detected
    SYNC_START_EVENT,                    // sync with obc_server started
    SYNC_STOP_EVENT                      //  sync with obc_server finished with success or failure
};

//shared queues
extern xQueueHandle reed_evt_queue;

//shared tasks
extern TaskHandle_t screenRefreshTaskHandle;
extern TaskHandle_t spiffsSyncOnStopTaskHandle;

#endif
