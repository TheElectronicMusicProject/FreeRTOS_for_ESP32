#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_err.h>
#include <stdio.h>

static TaskHandle_t gh_task = NULL;

static void
task_loop (void * p_param)
{
    esp_err_t ret = 0;
    int32_t dly = 1000;
    esp_err_t error;

    gh_task = xTaskGetCurrentTaskHandle();

    error = esp_task_wdt_status(gh_task);

    if (ESP_ERR_NOT_FOUND == error)
    {
        error = esp_task_wdt_init(5, true);
        assert(ESP_OK == error);
        error = esp_task_wdt_add(gh_task);
        assert(ESP_OK == error);
        printf("Task is subscribed to TWDT\n");
    }
    else
    {
        printf("Valore ritornato = %d\n", error);
    }

    for (;;)
    {
        printf("loop(dly=%d)..\n", dly);
        ret = esp_task_wdt_status(gh_task);
        assert(ESP_OK == ret);
        vTaskDelay(pdMS_TO_TICKS(dly));
        dly += 1000;
    }
}

void
app_main (void)
{
    BaseType_t ret = 0;

    vTaskDelay(pdMS_TO_TICKS(2000));

    ret = xTaskCreate(task_loop, "loop", 2048, NULL, 1, NULL);
    assert(pdPASS == ret);
}
