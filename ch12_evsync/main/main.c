#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdint.h>
#include <stdlib.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

#define N_LEDS      3

#define EV_RDY      (1 << 3)
#define EV_ALL      (EV_RDY | (1 << 2) | (1 << 1) | (1 << 0))

static EventGroupHandle_t gh_evt = NULL;
static int32_t g_leds[N_LEDS] = {GPIO_LED1, GPIO_LED2, GPIO_LED3};

static void
task_led (void * p_param)
{
    uint32_t ledx = (uint32_t) p_param;
    EventBits_t out_ev = 1 << ledx;
    EventBits_t rev = 0;
    TickType_t timeout = 0;
    uint32_t seed = ledx;
    int32_t led_status = 0;

    assert(ledx < N_LEDS);

    for (;;)
    {
        timeout = rand_r(&seed) % 100 + 50;

        rev = xEventGroupSync(gh_evt, out_ev, EV_ALL, timeout);

        if (EV_ALL == (rev & EV_ALL))
        {
            led_status = gpio_get_level(g_leds[ledx]);
            gpio_set_level(g_leds[ledx], !led_status);
        }
        else
        {
            fprintf(stderr, "LED %d timeout\n", ledx);
        }
    }
}

static void
task_loop (void * p_param)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        xEventGroupSetBits(gh_evt, EV_RDY);
    }
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    BaseType_t ret = 0;

    gh_evt = xEventGroupCreate();
    assert(gh_evt != NULL);

    for (int32_t idx = 0; idx < N_LEDS; ++idx)
    {
        gpio_pad_select_gpio(g_leds[idx]);
        gpio_set_direction(g_leds[idx], GPIO_MODE_INPUT_OUTPUT);
        gpio_set_level(g_leds[idx], 0);

        ret = xTaskCreatePinnedToCore(task_led, "led task", 2048, (void *) idx, 1, NULL, app_cpu);
        assert(pdPASS == ret);
    }

    ret = xTaskCreatePinnedToCore(task_loop, "loop task", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);
}
