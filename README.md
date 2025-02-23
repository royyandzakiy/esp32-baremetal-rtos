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