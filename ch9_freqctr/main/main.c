#include <freertos/Freertos.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <driver/ledc.h>
#include <driver/pcnt.h>
#include <driver/adc.h>
#include <driver/periph_ctrl.h>
#include <soc/pcnt_struct.h>
#include <soc/pcnt_reg.h>
#include <esp_err.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../components/ssd1306/ssd1306.h"

#define GPIO_PULSEIN    25
#define GPIO_FREQGEN    26
#define GPIO_ADC        14

#define PWM_GPIO        GPIO_FREQGEN
#define PWM_CH          LEDC_CHANNEL_0
#define PWM_FREQ        2000
#define PWM_RES         LEDC_TIMER_2_BIT

static int32_t g_app_cpu = 0;
static pcnt_isr_handle_t gh_isr_handle = NULL;
static SemaphoreHandle_t gh_sem = NULL;
static QueueHandle_t gh_evtq = NULL;

static void counter_init(void);
static void display_init(SSD1306_t * dev);
static void pwm_init(uint32_t frequency);
static void IRAM_ATTR isr_pulse(void * p_arg);
static void task_loop(void * p_arg);
static void task_monitor(void * p_arg);
static void display_clear(SSD1306_t * dev);
static void oled_lock(void);
static void oled_unlock(void);
static void analog_init(void);
static uint32_t retarget(uint32_t freq, uint32_t usec);
static void oled_freq(SSD1306_t * dev, uint32_t frequency);
static void oled_gen(SSD1306_t * dev, uint32_t frequency);

void
app_main (void)
{
    static SSD1306_t dev = {0};
    BaseType_t ret = 0;

    g_app_cpu = xPortGetCoreID();
    gh_sem = xSemaphoreCreateMutex();
    assert(gh_sem != NULL);
    gh_evtq = xQueueCreate(20, sizeof(uint32_t));
    assert(gh_evtq != NULL);

    pwm_init(PWM_FREQ);
    counter_init();
    display_init(&dev);
    analog_init();
    vTaskDelay(pdMS_TO_TICKS(2000));

    ret = xTaskCreatePinnedToCore(task_monitor, "monitor", 4096, (void *) &dev, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(task_loop, "loop", 4096, (void *) &dev, 1, NULL, g_app_cpu);
    assert(pdPASS == ret);
}

static void
task_loop (void * p_arg)
{
    uint32_t freq = 0;
    SSD1306_t * dev = (SSD1306_t *) p_arg;

    for (;;)
    {
        freq = adc1_get_raw(ADC1_CHANNEL_5) * 80 + 500;
        oled_gen(dev, freq);

        pwm_init(freq);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void
task_monitor (void * p_arg)
{
    uint32_t usecs = 0;
    int16_t thres = 0;
    BaseType_t ret = 0;
    SSD1306_t * dev = (SSD1306_t *) p_arg;
    
    for (;;)
    {
        ESP_ERROR_CHECK(pcnt_counter_clear(PCNT_UNIT_0));
        xQueueReset(gh_evtq);
        ESP_ERROR_CHECK(pcnt_counter_resume(PCNT_UNIT_0));

        ESP_ERROR_CHECK(pcnt_get_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, &thres));
        ret = xQueueReceive(gh_evtq, &usecs, 500);

        if (pdPASS == ret)
        {
            uint32_t frequency = (((uint64_t) thres) - 10) * (uint64_t) 1000000 / usecs;
            oled_freq(dev, frequency);
            thres = retarget(frequency, usecs);
            ESP_ERROR_CHECK(pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, thres));
        }
        else
        {
            ESP_ERROR_CHECK(pcnt_counter_pause(PCNT_UNIT_0));
            ESP_ERROR_CHECK(pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, 25));
        }
    }
}

static void
IRAM_ATTR isr_pulse (void * p_arg)
{
    static uint32_t usecs0 = 0;
    uint32_t intr_status = PCNT.int_st.val;
    uint32_t evt_status;
    uint32_t usecs;
    BaseType_t woken = pdFALSE;

    if (1 == (intr_status & BIT(0)))
    {
        evt_status = PCNT.status_unit[0].val;

        if ((evt_status & PCNT_STATUS_THRES0_M) != 0)
        {
            usecs0 = esp_timer_get_time();
        }
        else if ((evt_status & PCNT_STATUS_THRES1_M) != 0)
        {
            usecs = esp_timer_get_time() - usecs0;
            xQueueSendFromISR(gh_evtq, &usecs, &woken);
            pcnt_counter_pause(PCNT_UNIT_0);
        }

        PCNT.int_clr.val = BIT(0);
    }

    if (woken != 0)
    {
        portYIELD_FROM_ISR();
    }
}

