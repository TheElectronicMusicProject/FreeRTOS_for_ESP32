#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>

#define LED1    GPIO_NUM_15
#define LED2    GPIO_NUM_25
#define LED3    GPIO_NUM_27

typedef struct
{
    uint8_t         gpio;
    uint8_t         state;
    uint32_t        napms;
    TaskHandle_t    taskh;
} s_led;

static char * gp_tag = "task";

static s_led g_leds[3] = {
    {LED1, 0, 500, 0},
    {LED2, 0, 200, 0},
    {LED3, 0, 750, 0}
};

static void
led_task_func (void * argp)
{
    s_led * p_led = (s_led *) argp;
    uint32_t stack_hwm = 0;
    uint32_t temp = 0;

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;)
    {
        p_led->state ^= 1;
        (void) gpio_set_level(p_led->gpio, p_led->state);
        temp = uxTaskGetStackHighWaterMark(NULL);

        if ((0 == stack_hwm) || (temp < stack_hwm))
        {
            stack_hwm = temp;
            ESP_LOGI(gp_tag, "Task for gpio %d has stack hwm %u, heap %u",
                     p_led->gpio, stack_hwm, xPortGetFreeHeapSize());
        }

        vTaskDelay(pdMS_TO_TICKS(p_led->napms));
    }
}

static void
task_loop (void * argp)
{
    for (;;)
    {
        fprintf(stderr, "stampo\n");
        vTaskDelete(NULL);
    }
}

void
app_main (void)
{
    uint32_t    app_cpu = 0;
    int32_t     idx = 0;

    vTaskDelay(pdMS_TO_TICKS(500));

    app_cpu = xPortGetCoreID();
    ESP_LOGI(gp_tag, "app_cpu is %d (%s core)",
             app_cpu, app_cpu > 0 ? "Dual" : "Single");

    printf("LEDs on gpios:");

    for (idx = 0; idx < 3; ++idx)
    {
        gpio_pad_select_gpio(g_leds[idx].gpio);
        (void) gpio_set_direction(g_leds[idx].gpio, GPIO_MODE_OUTPUT);
        (void) gpio_set_level(g_leds[idx].gpio, 0);

        (void) xTaskCreatePinnedToCore(led_task_func,
                                       "led_task",
                                       2048, &g_leds[idx], 1,
                                       &g_leds[idx].taskh, app_cpu);
        printf("%d ", g_leds[idx].gpio);

        ESP_LOGI(gp_tag, "There are %u heap bytes available",
                 xPortGetFreeHeapSize());
    }

    xTaskCreate(task_loop, "task_loop", 1024, NULL, 1, NULL);

    printf("\n");
}
