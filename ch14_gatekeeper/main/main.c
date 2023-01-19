#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <stdint.h>
#include <stdio.h>

#define GPIO_I2C_SDA    GPIO_NUM_25
#define GPIO_I2C_SCL    GPIO_NUM_26
#define PCF8574A        1

#if PCF8574A
#   define DEV0 0x38
#   define DEV1 0x39
#else
#   define DEV0 0x20
#   define DEV1 0x21
#endif /* PCF8574A */

#define BUTTON0         12
#define BUTTON1         5
#define BUTTON2         4
#define N_BUTTON        3

#define LED0            1
#define LED1            2
#define LED2            3
#define LED3            9

#define N_DEV           2

#define GATEKRDY        (1 << 0)
#define IO_RDY          (1 << 0)
#define IO_ERROR        (1 << 1)
#define IO_BIT          (1 << 2)

#define STOP            ((int32_t) 1)

typedef struct
{
    EventGroupHandle_t grpevt;
    QueueHandle_t queue;
    uint8_t states[N_DEV];
} gatekeeper_t;

typedef struct
{
    uint8_t input:1;        // 1 input, 0 output
    uint8_t value:1;        // Valore bit (letto o da scrivere)
    uint8_t error:1;        // 1 errore presente, 0 errore assente
    uint8_t port:5;         // Numero della porta
    TaskHandle_t h_task;    // Handler del task di risposta
} ioport_t;

typedef struct
{
    uint8_t button;
    uint8_t led;
} states_t;

static gatekeeper_t g_gatekeeper = {NULL, NULL, {0xFF, 0xFF}};

static void task_gatekeeper(void * p_param);
static void task_usr1(void * p_param);
static void task_usr2(void * p_param);

static void
pcf8574_wait_ready (void)
{
    xEventGroupWaitBits(g_gatekeeper.grpevt,
                        GATEKRDY,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
}

static int16_t
pcf8574_get (uint8_t port)
{
    static ioport_t ioport = {0};
    uint32_t notify = 0;
    int16_t val = 0;
    BaseType_t ret = 0;

    assert(port < 16);
    ioport.input = true;
    ioport.port = port;
    ioport.h_task = xTaskGetCurrentTaskHandle();

    pcf8574_wait_ready();

    ret = xQueueSendToBack(g_gatekeeper.queue, &ioport, portMAX_DELAY);
    assert(pdPASS == ret);

    ret = xTaskNotifyWait(pdFALSE, IO_BIT | IO_ERROR | IO_RDY, &notify, portMAX_DELAY);
    assert(pdTRUE == ret);

    val = ((notify & IO_ERROR) ? -1 : !!(notify & IO_BIT));

    return val;
}

static int16_t
pcf8574_put (uint8_t port, bool value)
{
    ioport_t ioport = {0};
    BaseType_t ret = 0;
    uint32_t notify = 0;
    int16_t val = 0;

    assert(port < 16);
    ioport.input = false;
    ioport.port = port;
    ioport.value = value;
    ioport.h_task = xTaskGetCurrentTaskHandle();

    pcf8574_wait_ready();

    ret = xQueueSendToBack(g_gatekeeper.queue, &ioport, portMAX_DELAY);
    assert(pdPASS == ret);

    ret = xTaskNotifyWait(pdFALSE, IO_BIT | IO_ERROR | IO_RDY, &notify, portMAX_DELAY);
    assert(pdTRUE == ret);

    val = ((notify & IO_ERROR) ? -1 : !!(notify & IO_BIT));

    return val;
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    BaseType_t ret = 0;

    g_gatekeeper.grpevt = xEventGroupCreate();
    assert(g_gatekeeper.grpevt != NULL);

    ret = xTaskCreatePinnedToCore(task_gatekeeper, "gatekeeper", 2048, NULL, 2, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_usr1, "usrtask1", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_usr2, "usrtask2", 2048, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);
}

static void
i2c_init (int32_t gpio_sda, int32_t gpio_scl)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = gpio_sda,
        .scl_io_num = gpio_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
}

