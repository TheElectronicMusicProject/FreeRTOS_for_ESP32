#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_err.h>
#include <stdio.h>

#define GPIO_LED    GPIO_NUM_19

static TaskHandle_t gh_task = NULL;
static SemaphoreHandle_t gh_semph = NULL;

static void
task_hall (void * p_param)
{
    esp_err_t err = 0;
    BaseType_t ret = 0;
    uint32_t seed = hall_sensor_read();
    printf("seed = %d\n", seed);

    gh_task = xTaskGetCurrentTaskHandle();

    err = esp_task_wdt_status(gh_task);
    assert(ESP_ERR_NOT_FOUND == err);

    if (ESP_ERR_NOT_FOUND == err)
    {
        err = esp_task_wdt_init(5, true);
        assert(ESP_OK == err);
        err = esp_task_wdt_add(gh_task);
        assert(ESP_OK == err);
    }

    ret = xSemaphoreGive(gh_semph);
    assert(pdPASS == ret);

    for (;;)
    {
        gpio_set_level(GPIO_LED, 1 ^ gpio_get_level(GPIO_LED));
        esp_task_wdt_reset();
        vTaskDelay(rand_r(&seed) % 7 * 10);
    }
}

static void
task_loop (void * p_param)
{
    int32_t dly = 1000;
    esp_err_t err = 0;

    xSemaphoreTake(gh_semph, portMAX_DELAY);

    for (;;)
    {
        printf("loop(dly=%d)..\n", dly);
        err = esp_task_wdt_status(gh_task);
        assert(ESP_OK == err);
        vTaskDelay(pdMS_TO_TICKS(dly));
        dly += 1000;
    }
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    BaseType_t ret = 0;

    gh_semph = xSemaphoreCreateBinary();

    gpio_pad_select_gpio(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_LED, 0);

    adc1_config_width(ADC_WIDTH_BIT_12);

    ret = xTaskCreatePinnedToCore(task_loop, "loop", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_hall, "task2", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);
}
