# Coroutine Scheduler

## Scheduler

### Overview

`Scheduler` is the data structure provided by the BiSheng C language responsible for coordinating and managing coroutines. It uses the coroutine mechanism to support high-concurrency asynchronous programming. Its main purpose is to assign executable tasks to available threads for execution, so as to make the most of computer resources. Its usage example is as follows:

```c
#include <unistd.h>
#include "scheduler.hbs" // include the header file

atomic_int g_student_num = 10;

struct Student {
    char * name;
    int age;
    int score;
};

struct Student nameList[] =
{
    {"Mary", 12, 90},
    {"John", 13, 95},
    {"Michael", 12, 93},
    {"Emily", 11, 92},
    {"James", 12, 86},
    {"Emma", 12, 95},
    {"William", 11, 90},
    {"Jessica", 13, 80},
    {"Lily", 12, 92},
    {"Sarah", 12, 88}
};

_Async void readName(int i) {
    // simulate an IO operation, reading the student roster
    printf("name: %s, age: %d, score: %d\n", nameList[i].name, nameList[i].age, nameList[i].score);

    atomic_fetch_sub(&g_student_num, 1);
    // task completed, destroy the scheduler
    if (atomic_load(&g_student_num) == 0) {
        struct Scheduler::destroy();
    }
}

int main() {
    // initialize, creating 4 threads
    struct Scheduler::init(4);
    for (int i = 0; i < g_student_num; i++) {
        // put the task into the execution queue
        struct Scheduler::spawn(readName(i));
    }
    // run the scheduler
    struct Scheduler::run();
    return 0;
}
```

The output is as follows:

```text
name: Mary, age: 12, score: 90
name: Michael, age: 12, score: 93
name: Emily, age: 11, score: 92
name: James, age: 12, score: 86
name: Emma, age: 12, score: 95
name: William, age: 11, score: 90
name: Jessica, age: 13, score: 80
name: Lily, age: 12, score: 92
name: Sarah, age: 12, score: 88
name: John, age: 13, score: 95
```

The scheduler is a tool for managing and scheduling tasks. The `init` method is called when creating the scheduler object, and is mainly used to initialize some of the scheduler's attributes. The `spawn` method is used to create an asynchronous task and add it to the task queue, **waiting to be executed**. The `run` method is the core method of the scheduler, used to **execute the asynchronous tasks**. One thing to note is that the scheduler only actually executes tasks when we call the `run` method. The `destroy` method is called when we no longer need to use the scheduler; it is mainly used to clean up some of the scheduler's resources and release the system resources it occupies.

In addition, judging from this output, the output order is not the same as the order of the array itself, and the output order may also differ across multiple runs. So this is more suitable for tasks that do not strictly require a specific execution order.

### Header File

```c
#include "scheduler.hbs"
```

### API

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `void struct Scheduler::init(unsigned int threadCount)` | Used to initialize the scheduler; it cannot be initialized repeatedly. The parameter is the number of threads the user wants to create. | struct Scheduler::init(4); |
| `struct Task * struct Scheduler::spawn(_Trait Future<struct Void> * future)` | Creates an asynchronous task and puts it into the execution queue to wait for execution (currently only accepts tasks whose return type is void). | _Async void  taskFunc(int i) { <br/>    while(i < 10000) { <br/>        i = (i * 2) + 3 ; <br/>    }<br/>    printf("result: %d\n", i);<br/>}<br>struct Scheduler::spawn(taskFunc(0)); |
| `void struct Scheduler::run()` | Executes the asynchronous tasks created via the spawn function | struct Scheduler::run(); |
| `void struct Scheduler::destroy()` | Destroys the scheduler and releases resources | struct Scheduler::destroy(); |

The specific data structures are introduced as follows:

`struct Scheduler`: Represents the scheduler, including the task queue and thread pool. It is created and initialized via the `struct Scheduler::init` method, but because a process can have only one Scheduler, it cannot be initialized repeatedly. Of course, it is also not allowed to be used directly without initialization; before calling the `struct Scheduler::spawn` or `struct Scheduler::run` method, `struct Scheduler::init` must be called. If the scheduler is no longer needed, it can be destroyed via the `struct Scheduler::destroy` method (if it is not destroyed, the process cannot terminate).

`strcut Task`: Represents an asynchronous task, including the task's state and execution context. It is created via the `struct Scheduler::spawn` method; after creation, it is placed into the execution queue but is not executed immediately. The tasks in the queue are only actually executed once the `struct Scheduler::run` method has been called.

In addition, we note that the parameter of the `struct Scheduler::spawn` method is of type `_Trait Future<struct Void> *` (for the definition of `_Trait Future`, refer to the Stackless Coroutines chapter). So, besides being able to pass in a call to an _Async function whose return type is `void`, we can also pass in a call to a function that explicitly returns the `_Trait Futrue<struct Void> *` type. The specific usage is as follows:

```c
#include "scheduler.hbs"

atomic_int g_task_num = 10;

struct _Futurework {
    int a;
    int __future_state;
};

// the two functions poll and free must be implemented
struct PollResult<struct Void> struct _Futurework::poll(struct _Futurework *this) {
    switch (this->__future_state) {
      case 0:
        goto __L0;
    }
  __L0:
    ;
    printf("The %d-th calculation begins\n", this->a);
    while (this->a < 100000000)
        {
            this->a++;
        }

    atomic_fetch_sub(&g_task_num, 1);
    if (atomic_load(&g_task_num) == 0) {
        struct Scheduler::destroy();
    }

    this->__future_state = -1;
    struct Void __RES_RETURN = (struct Void){};
    return struct PollResult<struct Void>::completed(__RES_RETURN);
}

void struct _Futurework::free(struct _Futurework *this) {
    if (this != 0) {
        free((void *)this);
        this = (struct _Futurework *)(void *)0;
    }
}

_Impl _Trait Future<struct Void> for struct _Futurework;

_Trait Future<struct Void>* work(int a)     {
  struct _Futurework* ptr = malloc(sizeof(struct _Futurework));
  ptr->a = a;
  ptr->__future_state = 0;
  return ptr;
}

int main() {
    struct Scheduler::init(4);
    for (int i = 0; i < 10; i++) {
        struct Scheduler::spawn(work(i));
    }
    struct Scheduler::run();
    return 0;
}
```
