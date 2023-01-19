#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdio.h>

static void
task1 (void * p_arg)
{
    fprintf(stderr, "task1 priority is %u\n", (uint32_t) uxTaskPriorityGet(NULL));
    vTaskDelete(NULL);
}

void
app_main (void)
{
    BaseType_t ret = 0;
    TaskHandle_t h_task = NULL;
    int32_t app_cpu = 0;

    app_cpu = xPortGetCoreID();
    vTaskDelay(pdMS_TO_TICKS(2000));

    fprintf(stderr, "looptask priority is %u\n", (uint32_t) uxTaskPriorityGet(NULL));

    ret = xTaskCreatePinnedToCore(task1, "task 1", 2048, NULL, 0, &h_task, app_cpu);
    assert(pdPASS == ret);

    fprintf(stderr, "task1 created\n");

    vTaskSuspend(h_task);
    vTaskPrioritySet(h_task, 3);

    vTaskDelay(pdMS_TO_TICKS(3000));
    fprintf(stderr, "Zzzz... for 3 seconds\n");

    vTaskResume(h_task);
}
