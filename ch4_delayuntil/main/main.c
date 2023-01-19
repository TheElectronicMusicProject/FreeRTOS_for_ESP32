#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver/gpio.h"

#define GPIO_LED_1  GPIO_NUM_15
#define GPIO_LED_2  GPIO_NUM_14

static volatile bool startf = false;
static TickType_t period = 250;

static void
big_think (void)
{
    for (int32_t val = 0; val < 40000; ++val)
    {
        __asm__ __volatile__ ("nop");
    }
}

static void
task1 (void * argp)
{
    bool state = true;

    while (!startf)
    {

    }

    for (;;)
    {
        state ^= true;
        gpio_set_level(GPIO_LED_1, state);
        big_think();
        vTaskDelay(period / portTICK_PERIOD_MS);
    }
}

static void
task2 (void * argp)
{
    bool state = true;

    while (!startf)
    {

    }

    TickType_t ticktime = xTaskGetTickCount();

    for (;;)
    {
        state ^= true;
        gpio_set_level(GPIO_LED_2, state);
        big_think();
        vTaskDelayUntil(&ticktime, period / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    int32_t app_cpu = 1;
    BaseType_t ret = pdFALSE;

    gpio_pad_select_gpio(GPIO_LED_1);
    gpio_set_direction(GPIO_LED_1, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED_1, 1);

    gpio_pad_select_gpio(GPIO_LED_2);
    gpio_set_direction(GPIO_LED_2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED_2, 1);

    ret = xTaskCreatePinnedToCore(task1, "task1", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task2, "task2", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    startf = true;
}
