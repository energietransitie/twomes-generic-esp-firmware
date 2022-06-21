#include <generic_esp_32.h>

#ifdef CONFIG_TWOMES_OTA_FIRMWARE_UPDATE
#include <c_ota.h>
#endif

#ifdef CONFIG_TWOMES_PRESENCE_DETECTION
#define DEVICE_TYPE_NAME "Presence-Detector"
static const char *TAG = "Twomes ESP32 presence detector";
#define DEVICE_TYPE_OTA_ORG "energietransitie"
#define DEVICE_TYPE_OTA_REPO "twomes-generic-esp-firmware"
#else
#define DEVICE_TYPE_NAME "CO2-meter-SCD4x"
#define DEVICE_TYPE_OTA_ORG "energietransitie"
#define DEVICE_TYPE_OTA_REPO "twomes-generic-esp-firmware"
static const char *TAG = "Twomes ESP32 generic test device";
#endif

#if defined ESP32DEV
#define DEVICE_TYPE_OTA_FILENAME "firmware-signed_ESP32DEV.bin"
#elif defined M5STACK_COREINK
#define DEVICE_TYPE_OTA_FILENAME "firmware-signed_M5STACK_COREINK.bin"
#endif

#define BOOT_STARTUP_INTERVAL_MS (10 * 1000) // milliseconds ( 10 s * 1000 ms/s)
#define BOOT_STARTUP_INTERVAL_TXT "Wating 10 seconds before next measurement data series is started"

#if defined ESP32DEV
void app_main(void)
{
    twomes_device_provisioning(DEVICE_TYPE_NAME);
    // TODO: move tasks to new twomes_device_initialization() function in generic firmware library

    // Check if this (re)boot booted new firmware.
    twomes_ota_check_update_finished_successfully();

    ESP_LOGD(TAG, "Starting heartbeat task");
    xTaskCreatePinnedToCore(&heartbeat_task, "heartbeat_task", 4096, NULL, 1, NULL, 1);

    ESP_LOGD(TAG, BOOT_STARTUP_INTERVAL_TXT);
    vTaskDelay(BOOT_STARTUP_INTERVAL_MS / portTICK_PERIOD_MS);

    ESP_LOGD(TAG, "Starting timesync task");
    xTaskCreatePinnedToCore(&timesync_task, "timesync_task", 4096, NULL, 1, NULL, 1);

    ESP_LOGD(TAG, BOOT_STARTUP_INTERVAL_TXT);
    vTaskDelay(BOOT_STARTUP_INTERVAL_MS / portTICK_PERIOD_MS);

#ifdef CONFIG_TWOMES_OTA_FIRMWARE_UPDATE
    twomes_ota_set_location(DEVICE_TYPE_OTA_ORG, DEVICE_TYPE_OTA_REPO, DEVICE_TYPE_OTA_FILENAME);
    twomes_ota_start();
#endif // CONFIG_TWOMES_OTA_FIRMWARE_UPDATE

#ifdef CONFIG_TWOMES_PRESENCE_DETECTION
    ESP_LOGD(TAG, "Starting presence detection");
    start_presence_detection();
#endif

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS); //
    }
}
#elif defined M5STACK_COREINK

#include <scheduler.h>
#include <rtc.h>
#include <scheduled_tasks.h>
#include <powerpin.h>
#include <upload.h>

// schedule configuration
const interval_t min_tasks_interval_s = SCHEDULER_INTERVAL_2M;
scheduler_t schedule[] = {
    {heartbeatv2_task, "heartbeat", 4096, {0, NULL}, 1, SCHEDULER_INTERVAL_2M},
    {twomes_scd41_task, "twomes scd41", 4096, {0, NULL}, 1, SCHEDULER_INTERVAL_4M},
#ifdef CONFIG_TWOMES_OTA_FIRMWARE_UPDATE
    {twomes_ota_firmware_update_task, "firmware update", 16384, {0, NULL}, 1, SCHEDULER_INTERVAL_6M},
#endif
    {upload_task, "upload_task", 4096, {0, NULL}, 1, min_tasks_interval_s},
};
int schedule_size = sizeof(schedule)/sizeof(scheduler_t);

void app_main(void)
{
    powerpin_set();

    ESP_LOGD(TAG, "Target is M5Stack_CoreINK");

    // twomes device provisioning
    twomes_device_provisioning(DEVICE_TYPE_NAME);

#ifdef CONFIG_TWOMES_OTA_FIRMWARE_UPDATE
    twomes_ota_set_location(DEVICE_TYPE_OTA_ORG, DEVICE_TYPE_OTA_REPO, DEVICE_TYPE_OTA_FILENAME);
    // Check if this (re)boot booted new firmware.
    twomes_ota_check_update_finished_successfully();
#endif

    // initailize 
    rtc_initialize();
    rtc_syncronize_rtc_time();
    upload_initialize();
    scheduler_initialize(schedule,schedule_size,min_tasks_interval_s);

    while (1)
    {
        // start tasks that are due
        scheduler_execute_tasks(false);

        // wait for the end of all running tasks and then sleep
        scheduler_wait();

        // update
        scheduler_update();
    }
}
#endif
