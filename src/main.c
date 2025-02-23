
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include <stdbool.h>
#include "esp_rom_sys.h" // For esp_rom_delay_us
#include "driver/timer.h" // For hardware timer interrupts

#define TASK_STACK_SIZE 1024
#define MAX_TASKS 5
#define MAX_QUEUE_SIZE 10
#define INT_MAX 999

// Task states
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_TERMINATED
} task_state_t;

// Task function pointer
typedef void (*task_func_t)(void *);

// Task control block
typedef struct {
    task_func_t func;
    void *param;
    uint32_t interval_ms;
    uint64_t last_run;
    task_state_t state;
    int priority; // Priority for scheduling
} task_t;

// Queue for inter-task communication
typedef struct {
    void *items[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} queue_t;

// Event flag
typedef struct {
    volatile bool flag;
} event_flag_t;

// Semaphore
typedef struct {
    volatile int count;
} semaphore_t;

// Mutex
typedef struct {
    volatile bool locked;
} mutex_t;

// Task list and count
task_t task_list[MAX_TASKS];
int task_count = 0;

// Global variables
queue_t task_queue = { .front = 0, .rear = 0, .size = 0 };
event_flag_t event_flag = { .flag = false };
semaphore_t semaphore;
mutex_t mutex = { .locked = false };

// Current running task (for preemptive scheduling)
volatile int current_task = -1;

// Scheduler type
typedef enum {
    SCHEDULER_RR,       // Round Robin
    SCHEDULER_FCFS,     // First-Come-First-Served
    SCHEDULER_PRIORITY, // Priority Scheduling
    SCHEDULER_PREEMPTIVE // Preemptive Scheduling
} scheduler_type_t;

scheduler_type_t scheduler_type = SCHEDULER_RR; // Default scheduler

// Timer group and timer index for preemptive scheduling
#define TIMER_GROUP TIMER_GROUP_0
#define TIMER_IDX TIMER_0

// Function prototypes
void queue_push(queue_t *queue, void *item);
void *queue_pop(queue_t *queue);
void event_flag_set(event_flag_t *flag);
void event_flag_clear(event_flag_t *flag);
bool event_flag_check(event_flag_t *flag);
void semaphore_init(semaphore_t *sem, int value);
void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
void scheduler_add_task(task_func_t func, void *param, uint32_t interval_ms, int priority);
void scheduler_remove_task(int index);
void scheduler_setup(scheduler_type_t type);
void scheduler_run(void);
void IRAM_ATTR timer_isr(void *arg);
void producer_task(void *param);
void consumer_task(void *param);
void critical_task(void *param);
void semaphore_task(void *param);
void app_main(void);

// Queue functions
void queue_push(queue_t *queue, void *item) {
    if (queue->size < MAX_QUEUE_SIZE) {
        queue->items[queue->rear] = item;
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        queue->size++;
    }
}

void *queue_pop(queue_t *queue) {
    if (queue->size > 0) {
        void *item = queue->items[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        queue->size--;
        return item;
    }
    return NULL;
}

// Event flag functions
void event_flag_set(event_flag_t *flag) {
    flag->flag = true;
}

void event_flag_clear(event_flag_t *flag) {
    flag->flag = false;
}

bool event_flag_check(event_flag_t *flag) {
    return flag->flag;
}

// Semaphore functions
void semaphore_init(semaphore_t *sem, int value) {
    sem->count = value;
}

void semaphore_wait(semaphore_t *sem) {
    while (sem->count <= 0);
    sem->count--;
}

void semaphore_signal(semaphore_t *sem) {
    sem->count++;
}

// Mutex functions
void mutex_lock(mutex_t *mutex) {
    while (__atomic_test_and_set(&mutex->locked, __ATOMIC_ACQUIRE));
}

void mutex_unlock(mutex_t *mutex) {
    __atomic_clear(&mutex->locked, __ATOMIC_RELEASE);
}

// Task management
void scheduler_add_task(task_func_t func, void *param, uint32_t interval_ms, int priority) {
    if (task_count < MAX_TASKS) {
        task_list[task_count].func = func;
        task_list[task_count].param = param;
        task_list[task_count].interval_ms = interval_ms;
        task_list[task_count].last_run = 0;
        task_list[task_count].state = TASK_READY;
        task_list[task_count].priority = priority;
        task_count++;
    } else {
        ESP_LOGW("Scheduler", "Max tasks reached");
    }
}

void scheduler_remove_task(int index) {
    if (index >= 0 && index < task_count) {
        task_list[index].state = TASK_TERMINATED;
    }
}

// Scheduler setup
void scheduler_setup(scheduler_type_t type) {
    scheduler_type = type;

    // Configure timer for preemptive scheduling
    if (type == SCHEDULER_PREEMPTIVE) {
        timer_config_t timer_config = {
            .divider = 80, // 1 MHz timer (80 MHz / 80)
            .counter_dir = TIMER_COUNT_UP,
            .counter_en = TIMER_PAUSE,
            .alarm_en = TIMER_ALARM_EN,
            .auto_reload = TIMER_AUTORELOAD_EN
        };
        timer_init(TIMER_GROUP, TIMER_IDX, &timer_config);
        timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
        timer_set_alarm_value(TIMER_GROUP, TIMER_IDX, 1000000); // 1 second interval
        timer_enable_intr(TIMER_GROUP, TIMER_IDX);
        timer_isr_register(TIMER_GROUP, TIMER_IDX, timer_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
        timer_start(TIMER_GROUP, TIMER_IDX);
    }
}

// Scheduler run function
void scheduler_run(void) {
    uint64_t now = esp_timer_get_time() / 1000; // Get time in milliseconds

    switch (scheduler_type) {
        case SCHEDULER_RR: {
            static int current_task = 0;
            if (task_count == 0) return;

            if (task_list[current_task].state != TASK_TERMINATED &&
                now - task_list[current_task].last_run >= task_list[current_task].interval_ms) {
                task_list[current_task].state = TASK_RUNNING;
                task_list[current_task].func(task_list[current_task].param);
                task_list[current_task].last_run = now;
                task_list[current_task].state = TASK_READY;
            }
            current_task = (current_task + 1) % task_count;
            break;
        }

        case SCHEDULER_FCFS: {
            for (int i = 0; i < task_count; i++) {
                if (task_list[i].state != TASK_TERMINATED &&
                    now - task_list[i].last_run >= task_list[i].interval_ms) {
                    task_list[i].state = TASK_RUNNING;
                    task_list[i].func(task_list[i].param);
                    task_list[i].last_run = now;
                    task_list[i].state = TASK_READY;
                    break; // Run the first ready task and exit
                }
            }
            break;
        }

        case SCHEDULER_PRIORITY: {
            int highest_priority_task = -1;
            int highest_priority = INT_MAX;

            for (int i = 0; i < task_count; i++) {
                if (task_list[i].state != TASK_TERMINATED &&
                    now - task_list[i].last_run >= task_list[i].interval_ms &&
                    task_list[i].priority < highest_priority) {
                    highest_priority = task_list[i].priority;
                    highest_priority_task = i;
                }
            }

            if (highest_priority_task != -1) {
                task_list[highest_priority_task].state = TASK_RUNNING;
                task_list[highest_priority_task].func(task_list[highest_priority_task].param);
                task_list[highest_priority_task].last_run = now;
                task_list[highest_priority_task].state = TASK_READY;
            }
            break;
        }

        case SCHEDULER_PREEMPTIVE: {
            // Preemptive scheduling is handled in the timer ISR
            break;
        }

        default:
            break;
    }
}

// Timer ISR for preemptive scheduling
void IRAM_ATTR timer_isr(void *arg) {
    // Check for higher-priority tasks
    int highest_priority_task = -1;
    int highest_priority = INT_MAX;
    uint64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < task_count; i++) {
        if (task_list[i].state != TASK_TERMINATED &&
            now - task_list[i].last_run >= task_list[i].interval_ms &&
            task_list[i].priority < highest_priority) {
            highest_priority = task_list[i].priority;
            highest_priority_task = i;
        }
    }

    // If a higher-priority task is found, switch to it
    if (highest_priority_task != -1 && highest_priority_task != current_task) {
        if (current_task != -1) {
            task_list[current_task].state = TASK_READY; // Put the current task back to ready state
        }
        current_task = highest_priority_task;
        task_list[current_task].state = TASK_RUNNING;
        task_list[current_task].func(task_list[current_task].param);
        task_list[current_task].last_run = now;
        task_list[current_task].state = TASK_READY;
    }

    // Clear the interrupt
    timer_group_clr_intr_status_in_isr(TIMER_GROUP, TIMER_IDX);
    timer_group_enable_alarm_in_isr(TIMER_GROUP, TIMER_IDX);
}

// Task functions
void producer_task(void *param) {
    static int data = 0;
    queue_push(&task_queue, (void *)(uintptr_t)data);
    ESP_LOGI("Producer", "Produced: %d", data);
    data++;
    event_flag_set(&event_flag);
}

void consumer_task(void *param) {
    if (event_flag_check(&event_flag)) {
        int data = (int)(uintptr_t)queue_pop(&task_queue);
        ESP_LOGI("Consumer", "Consumed: %d", data);
        event_flag_clear(&event_flag);
    }
}

void critical_task(void *param) {
    mutex_lock(&mutex);
    ESP_LOGI("Critical", "In critical section");
    esp_rom_delay_us(500 * 1000); // Delay for 500ms
    mutex_unlock(&mutex);
}

void semaphore_task(void *param) {
    semaphore_wait(&semaphore);
    ESP_LOGI("Semaphore", "Accessing shared resource");
    esp_rom_delay_us(500 * 1000); // Delay for 500ms
    semaphore_signal(&semaphore);
}

// Main application
void app_main(void) {
    printf("Task scheduler example\n");
    semaphore_init(&semaphore, 1);

    // Add tasks with priorities
    scheduler_add_task(producer_task, NULL, 1000, 2);
    scheduler_add_task(consumer_task, NULL, 1500, 1);
    scheduler_add_task(critical_task, NULL, 2000, 3);
    scheduler_add_task(semaphore_task, NULL, 2500, 4);

    // Set up the scheduler (choose the type here)
    // scheduler_setup(SCHEDULER_PREEMPTIVE);
    scheduler_setup(SCHEDULER_PRIORITY);


    printf("Starting scheduler\n");

    // Main loop
    while (1) {
        scheduler_run();
        esp_rom_delay_us(100 * 1000); // Small delay to avoid busy-waiting
    }
}