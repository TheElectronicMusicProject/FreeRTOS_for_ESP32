#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <stdio.h>

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

#define GPIO_BUT1   GPIO_NUM_26
#define GPIO_BUT2   GPIO_NUM_27
#define GPIO_BUT3   GPIO_NUM_25

#define N_BUTTONS   3

static void IRAM_ATTR isr_gpio1();
static void IRAM_ATTR isr_gpio2();
static void IRAM_ATTR isr_gpio3();

typedef struct
{
    int32_t     butn_gpio;
    int32_t     led_gpio;
    uint32_t    buttonx;
    gpio_isr_t  p_isr;
} button_t;

static button_t g_buttons[N_BUTTONS] = {
    {GPIO_BUT1, GPIO_LED1, 0u, isr_gpio1},
    {GPIO_BUT2, GPIO_LED2, 1u, isr_gpio2},
    {GPIO_BUT3, GPIO_LED3, 2u, isr_gpio3}
};

static TaskHandle_t gh_task1 = NULL;

inline static BaseType_t IRAM_ATTR
isr_gpiox (uint8_t gpiox)
{
    BaseType_t woken = pdFALSE;

    xTaskNotifyFromISR(gh_task1, 1 << g_buttons[gpiox].buttonx, eSetBits, &woken);
    return woken;
}

static void
task1 (void * p_param)
{
    uint32_t evt = 0;
    BaseType_t ret = 0;
    uint32_t level = 0;

    for (;;)
    {
        ret = xTaskNotifyWait(0, 0x7, &evt, portMAX_DELAY);
        fprintf(stderr, "Task notified: rv = %u\n", evt);

        for (int32_t idx = 0; idx < N_BUTTONS; ++idx)
        {
            if (evt & (1 << idx))
            {
                level = gpio_get_level(g_buttons[idx].butn_gpio);
                fprintf(stderr, "Button %u notified, reads %d\n", idx, level);
                gpio_set_level(g_buttons[idx].led_gpio, level);
            }
        }
    }
}

void
app_main (void)
{
    int32_t app_cpu = 0;
    BaseType_t ret = 0;

    app_cpu = xPortGetCoreID();
    
    for (uint32_t idx = 0; idx < N_BUTTONS; ++idx)
    {
        gpio_pad_select_gpio(g_buttons[idx].led_gpio);
        ESP_ERROR_CHECK(gpio_set_direction(g_buttons[idx].led_gpio, GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(g_buttons[idx].led_gpio, 1));

        gpio_pad_select_gpio(g_buttons[idx].butn_gpio);
        ESP_ERROR_CHECK(gpio_set_direction(g_buttons[idx].butn_gpio, GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_pullup_en(g_buttons[idx].butn_gpio));
        ESP_ERROR_CHECK(gpio_pulldown_dis(g_buttons[idx].butn_gpio));
        ESP_ERROR_CHECK(gpio_set_intr_type(g_buttons[idx].butn_gpio, GPIO_INTR_ANYEDGE));
    }

    ret = xTaskCreatePinnedToCore(task1, "task1", 3000, NULL, 1, &gh_task1, app_cpu);
    assert(pdPASS == ret);

    gpio_install_isr_service(0);

    for (uint32_t idx = 0; idx < N_BUTTONS; ++idx)
    {
        ESP_ERROR_CHECK(gpio_isr_handler_add(g_buttons[idx].butn_gpio, g_buttons[idx].p_isr, NULL));
    }
}

static void IRAM_ATTR
isr_gpio1 ()
{
    if (isr_gpiox(0))
    {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR
isr_gpio2 ()
{
    if (isr_gpiox(1))
    {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR
isr_gpio3 ()
{
    if (isr_gpiox(2))
    {
        portYIELD_FROM_ISR();
    }
}