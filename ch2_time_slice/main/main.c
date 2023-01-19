#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"

#define GPIO    23

static void
gpio_on (void * argp)
{
    for (;;)
    {
        (void) gpio_set_level(GPIO, 1);
    }
}

static void
gpio_off (void * argp)
{
    for (;;)
    {
        (void) gpio_set_level(GPIO, 0);
    }
}

void
app_main (void)
{
    gpio_pad_select_gpio(GPIO);
    (void) gpio_set_direction(GPIO, GPIO_MODE_OUTPUT);
    
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Setup started..\n");

    xTaskCreatePinnedToCore(gpio_on, "gpio_on", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(gpio_off, "gpio_off", 2048, NULL, 1, NULL, 1);
}