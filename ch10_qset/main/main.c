#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <stdint.h>
#include <stdio.h>

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

#define GPIO_BUT1   GPIO_NUM_26
#define GPIO_BUT2   GPIO_NUM_27
#define GPIO_BUT3   GPIO_NUM_25

#define N_BUTTONS   3
#define Q_DEPTH     8

static void IRAM_ATTR isr_gpio1();
static void IRAM_ATTR isr_gpio2();
static void IRAM_ATTR isr_gpio3();

typedef struct 
{
    int32_t         butn_gpio;
    int32_t         led_gpio;
    QueueHandle_t   h_queue;
    gpio_isr_t      p_isr;
} button_t;

button_t g_buttons[N_BUTTONS] = {
    {GPIO_BUT1, GPIO_LED1, NULL, isr_gpio1},
    {GPIO_BUT2, GPIO_LED2, NULL, isr_gpio2},
    {GPIO_BUT3, GPIO_LED3, NULL, isr_gpio3}
};

inline static BaseType_t IRAM_ATTR
isr_gpiox (uint8_t gpiox)
{
    bool state = gpio_get_level(g_buttons[gpiox].butn_gpio);
    BaseType_t woken = pdFALSE;

    xQueueSendToBackFromISR(g_buttons[gpiox].h_queue, &state, &woken);
    return woken;
}

static void
task_ev (void * p_param)
{
    for (;;)
    {
        QueueSetHandle_t h_qset = (QueueSetHandle_t) p_param;
        QueueSetMemberHandle_t h_memberset = NULL;
        bool b_state = false;
        BaseType_t ret = 0;

        for (;;)
        {
            h_memberset = xQueueSelectFromSet(h_qset, portMAX_DELAY);

            for (uint32_t idx = 0; idx < N_BUTTONS; ++idx)
            {
                if (h_memberset == g_buttons[idx].h_queue)
                {
                    ret = xQueueReceive(h_memberset, &b_state, 0);
                    assert(pdPASS == ret);
                    ESP_ERROR_CHECK(gpio_set_level(g_buttons[idx].led_gpio, b_state));
                    break;
                }
            }
        }
    }
}

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    QueueSetHandle_t h_qset = NULL;
    BaseType_t ret = 0;

    h_qset = xQueueCreateSet(Q_DEPTH * N_BUTTONS);
    assert(h_qset != NULL);

    for (uint32_t idx = 0; idx < N_BUTTONS; ++idx)
    {
        g_buttons[idx].h_queue = xQueueCreate(Q_DEPTH, sizeof(bool));
        assert(g_buttons[idx].h_queue != NULL);

        ret = xQueueAddToSet(g_buttons[idx].h_queue, h_qset);
        assert(pdPASS == ret);

        gpio_pad_select_gpio(g_buttons[idx].led_gpio);
        ESP_ERROR_CHECK(gpio_set_direction(g_buttons[idx].led_gpio, GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(g_buttons[idx].led_gpio, 1));

        gpio_pad_select_gpio(g_buttons[idx].butn_gpio);
        ESP_ERROR_CHECK(gpio_set_direction(g_buttons[idx].butn_gpio, GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_pullup_en(g_buttons[idx].butn_gpio));
        ESP_ERROR_CHECK(gpio_pulldown_dis(g_buttons[idx].butn_gpio));
        ESP_ERROR_CHECK(gpio_set_intr_type(g_buttons[idx].butn_gpio, GPIO_INTR_ANYEDGE));
    }

    ret = xTaskCreatePinnedToCore(task_ev, "evtask", 4096, (void *) h_qset, 1, NULL, app_cpu);
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