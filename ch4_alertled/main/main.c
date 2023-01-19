#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver/gpio.h"
#include <stdio.h>

#define GPIO_LED    GPIO_NUM_15

typedef struct
{
    TimerHandle_t h_timer;
    volatile bool state;
    volatile uint32_t count;
    uint32_t period_ms;
    int32_t gpio;
} alert_led;

static alert_led str_led_alert =
{
    NULL,
    0,
    0,
    1000,
    GPIO_LED
};

static void
led_init (alert_led * led_str)
{
    gpio_pad_select_gpio(led_str->gpio);
    (void) gpio_set_direction(led_str->gpio, GPIO_MODE_OUTPUT);
    (void) gpio_set_level(led_str->gpio, 0);
}

static void
led_reset (alert_led * led_str, bool status)
{
    led_str->state = status;
    led_str->count = 0;
    (void) gpio_set_level(led_str->gpio, status ? 1 : 0);
}

static void
led_cancel (alert_led * led_str)
{
    if (NULL != led_str->h_timer)
    {
        xTimerStop(led_str->h_timer, portMAX_DELAY);
        (void) gpio_set_level(led_str->gpio, 0);
    }
}

static void
led_callback (TimerHandle_t h_tr)
{
    alert_led * p_obj = (alert_led *) pvTimerGetTimerID(h_tr);

    assert(p_obj->h_timer != NULL);

    p_obj->state ^= true;
    (void) gpio_set_level(p_obj->gpio, p_obj->state ? 1 : 0);

    if (++p_obj->count >= 5 * 2)
    {
        led_reset(p_obj, true);
        xTimerChangePeriod(p_obj->h_timer, pdMS_TO_TICKS(p_obj->period_ms / 20), portMAX_DELAY);
    }

    if (5 * 2 - 1 == p_obj->count)
    {
        xTimerChangePeriod(p_obj->h_timer,
                           pdMS_TO_TICKS(p_obj->period_ms / 20 + p_obj->period_ms / 2),
                           portMAX_DELAY);

        assert(false == p_obj->state);
    }
}

static void
led_alert (alert_led * led_str)
{
    if (NULL == led_str->h_timer)
    {
        led_str->h_timer = xTimerCreate("alert_tmr", pdMS_TO_TICKS(led_str->period_ms / 20),
                                        pdTRUE, led_str, led_callback);
        assert(led_str->h_timer);
    }

    led_reset(led_str, true);
    xTimerStart(led_str->h_timer, portMAX_DELAY);
}

static void
task_loop (void * argp)
{
    uint32_t loop_count = 70;

    for (;;)
    {
        if (loop_count >= 70)
        {
            led_alert(&str_led_alert);
            loop_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        if (++loop_count >= 50)
        {
            led_cancel(&str_led_alert);
        }
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    led_init(&str_led_alert);

    xTaskCreatePinnedToCore(task_loop, "loop", 8192, NULL, 1, NULL, 1);
}
