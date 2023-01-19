#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/i2c.h>
#include <esp_err.h>
#include <stdint.h>
#include <stdio.h>

#define GPIO_I2C_SDA    21
#define GPIO_I2C_SCL    22
#define PCF8574A        1

#if PCF8574A
#   define DEV0 0x38
#   define DEV1 0x39
#else
#   define DEV0 0x20
#   define DEV1 0x21
#endif /* PCF8574A */

static int32_t g_app_cpu = 0;
static SemaphoreHandle_t gh_mutex = NULL;
static int32_t g_pcf8574_1 = DEV0;
static int32_t g_pcf8574_2 = DEV1;

static void
lock_i2c (void)
{
    BaseType_t ret = 0;

    ret = xSemaphoreTake(gh_mutex, portMAX_DELAY);
    assert(pdPASS == ret);
}

static void
unlock_i2c (void)
{
    BaseType_t ret = 0;

    ret = xSemaphoreGive(gh_mutex);
    assert(pdPASS == ret);
}

static void
task_led (void * p_arg)
{
    int32_t i2c_addr = *((uint32_t *) p_arg);
    bool b_led_status = false;
    uint8_t buffer[1] = {0xFF};
    int32_t ret = 0;

    lock_i2c();

    ret = i2c_master_write_to_device(I2C_NUM_0, i2c_addr, buffer, sizeof(buffer), 1000 / portTICK_RATE_MS);

    if (ESP_OK == ret)
    {
        printf("I2C address 0x%02X present\n", i2c_addr);
    }
    else
    {
        printf("I2C address 0x%02X NOT RESPONDING with error %s\n", i2c_addr, esp_err_to_name(ret));
    }

    unlock_i2c();

    if (ret != ESP_OK)
    {
        vTaskDelete(NULL);
    }

    for (;;)
    {
        lock_i2c();
        b_led_status ^= true;
        printf("LED 0x%02X %s\n", i2c_addr, b_led_status ? "on" : "off");
        buffer[0] = true == b_led_status ? 0xF7 : 0xFF;
        ret = i2c_master_write_to_device(I2C_NUM_0, i2c_addr, buffer, sizeof(buffer), 1000 / portTICK_RATE_MS);
        unlock_i2c();
        vTaskDelay(pdMS_TO_TICKS(i2c_addr & 1 ? 500 : 1000));
    }
}

void
app_main (void)
{
    BaseType_t ret = 0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };
    
    g_app_cpu = xPortGetCoreID();

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

    gh_mutex = xSemaphoreCreateMutex();
    assert(gh_mutex != NULL);

    vTaskDelay(2000);

    ret = xTaskCreatePinnedToCore(task_led, "led task 1", 2048, &g_pcf8574_1, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_led, "led task 2", 2048, &g_pcf8574_2, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);
}
