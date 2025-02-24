# Bare-Metal RTOS Scheduler for ESP32

This project implements a simple bare-metal Real-Time Operating System (RTOS) scheduler for the ESP32 microcontroller. It supports multiple scheduling algorithms and provides basic task management, inter-task communication, and synchronization mechanisms.

---

## Features

1. **Task Scheduling**:
   - Supports multiple scheduling algorithms:
     - **Round Robin (RR)**: Tasks are executed in a cyclic order.
     - **First-Come-First-Served (FCFS)**: Tasks are executed in the order they are added.
     - **Priority Scheduling**: Tasks are executed based on their priority (lower value means higher priority).
     - **Preemptive Scheduling**: A timer interrupt preempts the current task to switch to a higher-priority task.

2. **Task Management**:
   - Add tasks with a function pointer, parameters, interval, and priority.
   - Remove tasks dynamically.

3. **Inter-Task Communication**:
   - **Queue**: A simple FIFO queue for passing data between tasks.
   - **Event Flag**: A flag to signal events between tasks.

4. **Synchronization**:
   - **Semaphore**: A counting semaphore for resource management.
   - **Mutex**: A binary mutex for critical section protection.

5. **Timer Interrupt**:
   - A hardware timer is used for preemptive scheduling, allowing higher-priority tasks to interrupt lower-priority ones.

---

## How It Works

### Task Structure
Each task is represented by a `task_t` structure:
- `func`: The function to execute.
- `param`: Parameters passed to the function.
- `interval_ms`: The interval at which the task should run.
- `last_run`: The last time the task was executed.
- `state`: The current state of the task (`TASK_READY`, `TASK_RUNNING`, etc.).
- `priority`: The priority of the task (used in priority and preemptive scheduling).

### Scheduling Algorithms
1. **Round Robin (RR)**:
   - Tasks are executed in a fixed order.
   - Each task runs for its specified interval before switching to the next task.

2. **First-Come-First-Served (FCFS)**:
   - Tasks are executed in the order they are added.
   - The first ready task is executed until it completes.

3. **Priority Scheduling**:
   - The task with the highest priority (lowest value) is executed.
   - If multiple tasks have the same priority, they are executed in the order they were added.

4. **Preemptive Scheduling**:
   - A timer interrupt periodically checks for higher-priority tasks.
   - If a higher-priority task is ready, it preempts the current task and starts executing.

### Inter-Task Communication
- **Queue**:
  - Tasks can push data into the queue using `queue_push`.
  - Tasks can pop data from the queue using `queue_pop`.

- **Event Flag**:
  - Tasks can set or clear an event flag using `event_flag_set` and `event_flag_clear`.
  - Tasks can check the event flag using `event_flag_check`.

### Synchronization
- **Semaphore**:
  - Tasks can wait for a semaphore using `semaphore_wait`.
  - Tasks can signal a semaphore using `semaphore_signal`.

- **Mutex**:
  - Tasks can lock a mutex using `mutex_lock`.
  - Tasks can unlock a mutex using `mutex_unlock`.

---

## Usage

### Adding Tasks
Tasks are added using the `scheduler_add_task` function:
```c
scheduler_add_task(producer_task, NULL, 1000, 2); // Add a task with 1000ms interval and priority 2
```

### Setting Up the Scheduler
The scheduler is configured using the scheduler_setup function:

```c
scheduler_setup(SCHEDULER_PRIORITY); // Use priority scheduling
```

### Running the Scheduler
The scheduler is executed in the main loop:

```c
while (1) {
    scheduler_run();
    esp_rom_delay_us(100 * 1000); // Small delay to avoid busy-waiting
}
```

### Example Tasks
- Producer Task: Produces data and pushes it into the queue.
- Consumer Task: Consumes data from the queue.
- Critical Task: Demonstrates mutex usage for critical section protection.
- Semaphore Task: Demonstrates semaphore usage for resource management

### Example Output
```bash
I (40521) Semaphore: Accessing shared resource
I (41121) Producer: Produced: 32
I (41221) Critical: In critical section
I (41821) Consumer: Consumed: 24
I (42121) Producer: Produced: 33
I (43021) Semaphore: Accessing shared resource
I (43621) Consumer: Consumed: 25
I (43721) Producer: Produced: 34
I (43821) Critical: In critical section
I (44721) Producer: Produced: 35
I (45121) Consumer: Consumed: 26
E (45281) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (45281) task_wdt:  - IDLE0 (CPU 0)
E (45281) task_wdt: Tasks currently running:
E (45281) task_wdt: CPU 0: main
E (45281) task_wdt: CPU 1: IDLE1
```

### Configuration
- Max Tasks: `MAX_TASKS` defines the maximum number of tasks (default: 5).
- Queue Size: `MAX_QUEUE_SIZE` defines the maximum size of the queue (default: 10).
- Timer Interval: The timer interval for preemptive scheduling can be adjusted in `timer_setup`.

### Dependencies
- ESP-IDF: The project uses ESP-IDF APIs for timers, logging, and delays.
- No FreeRTOS: This is a bare-metal implementation and does not depend on FreeRTOS.

---

## Bugs & Future Improvement 🐛
- solve idle CPU `CPU 1: IDLE1`
```
E (45281) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (45281) task_wdt:  - IDLE0 (CPU 0)
E (45281) task_wdt: Tasks currently running:
E (45281) task_wdt: CPU 0: main
E (45281) task_wdt: CPU 1: IDLE1
E (45281) task_wdt: Print CPU 0 (current core) backtrace


Backtrace: 0x400D4692:0x3FFB0FF0 0x400D4A54:0x3FFB1010 0x4008387D:0x3FFB1040 0x4000C050:0x3FFB3F80 0x40008544:0x3FFB3F90 0x400D11BD:0x3FFB3FB0 0x400E3A24:0x3FFB3FD0 0x400860C1:0x3FFB4000
```
- preemptive scheduler not working `scheduler_setup(SCHEDULER_PREEMPTIVE);`
```
I (281) main_task: Calling app_main()
Task scheduler example
Starting scheduler

abort() was called at PC 0x40082a83 on core 0


Backtrace: 0x40082355:0x3ffb0b00 0x400858cd:0x3ffb0b20 0x4008b609:0x3ffb0b40 0x40082a83:0x3ffb0bb0 0x40082bc1:0x3ffb0be0 0x40082c3a:0x3ffb0c00 0x400d8c3f:0x3ffb0c30 0x400d8281:0x3ffb0f50 0x400e40ad:0x3ffb0f80 0x4008b579:0x3ffb0fb0 0x400d0e91:0x3ffb1000 0x400810f5:0x3ffb1020 0x4008387d:0x3ffb1040 0x40008541:0x3ffb3f90 0x400d11bd:0x3ffb3fb0 0x400e3a24:0x3ffb3fd0 0x400860c1:0x3ffb4000




ELF file SHA256: 8d4fd7470

Rebooting...
```