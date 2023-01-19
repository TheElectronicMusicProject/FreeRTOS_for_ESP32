#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

#define N_LED       3

static int32_t g_leds[N_LED] = {
    GPIO_LED1,
    GPIO_LED2,
    GPIO_LED3
};

typedef struct
{
    int32_t index;
    int32_t led_gpio;
    bool state;
} task_local_t;

static void
blink_led (void)
{
    task_local_t * p_local = (task_local_t *) pvTaskGetThreadLocalStoragePointer(NULL, 0);

    vTaskDelay(pdMS_TO_TICKS(p_local->index * 250 + 250));
    p_local->state ^= true;
    gpio_set_level(p_local->led_gpio, p_local->state);
}

static void
task_led (void * p_param)
{
    int32_t val = (int32_t) p_param;
    task_local_t * p_local = (task_local_t *) malloc(sizeof(task_local_t));

    p_local->index = val;
    p_local->led_gpio = g_leds[val];
    p_local->state = false;

    gpio_pad_select_gpio(p_local->led_gpio);
    gpio_set_direction(p_local->led_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(p_local->led_gpio, 0);

    vTaskSetThreadLocalStoragePointer(NULL, 0, p_local);

    for (;;)
    {
        blink_led();
    }
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    BaseType_t ret = 0;

    for (int32_t idx = 0; idx < N_LED; ++idx)
    {
        ret = xTaskCreatePinnedToCore(task_led, "led", 2100, (void *) idx, 1, NULL, app_cpu);
        assert(pdPASS == ret);
    }
}
