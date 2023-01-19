#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

static void
task_loop (void * p_param)
{
    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
    bool state = gpio_get_level(GPIO_LED1) ^ 1;

    for (;;)
    {
        portENTER_CRITICAL(&mutex);
        
        gpio_set_level(GPIO_LED1, state);
        gpio_set_level(GPIO_LED2, state);
        gpio_set_level(GPIO_LED3, state);

        portEXIT_CRITICAL(&mutex);

        state ^= 1;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void
app_main (void)
{
    BaseType_t ret = 0;

    gpio_pad_select_gpio(GPIO_LED1);
    gpio_set_direction(GPIO_LED1, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_LED1, 0);

    gpio_pad_select_gpio(GPIO_LED2);
    gpio_set_direction(GPIO_LED2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED2, 0);

    gpio_pad_select_gpio(GPIO_LED3);
    gpio_set_direction(GPIO_LED3, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED3, 0);

    ret = xTaskCreate(task_loop, "loop", 2048, NULL, 1, NULL);
    assert(pdPASS == ret);
}
