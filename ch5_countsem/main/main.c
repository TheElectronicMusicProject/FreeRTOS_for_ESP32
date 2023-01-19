#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdio.h>

#define RANDOM_EN           1

#if RANDOM_EN
#   include <driver/adc.h>
#endif /* RANDOM_EN */

#define PREVENT_DEADLOCK    1
#define N                   4
#define N_EATERS            (N - 1)

static QueueHandle_t        g_msgq;
static SemaphoreHandle_t    g_csem;
static int32_t app_cpu = 0;

typedef enum
{
    THINKING = 0,
    HUNGRY,
    EATING
} state_t;

static const char * state_name[] = {
    "Thinking",
    "Hungry",
    "Eating"
};

typedef struct
{
    TaskHandle_t    h_task;
    uint32_t        num;
    state_t         state;
    uint32_t        seed;
} s_phylosopher_t;

typedef struct
{
    uint32_t        num;
    state_t         state;
} s_message_t;

static s_phylosopher_t philosophers[N];
static SemaphoreHandle_t forks[N];
static volatile uint32_t logno = 0;

static BaseType_t
send_state (s_phylosopher_t * philo)
{
    s_message_t msg;
    BaseType_t ret;

    msg.num = philo->num;
    msg.state = philo->state;

    ret = xQueueSendToBack(g_msgq, &msg, portMAX_DELAY);

    return ret;
}

static void
task_philo (void * p_arg)
{
    s_phylosopher_t * philo = (s_phylosopher_t *) p_arg;
    SemaphoreHandle_t fork1 = 0;
    SemaphoreHandle_t fork2 = 0;
    BaseType_t ret;

    vTaskDelay(pdMS_TO_TICKS(rand_r(&philo->seed) % 20 + 10));

    for (;;)
    {
        philo->state = THINKING;
        send_state(philo);
        vTaskDelay(pdMS_TO_TICKS(rand_r(&philo->seed) % 20 + 10));

        philo->state = HUNGRY;
        send_state(philo);
        vTaskDelay(pdMS_TO_TICKS(rand_r(&philo->seed) % 20 + 10));

#if PREVENT_DEADLOCK
        ret = xSemaphoreTake(g_csem, portMAX_DELAY);
        assert(pdPASS == ret);
#endif /* PREVENT_DEADLOCK */

        fork1 = forks[philo->num];
        fork2 = forks[(philo->num + 1) % N];
        ret = xSemaphoreTake(fork1, portMAX_DELAY);
        assert(pdPASS == ret);
        vTaskDelay(pdMS_TO_TICKS(rand_r(&philo->seed) % 20 + 10));
        ret = xSemaphoreTake(fork2, portMAX_DELAY);
        assert(pdPASS == ret);

        philo->state = EATING;
        send_state(philo);
        vTaskDelay(pdMS_TO_TICKS(rand_r(&philo->seed) % 20 + 10));

        ret = xSemaphoreGive(fork1);
        assert(pdPASS == ret);
        vTaskDelay(pdMS_TO_TICKS(1));
        ret = xSemaphoreGive(fork2);
        assert(pdPASS == ret);

#if PREVENT_DEADLOCK
        ret = xSemaphoreGive(g_csem);
        assert(pdPASS == ret);
#endif /* PREVENT_DEADLOCK */
    }
}

static void
task_loop (void * p_arg)
{
    s_message_t msg;

    for (;;)
    {
        while (pdPASS == xQueueReceive(g_msgq, &msg, 1))
        {
            fprintf(stderr, "%05u: Philosopher %u is %s\n",
                    ++logno, msg.num, state_name[msg.state]);
        }

        ets_delay_us(1000);
    }
}

void
app_main (void)
{
    BaseType_t ret = 0;
    
    app_cpu = xPortGetCoreID();
    g_msgq = xQueueCreate(30, sizeof(s_message_t));
    assert(g_msgq != NULL);

    for (uint32_t idx = 0; idx < N; ++idx)
    {
        forks[idx] = xSemaphoreCreateBinary();
        assert(forks[idx] != NULL);
        ret = xSemaphoreGive(forks[idx]);
        assert(pdPASS == ret);
        assert(forks[idx] != NULL);
    }

#if RANDOM_EN
    adc1_config_width(ADC_WIDTH_BIT_12);
#endif /* RANDOM_EN */

    fprintf(stderr, "\nThe dining philosopher's problem:\n");
    fprintf(stderr, "There are %u philosophers.\n", N);

#if PREVENT_DEADLOCK
    g_csem = xSemaphoreCreateCounting(N_EATERS, N_EATERS);
    assert(g_csem != NULL);
    fprintf(stderr, "With deadlock prevention.\n");
#else
    g_csem = NULL;
    fprintf(stderr, "Without deadlock prevention.\n");
#endif /* PREVENT_DEADLOCK */

    for (uint32_t idx = 0; idx < N; ++idx)
    {
        philosophers[idx].num = idx;
        philosophers[idx].state = THINKING;
#if RANDOM_EN
        philosophers[idx].seed = hall_sensor_read();
#else
        philosophers[idx].seed = 7369 + idx;
#endif /* RANDOM_EN */
    }

    for (uint32_t idx = 0; idx < N; ++idx)
    {
        ret = xTaskCreatePinnedToCore(task_philo, "philo_task", 5000,
                                      &philosophers[idx], 1,
                                      &philosophers[idx].h_task, app_cpu);

        assert(pdPASS == ret);
        assert(philosophers[idx].h_task != NULL);
    }

    ret = xTaskCreatePinnedToCore(task_loop, "loop_task", 2048,
                                  NULL, 1, NULL, app_cpu);
}