static uint64_t
target (uint32_t usec, uint32_t freq)
{
    if (freq > 100000)
    {
        return ((uint64_t) freq / 1000 * usec / 1000 + 10);
    }
    else
    {
        return ((uint64_t) freq * usec / 1000000 + 10);
    }
}

static uint64_t
useconds (uint64_t thres, uint32_t freq)
{
    return ((thres - 10) * 1000000 / freq);
}

static uint32_t
retarget (uint32_t freq, uint32_t usec)
{
    static const uint32_t target_usecs = 100000;
    uint64_t thres = 0;
    uint64_t usecs = 0;

    thres = target(usec, freq);

    if (thres > 32000)
    {
        thres = target(target_usecs, freq);

        if (thres > 32500)
        {
            thres = 32500;
        }

        usecs = useconds(thres, freq);
        thres = target(usecs, freq);
    }
    else
    {
        thres = target(target_usecs, freq);

        if (thres < 25)
        {
            thres = 25;
        }

        usecs = useconds(thres, freq);
        thres = target(usecs, freq);
    }

    if (thres > 32500)
    {
        thres = 32500;
    }
    else if (thres < 25)
    {
        thres = 25;
    }

    return thres;
}

static void
counter_init (void)
{
    pcnt_config_t cfg = {0};
    static int32_t pcnt_unit = 0;

    cfg.pulse_gpio_num = GPIO_PULSEIN;
    cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    cfg.channel = PCNT_CHANNEL_0;
    cfg.unit = PCNT_UNIT_0;
    cfg.pos_mode = PCNT_COUNT_INC;
    cfg.neg_mode = PCNT_COUNT_DIS;
    cfg.lctrl_mode = PCNT_MODE_KEEP;
    cfg.hctrl_mode = PCNT_MODE_KEEP;
    cfg.counter_h_lim = 32767;
    cfg.counter_l_lim = 0;

    pcnt_unit = PCNT_UNIT_0;
    
    ESP_ERROR_CHECK(pcnt_unit_config(&cfg));
    
    ESP_ERROR_CHECK(pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_0, 10));
    ESP_ERROR_CHECK(pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, 10000));

    ESP_ERROR_CHECK(pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_THRES_0));
    ESP_ERROR_CHECK(pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_THRES_1));

    ESP_ERROR_CHECK(pcnt_counter_pause(PCNT_UNIT_0));
    
    ESP_ERROR_CHECK(pcnt_isr_register(isr_pulse, &pcnt_unit, 0, &gh_isr_handle));
    ESP_ERROR_CHECK(pcnt_intr_enable(PCNT_UNIT_0));
}

static void
pwm_init (uint32_t frequency)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = PWM_RES,
        .freq_hz          = frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = PWM_CH,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_GPIO,
        .hpoint         = 0,
        .duty           = 2
    };

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void
display_init (SSD1306_t * dev)
{
	i2c_master_init(dev, 21, 22, 15);
	ssd1306_init(dev, 128, 64);

    display_clear(dev);
}

static void
analog_init (void)
{
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC_CHANNEL_1, ADC_ATTEN_DB_11);
}

static void
display_clear (SSD1306_t * dev)
{
    ssd1306_clear_screen(dev, false);
    ssd1306_contrast(dev, 0xff);
}

static void
oled_gen (SSD1306_t * dev, uint32_t freq)
{
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%u\t\tgen", freq);

    oled_lock();
    ssd1306_display_text(dev, 0, buf, strlen(buf), false);
    oled_unlock();
}

static void
oled_freq (SSD1306_t * dev, uint32_t freq)
{
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%u\t\tHz", freq);
    
    oled_lock();
    ssd1306_display_text(dev, 5, buf, strlen(buf), false);
    oled_unlock();
}

static void
oled_lock (void)
{
    xSemaphoreTake(gh_sem, portMAX_DELAY);
}

static void
oled_unlock (void)
{
    xSemaphoreGive(gh_sem);
}