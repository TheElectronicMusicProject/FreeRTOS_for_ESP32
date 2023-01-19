#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED        GPIO_NUM_14
#define GPIO_TRIGGER    GPIO_NUM_32
#define GPIO_ECHO       GPIO_NUM_33

typedef uint32_t usec_t;

static SemaphoreHandle_t gh_barrier = NULL;
static TickType_t g_repeat_ticks = 100;

static void
report_cm (usec_t usecs)
{
    uint32_t dist_cm = 0;
    uint32_t tenths = 0;

    dist_cm = usecs * 10ul / 58ul;
    tenths = dist_cm % 10;
    dist_cm /= 10;

    fprintf(stderr, "Distance %u.%u cm, %u usecs\n", dist_cm, tenths, usecs);
}

static void
task_range (void * argp)
{
    BaseType_t ret = 0;
    usec_t usecs = 0;
    int64_t start_us = 0;
    bool b_timeout = false;

    for (;;)
    {
        ret = xSemaphoreTake(gh_barrier, portMAX_DELAY);
        assert(pdPASS == ret);

        gpio_set_level(GPIO_LED, 1);
        gpio_set_level(GPIO_TRIGGER, 1);
        ets_delay_us(10);
        gpio_set_level(GPIO_TRIGGER, 0);

        // Previous ping isn't ended
        if (1 == gpio_get_level(GPIO_ECHO))
        {
            fprintf(stderr, "Previous ping not ended\n");
        }
        else
        {
            // Wait for echo
            //
            start_us = esp_timer_get_time();
            while (0 == gpio_get_level(GPIO_ECHO))
            {
                if ((esp_timer_get_time() - start_us) > 25000L)
                {
                    b_timeout = true;
                    break;
                }
            }

            // Got echo, measuring
            //
            int64_t echo_start = esp_timer_get_time();
            int64_t time = echo_start;

            while (1 == gpio_get_level(GPIO_ECHO))
            {
                time = esp_timer_get_time();

                if ((time - echo_start) > 25000L)
                {
                    b_timeout = true;
                    break;
                }
            }

            usecs = time - echo_start;
        }

        gpio_set_level(GPIO_LED, 0);

        if ((usecs > 0) && (usecs < 50000L) && (false == b_timeout))
        {
            report_cm(usecs);
        }
        else
        {
            b_timeout = false;
            fprintf(stderr, "No echo\n");
        }

    }
}

static void
task_sync (void * argp)
{
    BaseType_t ret = 0;
    TickType_t ticktime = 0;

    vTaskDelay(pdMS_TO_TICKS(1000));

    ticktime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&ticktime, g_repeat_ticks);
        ret = xSemaphoreGive(gh_barrier);
    }
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    TaskHandle_t h_task = NULL;
    BaseType_t ret = 0;
    TickType_t ticktime = xTaskGetTickCount();

    gh_barrier = xSemaphoreCreateBinary();
    assert(gh_barrier != NULL);

    gpio_pad_select_gpio(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED, 0);

    gpio_pad_select_gpio(GPIO_TRIGGER);
    gpio_set_direction(GPIO_TRIGGER, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_TRIGGER, 0);

    gpio_pad_select_gpio(GPIO_ECHO);
    gpio_set_direction(GPIO_ECHO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ECHO, GPIO_PULLUP_ENABLE);

    vTaskDelayUntil(&ticktime, pdMS_TO_TICKS(2000));

    ret = xTaskCreatePinnedToCore(task_range, "tast range", 2048, NULL, 1, &h_task, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_sync, "tast sync", 2048, NULL, 1, &h_task, app_cpu);
    assert(pdPASS == ret);
}
