# 协程调度器

## Scheduler

### 概述

`Scheduler` 是 BiShengC 语言提供的负责协调和管理协程的数据结构，它使用了协程机制来支持高并发的异步编程，主要作用是将可执行的任务分配给可用线程来执行，以最大程度地利用计算机资源。其使用示例如下：

```c
#include <unistd.h>
#include "scheduler.hbs" // 引入头文件

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
    // 模拟 IO 操作，读取学生名单
    printf("name: %s, age: %d, score: %d\n", nameList[i].name, nameList[i].age, nameList[i].score);

    atomic_fetch_sub(&g_student_num, 1);
    // 任务完成，销毁调度器
    if (atomic_load(&g_student_num) == 0) {
        struct Scheduler::destroy();
    }
}

int main() {
    // 初始化，创建 4 个线程
    struct Scheduler::init(4);
    for (int i = 0; i < g_student_num; i++) {
        // 将任务放入执行队列中
        struct Scheduler::spawn(readName(i));
    }
    // 执行调度器
    struct Scheduler::run();
    return 0;
}
```

输出结果如下：

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

调度器是一个用于管理和调度任务的工具，`init`方法是在创建调度器对象时调用的，主要用于初始化调度器的一些属性。`spawn` 方法用于创建一个异步任务，并将任务添加到任务队列中**等待执行**。`run` 方法是调度器的核心方法，用于**执行异步任务**。需要注意的一点是，只有当我们调用 `run` 方法时，调度器才会真正的执行任务。`destroy` 方法是在我们不再需要使用调度器时调用的，主要用于清理调度器的一些资源和释放占用的系统资源。

另外，从这个输出结果来看，输出顺序并不是按照数组本身的顺序，并且多次执行输出顺序也可能不同，所以这比较适用于不需要严格要求执行顺序的任务。

### 头文件

```c
#include "scheduler.hbs"
```

### API

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `void struct Scheduler::init(unsigned int threadCount)` | 用于初始化调度器，不可多次重复初始化，参数为用户需要创建的线程数 | struct Scheduler::init(4); |
| `struct Task * struct Scheduler::spawn(_Trait Future<struct Void> * future)` | 创建异步任务，并将任务放入执行队列中等待执行（目前仅接受返回类型是void的任务）。 | _Async void  taskFunc(int i) { <br/>    while(i < 10000) { <br/>        i = (i * 2) + 3 ; <br/>    }<br/>    printf("result: %d\n", i);<br/>}<br>struct Scheduler::spawn(taskFunc(0)); |
| `void struct Scheduler::run()` | 执行通过 spawn 函数创建的异步任务 | struct Scheduler::run(); |
| `void struct Scheduler::destroy()` | 销毁调度器，释放资源 | struct Scheduler::destroy(); |

具体数据结构介绍如下：

`struct Scheduler`：表示调度器，包括任务队列和线程池。通过 `struct Scheduler::init`方法创建并初始，但由于一个进程只能有一个 Scheduler，所以不能重复初始化。当然，也不允许未初始化就直接使用，在调用 `struct Scheduler::spawn` 或  `struct Scheduler::run` 方法前，必须调 `struct Scheduler::init`。如果不再需要调度器了，可以通过 `struct Scheduler::destroy` 方法进行销毁（如果不销毁，进程无法终止）。

`strcut Task`: 表示异步任务，包括任务的状态和执行上下文。通过 `struct Scheduler::spawn` 方法进行创建，创建完成后会将其放入执行队列，但不会立马执行。只有调用了 `struct Scheduler::run` 方法才会真正的执行队列中的任务。

另外，我们注意到 `struct Scheduler::spawn` 方法的入参是 `_Trait Future<struct Void> *` 类型（`_Trait Future` 的定义可参考无栈协程章节），所以我们除了可以传入返回类型是 `void` 的 _Async 函数调用，也可以传显式返回 `_Trait Futrue<struct Void> *`  类型的函数调用，具体使用如下：

```c
#include "scheduler.hbs"

atomic_int g_task_num = 10;

struct _Futurework {
    int a;
    int __future_state;
};

// 必须实现 poll 和 free 两个函数
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
