#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdint.h>
#include "driver/gpio.h"

#define GPIO_LED        15
#define GPIO_BUTTONL    21
#define GPIO_BUTTONR    19

static QueueHandle_t queue = NULL;
static const int reset_press = -998;

static void
task_debounce (void * argp)
{
    uint32_t button_gpio = *((gpio_num_t *) argp);
    uint32_t level = 0;
    uint32_t state = 0;
    uint32_t mask = 0x7FFFFFFF;
    int32_t event;
    int32_t last = -999;

    for (;;)
    {
        level = !gpio_get_level(button_gpio);
        state = (state << 1) | level;

        if ((state & mask) == mask)
        {
            event = button_gpio;
        }
        else
        {
            event = -button_gpio;
        }

        if (event != last)
        {
            if (pdPASS == xQueueSendToBack(queue, &event, 0))
            {
                last = event;
            }
            else if (event < 0)
            {
                do
                {
                    xQueueReset(queue);
                } while (xQueueSendToBack(queue, &reset_press, 0) != pdPASS);

                last = event;
            }
        }

        taskYIELD();
    }
}

static void
task_led (void * argp)
{
    static const uint32_t enable = (1 << GPIO_BUTTONL) | (1 << GPIO_BUTTONR);
    BaseType_t st;
    int32_t event;
    uint32_t state = 0;

    (void) gpio_set_level(GPIO_LED, 0);

    for (;;)
    {
        st = xQueueReceive(queue, &event, portMAX_DELAY);
        assert(pdPASS == st);

        if (reset_press == event)
        {
            (void) gpio_set_level(GPIO_LED, 0);
            state = 0;
            printf("RESET!\n");
        }
        else
        {
            if (event >= 0)
            {
                state |= 1 << event;
            }
            else
            {
                state &= ~(1 << -event);
            }

            if (state == enable)
            {
                (void) gpio_set_level(GPIO_LED, 1);
            }
            else
            {
                (void) gpio_set_level(GPIO_LED, 0);
            }
        }
    }
}

void
app_main (void)
{
    int app_cpu = xPortGetCoreID();
    static int left = GPIO_BUTTONL;
    static int right = GPIO_BUTTONR;
    TaskHandle_t h_task = NULL;
    BaseType_t rc = 0;

    vTaskDelay(pdMS_TO_TICKS(2000));

    queue = xQueueCreate(40, sizeof(int32_t));
    assert(queue);
    
    gpio_pad_select_gpio(GPIO_LED);
    (void) gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(GPIO_BUTTONL);
    (void) gpio_set_direction(GPIO_BUTTONL, GPIO_MODE_INPUT);
    (void) gpio_set_pull_mode(GPIO_BUTTONL, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(GPIO_BUTTONR);
    (void) gpio_set_direction(GPIO_BUTTONR, GPIO_MODE_INPUT);
    (void) gpio_set_pull_mode(GPIO_BUTTONR, GPIO_PULLUP_ONLY);

    rc = xTaskCreatePinnedToCore(task_debounce, "debounce_l", 2048, &left, 1, &h_task, app_cpu);
    assert(pdPASS == rc);
    assert(h_task);

    rc = xTaskCreatePinnedToCore(task_debounce, "debounce_r", 2048, &right, 1, &h_task, app_cpu);
    assert(pdPASS == rc);
    assert(h_task);

    rc = xTaskCreatePinnedToCore(task_led, "led", 2048, NULL, 1, &h_task, app_cpu);
    assert(pdPASS == rc);
    assert(h_task);
}