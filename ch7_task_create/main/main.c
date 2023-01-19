#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdio.h>

static int32_t app_cpu = 0;

static void
task2 (void * p_arg)
{
    fprintf(stderr, "Task 2 priority is %u\n", (uint32_t) uxTaskPriorityGet(NULL));

    vTaskDelete(NULL);
}

static void
task1 (void * p_arg)
{
    BaseType_t ret = 0;
    TaskHandle_t h_task;

    fprintf(stderr, "Task 1 priority is %u\n", (uint32_t) uxTaskPriorityGet(NULL));
    ret = xTaskCreatePinnedToCore(task2, "task2", 2048, NULL, 4, &h_task, app_cpu);
    assert(pdPASS == ret);
    fprintf(stderr, "Task 2 created\n");

    vTaskDelete(NULL);
}

void app_main(void)
{
    BaseType_t ret = 0;
    uint32_t priority = 0;
    TaskHandle_t h_task;

    app_cpu = xPortGetCoreID();
    vTaskDelay(pdMS_TO_TICKS(2000));

    vTaskPrioritySet(NULL, 3);
    priority = uxTaskPriorityGet(NULL);
    assert(3 == priority);

    fprintf(stdout, "Looptask priority is %u\n", priority);
    ret = xTaskCreatePinnedToCore(task1, "task1", 2048, NULL, 2, &h_task, app_cpu);
    assert(pdPASS == ret);
    vTaskDelay(pdMS_TO_TICKS(1000));
    fprintf(stderr, "Task 1 created\n");

    vTaskPrioritySet(h_task, 3);
}
