#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <driver/adc.h>
#include <math.h>
#include <stdio.h>

#define PIN_S1  GPIO_NUM_12
#define PIN_S2  GPIO_NUM_13

static int32_t              g_app_cpu = 0;
static QueueHandle_t        gh_queue_sens1 = NULL;
static QueueHandle_t        gh_queue_sens2 = NULL;
static SemaphoreHandle_t    gh_sem_disp = NULL;
static SemaphoreHandle_t    gh_sem_comm = NULL;

typedef struct
{
    float    temp;
} sens1_t;

typedef struct
{
    float    temp;
} sens2_t;

double
thermistor (int32_t adc_raw)
{
    double temp = 0.0;
    double vout = 0.0;
    double rth_log = 0.0;
    
    vout = (adc_raw * 3.3) / 4096.0;
    rth_log = log((3.3 * 10000.0 / vout) - 10000.0);
    temp = 1.0 / (1.129148e-3 + (2.34125e-4 + (8.76741e-8 * rth_log * rth_log)) * rth_log);
    temp = temp - 273.15;

    return temp;
}

static inline BaseType_t
lock_resource (void)
{
    BaseType_t ret = 0;

    ret = xSemaphoreTake(gh_sem_comm, portMAX_DELAY);
    assert(pdPASS == ret);

    return ret;
}

static inline BaseType_t
unlock_resource (void)
{
    BaseType_t ret = 0;

    ret = xSemaphoreGive(gh_sem_comm);
    assert(pdPASS == ret);

    return ret;
}

static void
task_temp1 (void * p_arg)
{
    sens1_t reading;
    BaseType_t ret = 0;
    int32_t raw_reading = 0;

    for (;;)
    {
        lock_resource();
        adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &raw_reading);
        unlock_resource();

        reading.temp = thermistor(raw_reading);
        ret = xQueueOverwrite(gh_queue_sens1, &reading);
        assert(pdPASS == ret);

        xSemaphoreGive(gh_sem_disp);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void
task_temp2 (void * p_arg)
{
    sens2_t reading;
    BaseType_t ret = 0;
    int32_t raw_reading = 0;

    for (;;)
    {
        lock_resource();
        adc2_get_raw(ADC2_CHANNEL_4, ADC_WIDTH_BIT_12, &raw_reading);
        unlock_resource();

        reading.temp = thermistor(raw_reading);
        ret = xQueueOverwrite(gh_queue_sens2, &reading);
        assert(pdPASS == ret);

        xSemaphoreGive(gh_sem_disp);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void
task_disp (void * p_arg)
{
    sens1_t temp1_reading;
    sens2_t temp2_reading;
    BaseType_t ret = 0;

    for (;;)
    {
        ret = xSemaphoreTake(gh_sem_disp, portMAX_DELAY);
        assert(pdPASS == ret);

        ret = xQueuePeek(gh_queue_sens1, &temp1_reading, 0);
        if (pdPASS == ret)
        {
            printf("T1 = %4.2fC \n", temp1_reading.temp);
        }
        else
        {
            printf("T1 not available \n");
        }

        ret = xQueuePeek(gh_queue_sens2, &temp2_reading, 0);
        if (pdPASS == ret)
        {
            printf("T2 = %4.2fC \n", temp2_reading.temp);
        }
        else
        {
            printf("T2 not available \n");
        }
    }
}

void
app_main (void)
{
    BaseType_t ret = 0;

    g_app_cpu = xPortGetCoreID();
    gh_sem_disp = xSemaphoreCreateBinary();
    assert(gh_sem_disp != NULL);

    gh_sem_comm = xSemaphoreCreateBinary();
    assert(gh_sem_comm != NULL);
    ret = xSemaphoreGive(gh_sem_comm);
    assert(pdPASS == ret);

    gh_queue_sens1 = xQueueCreate(1, sizeof(sens1_t));
    assert(gh_queue_sens1 != NULL);
    gh_queue_sens2 = xQueueCreate(1, sizeof(sens2_t));
    assert(gh_queue_sens2 != NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    ret = xTaskCreatePinnedToCore(task_temp1, "temp1", 2048, NULL, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);
    ret = xTaskCreatePinnedToCore(task_temp2, "temp2", 2048, NULL, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_disp, "display", 4092, NULL, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);
}