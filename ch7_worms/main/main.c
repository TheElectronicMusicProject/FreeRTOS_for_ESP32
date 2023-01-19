#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <esp_log.h>
#include "ssd1306.h"

#define WORM1_TASK_PRIORITY     9
#define WORM2_TASK_PRIORITY     9
#define WORM3_TASK_PRIORITY     9

#define MAIN_TASK_PRIORITY      10

#define tag "SSD1306"

static QueueHandle_t gh_queue = NULL;

typedef struct
{
    int32_t seg_w;
    int32_t seg_sw;
    int32_t seg_h;
    int32_t worm;
    int32_t x_coord;
    int32_t y_coord;
    int32_t width;
    int32_t height;
    int32_t direction;
    int32_t state;
} inch_worm_t;

inch_worm_t worm1 = {0};
inch_worm_t worm2 = {0};
inch_worm_t worm3 = {0};

static void
display_clear (SSD1306_t * dev)
{
    ssd1306_clear_screen(dev, false);
}

static void
display_draw (SSD1306_t * dev, inch_worm_t * worm)
{
    int pos_y = 7 + (worm->worm - 1) * 200;
    int pos_x = 2 + worm->x_coord;

    pos_y += worm->height - 3;
    display_clear(dev);
    //_ssd1306_line(dev, pos_x, pos_y - 2 * worm->seg_h, 3 * worm->seg_w, 3 * worm->seg_h, false);

    switch (worm->state)
    {
        default:
        case 0: // _-_
            _ssd1306_line(dev, pos_x, pos_y, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w, pos_y - worm->seg_h, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w + worm->seg_sw, pos_y, worm->seg_w, worm->seg_h, false);
        break;

        case 1: // _^_
            _ssd1306_line(dev, pos_x, pos_y, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w, pos_y -  2 * worm->seg_h, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w + worm->seg_sw, pos_y, worm->seg_w, worm->seg_h, false);
        break;

        case 2: // _^^_
            if (worm->direction < 0)
            {
                pos_x -= worm->seg_sw;
            }

            _ssd1306_line(dev, pos_x, pos_y, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w, pos_y -  2 * worm->seg_h, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + 2 * worm->seg_w + worm->seg_sw, pos_y, worm->seg_w, worm->seg_h, false);
        break;

        case 3: // _-_
            if (worm->direction < 0)
            {
                pos_x -= worm->seg_sw;
            }
            else
            {
                pos_x += worm->seg_sw;
            }

            _ssd1306_line(dev, pos_x, pos_y, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w, pos_y - worm->seg_h, worm->seg_w, worm->seg_h, false);
            //_ssd1306_line(dev, pos_x + worm->seg_w + worm->seg_sw, pos_y, worm->seg_w, worm->seg_h, false);
        break;
    }

    worm->state = (worm->state + 1) % 4;
    if (!worm->state)
    {
        worm->x_coord += worm->direction + worm->seg_sw;
        if (worm->direction > 0)
        {
            if (worm->x_coord + 3 * worm->seg_w + worm->seg_sw >= 128)
            {
                worm->direction = -1;
            }
            else if (worm->x_coord <= 2)
            {
                worm->direction = 1;
            }
        }
    }

    ssd1306_show_buffer(dev);
}

static void
display_init (SSD1306_t * dev)
{
	i2c_master_init(dev, 21, 22, 15);

    ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(dev, 128, 64);

    display_clear(dev);
    ssd1306_contrast(dev, 0xff);
}

static void
loop_task (void * p_arg)
{
    SSD1306_t * dev = (SSD1306_t *) p_arg;
    inch_worm_t * worm = NULL;

    for (;;)
    {
        if (pdPASS == xQueueReceive(gh_queue, &worm, 1))
        {
            display_draw(dev, worm);
        }
        else
        {
            vTaskDelay(1);
        }
    }
}

static void
worm_task (void * p_arg)
{
    inch_worm_t * worm = (inch_worm_t *) p_arg;

    for (;;)
    {
        for (int idx = 0; idx < 800000; ++idx)
        {
            __asm__ __volatile__ ("nop");
        }

        xQueueSendToBack(gh_queue, &worm, 0);
    }
}

void
app_main (void)
{
    static SSD1306_t dev = {0};
    BaseType_t ret = 0;
    int32_t app_cpu = xPortGetCoreID();
    TaskHandle_t h_task = xTaskGetCurrentTaskHandle();

    display_init(&dev);

    vTaskPrioritySet(h_task, MAIN_TASK_PRIORITY);
    gh_queue = xQueueCreate(4, sizeof(inch_worm_t *));

    worm1.seg_w = 9;
    worm1.seg_sw = 4;
    worm1.seg_h = 3;
    worm1.width = 30;
    worm1.height = 10;
    worm1.direction = 1;
    worm1.state = 0;
    worm1.worm = 1;

    worm2.seg_w = 9;
    worm2.seg_sw = 4;
    worm2.seg_h = 3;
    worm2.width = 30;
    worm2.height = 10;
    worm2.direction = 1;
    worm2.state = 0;
    worm2.worm = 1;

    worm3.seg_w = 9;
    worm3.seg_sw = 4;
    worm3.seg_h = 3;
    worm3.width = 30;
    worm3.height = 10;
    worm3.direction = 1;
    worm3.state = 0;
    worm3.worm = 1;

    display_draw(&dev, &worm1);
    display_draw(&dev, &worm2);
    display_draw(&dev, &worm3);

    ret = xTaskCreatePinnedToCore(worm_task, "worm 1", 3000, &worm1, WORM1_TASK_PRIORITY, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(worm_task, "worm 2", 3000, &worm2, WORM2_TASK_PRIORITY, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(worm_task, "worm 3", 3000, &worm3, WORM3_TASK_PRIORITY, NULL, app_cpu);
    assert(pdPASS == ret);

    ret = xTaskCreatePinnedToCore(loop_task, "loop", 2048, &dev, MAIN_TASK_PRIORITY, NULL, app_cpu);
    assert(pdPASS == ret);

    vTaskDelay(pdMS_TO_TICKS(1000));
}
