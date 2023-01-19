#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdint.h>
#include "driver/gpio.h"

#define GPIO_LED    15
#define GPIO_BUTTON 21

static QueueHandle_t queue = NULL;

static void
task_debounce (void * argp)
{
    uint32_t level = 0;
    uint32_t state = 0;
    uint32_t last = 0xFFFFFFFF;
    uint32_t mask = 0x7FFFFFFF;
    bool event = false;

    for (;;)
    {
        level = gpio_get_level(GPIO_BUTTON);
        state = (state << 1) | level;

        if (((state & mask) == mask) || ((state & mask) == 0))
        {
            if (level != last)
            {
                event = (bool) level;

                if (pdPASS == xQueueSendToBack(queue, &event, 1))
                {
                    last = level;
                }
            }
        }

        taskYIELD();
    }
}

static void
task_led (void * argp)
{
    BaseType_t st;
    bool event;
    bool led = false;

    (void) gpio_set_level(GPIO_LED, led);

    for (;;)
    {
        st = xQueueReceive(queue, &event, portMAX_DELAY);
        assert(pdPASS == st);

        if (true == event)
        {
            led ^= true;
            gpio_set_level(GPIO_LED, led);
        }
    }
}

void
app_main (void)
{
    int app_cpu = 1;
    TaskHandle_t h_task = NULL;
    BaseType_t rc = 0;

    vTaskDelay(pdMS_TO_TICKS(2000));

    queue = xQueueCreate(40, sizeof(bool));
    assert(queue);
    
    gpio_pad_select_gpio(GPIO_LED);
    (void) gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(GPIO_BUTTON);
    (void) gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    (void) gpio_set_pull_mode(GPIO_BUTTON, GPIO_PULLUP_ONLY);

    rc = xTaskCreatePinnedToCore(task_debounce, "debounce", 2048, NULL, 1, &h_task, app_cpu);
    assert(pdPASS == rc);
    assert(h_task);

    rc = xTaskCreatePinnedToCore(task_led, "led", 2048, NULL, 1, &h_task, app_cpu);
    assert(pdPASS == rc);
    assert(h_task);
}