static void
task_gatekeeper (void * p_param)
{
    int32_t i2caddr[N_DEV] = {DEV0, DEV1};
    int32_t addr = 0;
    uint8_t devx = 0;
    uint8_t portx = 0;
    ioport_t ioport = {0};
    uint32_t notify = 0;
    BaseType_t ret = 0;

    g_gatekeeper.queue = xQueueCreate(8, sizeof ioport);
    assert(g_gatekeeper.queue != NULL);

    i2c_init(GPIO_I2C_SDA, GPIO_I2C_SCL);

    for (devx = 0; devx < N_DEV; ++devx)
    {
        uint8_t buffer[1] = {0xFF};
        addr = i2caddr[devx];
        ret = i2c_master_write_to_device(I2C_NUM_0, addr, buffer, sizeof(buffer), 1000 / portTICK_RATE_MS);
        
        if (ESP_OK == ret)
        {
            printf("I2C address 0x%02X present\n", addr);
        }
        else
        {
            printf("I2C address 0x%02X NOT RESPONDING with error %s\n", addr, esp_err_to_name(ret));
        }
    }

    xEventGroupSetBits(g_gatekeeper.grpevt, GATEKRDY);

    for (;;)
    {
        notify = 0;

        ret = xQueueReceive(g_gatekeeper.queue, &ioport, portMAX_DELAY);
        assert(pdPASS == ret);

        devx = ioport.port / 8;
        portx = ioport.port % 8;
        assert(devx < N_DEV);
        addr = i2caddr[devx];

        if (ioport.input != 0)
        {
            uint8_t out_data[1] = {0};
            i2c_cmd_handle_t h_cmd = i2c_cmd_link_create();
            ret = i2c_master_start(h_cmd);

            if (ret != ESP_OK)
            {
                goto input_err;
            }

            ret = i2c_master_write_byte(h_cmd, addr << 1 | I2C_MASTER_READ, true);

            if (ret != ESP_OK)
            {
                goto input_err;
            }

            ret = i2c_master_read_byte(h_cmd, out_data, I2C_MASTER_NACK);
            
            if (ret != ESP_OK)
            {
                goto input_err;
            }

            ret = i2c_master_stop(h_cmd);

            if (ret != ESP_OK)
            {
                goto input_err;
            }

            ret = i2c_master_cmd_begin(I2C_NUM_0, h_cmd, 1000 / portTICK_RATE_MS);
input_err:
            i2c_cmd_link_delete(h_cmd);
            
            if (ESP_OK == ret)
            {
                ioport.error = false;
                ioport.value = ((out_data[0] >> portx) & 1 );
            }
            else
            {
                printf("\terrore 1\n");
                ioport.error = true;
                ioport.value = false;
            }
        }
        else
        {
            uint8_t data[1] = {0};
            data[0] = g_gatekeeper.states[devx];
            
            if (ioport.value != 0)
            {
                data[0] |= 1 << portx;
            }
            else
            {
                data[0] &= ~(1 << portx);
            }

            i2c_cmd_handle_t h_cmd = i2c_cmd_link_create();
            ret = i2c_master_start(h_cmd);

            if (ret != ESP_OK)
            {
                goto output_err;
            }

            ret = i2c_master_write_byte(h_cmd, addr << 1 | I2C_MASTER_WRITE, true);

            if (ret != ESP_OK)
            {
                goto output_err;
            }

            ret = i2c_master_write_byte(h_cmd, data[0], true);

            if (ret != ESP_OK)
            {
                goto output_err;
            }

            ret = i2c_master_stop(h_cmd);

            if (ret != ESP_OK)
            {
                goto output_err;
            }

            ret = i2c_master_cmd_begin(I2C_NUM_0, h_cmd, 1000 / portTICK_RATE_MS);
output_err:
            i2c_cmd_link_delete(h_cmd);

            if (ESP_OK == ret)
            {
                ioport.error = 0;
                g_gatekeeper.states[devx] = data[0];
            }
            else
            {
                printf("\terrore 2\n");
                ioport.error = 1;
            }
        }

        notify = IO_RDY;

        if (1 == ioport.error)
        {
            notify |= IO_ERROR;
        }

        if (ioport.value != 0)
        {
            notify |= IO_BIT;
        }

        if (ioport.h_task != NULL)
        {
            xTaskNotify(ioport.h_task, notify, eSetValueWithOverwrite);
        }
    }
}

static void
task_usr1 (void * p_param)
{
    const states_t states[3] = {{BUTTON0, LED0},
                                {BUTTON1, LED1},
                                {BUTTON2, LED2}};
    int16_t ret = 0;

    for (uint32_t idx = 0; idx < N_BUTTON; ++idx)
    {
        ret = pcf8574_put(states[idx].led, true);
        assert(ret != -1);
    }

    for (;;)
    {
        for (uint32_t idx = 0; idx < N_BUTTON; ++idx)
        {
            ret = pcf8574_get(states[idx].button);
            assert(ret != -1);
            ret = pcf8574_put(states[idx].led, ret & 1);
            assert(ret != -1);
        }
    }
}

static void
task_usr2 (void * p_param)
{
    bool state = false;
    int16_t ret;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        ret = pcf8574_get(LED3);
        assert(ret != -1);
        state = !(ret & 1);
        pcf8574_put(LED3, state);
    }
}