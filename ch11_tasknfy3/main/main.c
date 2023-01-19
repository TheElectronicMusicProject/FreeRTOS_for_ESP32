#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED    GPIO_NUM_19

static TaskHandle_t gh_task1 = NULL;

static void
task1 (void * p_param)
{
    uint32_t ret = 0;
    uint32_t level = 0;

    for (;;)
    {
        ret = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        level ^= 1;
        gpio_set_level(GPIO_LED, level);
        fprintf(stderr, "Task notified: rv = %u\n", ret);
    }
}

static void
task2 (void * p_param)
{
    uint32_t count = 0;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(500 + count));
        xTaskNotifyGive(gh_task1);
        count += 100;
    }
}

static void
task_loop (void * p_param)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        xTaskNotifyGive(gh_task1);
    }
}

void
app_main (void)
{
    int32_t app_cpu = 0;
    BaseType_t ret = 0;

    app_cpu = xPortGetCoreID();
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED, 0);

    vTaskDelay(pdMS_TO_TICKS(2000));

    ret = xTaskCreatePinnedToCore(task1, "task1", 3000, NULL, 1, &gh_task1, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task2, "task2", 3000, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_loop, "loop", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);
}
