# 智能指针

## 概述

BiShengC 的所有权机制要求一个值只能有一个所有者，然而，我们考虑以下场景：

1. 在图数据结构中，多个边可能指向相同的节点，这个节点为所有指向它的边所共有，该节点直到没有边指向它时，才能被释放清理
2. 当我们希望在堆上分配一些内存供程序的多个部分读取，而且无法在编译时确定程序的哪一部分会最后结束使用它的时候，这部分内存直到最后一个部分使用结束时，才能被释放清理

为解决此类问题，需要允许一个值在同一时刻拥有多个所有者，因此 BSC **在标准库中提供 `Rc<T>` 类型来实现所有权的共享，它是一种智能指针，指向被共享的数据**。为提高 Rc 的安全性，BSC 同时提供了 `Weak<T>` 来避免循环引用造成的内存泄漏问题，也提供了 `Cell<T>` 和 `RefCell<T>` 与 Rc 组合使用来实现 Rc 指向数据的可变性。

## 使用 `Rc<T>` 共享数据

Rc 是引用计数（reference counting）的缩写，通过记录数据被引用的次数，来确定数据是否正在被使用，引用的数量就相当于所有者的数量。**如果引用计数变为 0，就意味着被引用数据可以被清理，此时 Rc 会自动调用数据的析构函数，避免内存泄漏。**

注意：在使用 `Rc<T>` 时，需要保证 T 是 copy 语义的类型或 _Owned struct 类型，否则会编译期报错，因为对于其它类型，编译器无法知道应该如何清理内存。

`Rc<T>` 的使用方法是：

1. 调用 new 方法来创建 Rc，该方法会分配堆内存，并将所要引用的数据存储在堆上
2. 调用 clone 方法可以从已有 Rc 得到另一个 Rc，该方法不执行深拷贝，仅仅创建另一个指向被引用数据的指针
3. 调用 deref 方法可以获取 Rc 所指向数据的不可变借用

我们通过以下的例子来学习 Rc 的使用：
假如一个班上有若干学生，他们共享一份成绩单，这份成绩记录文件就是需要被共享的数据资源，现在我们读取该文件来获取 1 号学生和 2 号学生的成绩：

```C
#include "rc.hbs"

_Owned struct ScoreRecord {  //学生成绩记录文件
    FILE *fp;
    ~ScoreRecord(This this) {
        fclose(this.fp);
    }
};

_Owned struct Student {
    unsigned student_id;    //学生id
    Rc<ScoreRecord> scores; //假设学生成绩记录文件为所有学生所共有
};

// 打开文件
ScoreRecord ScoreRecord::open(String filename);
// 读取文件第 id 行内容
String ScoreRecord::read(const This *_Borrow this, unsigned id);

// 获取 id 号学生的成绩
String Student::get_score(const This *_Borrow this) {
    const ScoreRecord *_Borrow scores = this->scores.deref(); 
    return scores->read(this->id);
}

int main() {
    ScoreRecord scores = ScoreRecord::open(String::from("scores.txt"));

    // student1 和 student2 共同拥有成绩文件的所有权
    Rc<ScoreRecord> rc1 = Rc<ScoreRecord>::new(scores);
    Rc<ScoreRecord> rc2 = rc1.clone()；
    Student student1 = { .id = 1, .scores = rc1 };
    Student student2 = { .id = 2, .scores = rc2 };
    // 此时共享文件的引用计数为 2，因为它有 2 个所有者

    print("学生1的成绩为：" + student1.get_score());
    print("学生2的成绩为：" + student2.get_score());

    return 0;
}   // 共享文件被使用完毕，引用计数变为 0，此时 Rc 会自动调用 ～ScoreRecord 来关闭文件
```

## 引用计数

让我们通过以下案例来观察 `Rc<T>` 在创建、克隆和走出作用域时引用计数的变化。

```C
int main() {
    Rc<int> rc1 = Rc<int>::new(5);
    printf("ref count after creating rc1 = %d\n", rc1.strong_ref_count());
    Rc<int> rc2 = rc1.clone();
    printf("ref count after creating rc2 = %d\n", rc1.strong_ref_count());
    {
        Rc<int> rc3 = rc1.clone();
        printf("ref count after creating rc3 = %d\n", rc1.strong_ref_count());
    }
    printf("ref count after rc3 goes out of scope = %d\n", rc1.strong_ref_count());
    return 0;
}
```

这段代码会打印出：

```text
ref count after creating rc1 = 1
ref count after creating rc2 = 2
ref count after creating rc3 = 3
ref count after rc3 goes out of scope = 2
```

我们能够看到 rc1 的初始引用计数为 1，随着每次调用 clone，计数会增加 1。当 rc3 离开作用域时，计数减 1。不必像调用 clone 增加引用计数那样调用一个函数来减少计数，因为 `Rc<T>` 析构函数的实现可以保证当一个 Rc 离开作用域时自动减少引用计数。

从这个例子我们所不能看到的是，在 ref_count 函数的结尾处，随着 rc1、rc2 离开作用域，计数会降为 0，其指向的数据也将被清理，这一清理操作也是通过 `Rc<T>` 的析构函数来自动实现的。

## `Cell<T>`/`RefCell<T>` 与内部可变性

通过 deref 方法可以获取 Rc 所指向数据的不可变借用，这允许在程序的多个部分之间只读地共享数据。如果 `Rc<T>` 也允许多个可变借用，会违反 BSC 的借用规则：不能同时存在对同一数据的多个可变借用。但是在实际应用场景，修改数据是不可避免的。在这一部分，我们将讨论内部可变性模式和 Cell/RefCell 类型，它们**可以与 Rc 结合使用来修改 Rc 所指向的数据**。

BSC 编译器会在编译时对借用规则进行严格的检查，它可能会拒绝一个正确的程序，这往往会让用户痛苦不堪。因此在标准库中提供了 `Cell<T>` 和 `RefCell<T>` 来实现内部可变性：在拥有不可变借用的同时修改目标数据！对于正常的代码实现来说，这个是不可能做到的（要么一个可变借用，要么多个不可变借用）。

### `Cell<T>`

`Cell<T>` 用于获取它内部包裹的值的拷贝或直接修改内部值，适用于 Copy 语义的类型，例如 int、float 等基本数据类型或 struct 类型。

```C
void cell_example() {
    Cell<int> c = Cell<int>::new(5);
    printf("Value in cell is: %d\n", c.get()); // get 方法可以获取 cell 内部包裹的值的拷贝
    c.set(10);   // set 方法可以直接修改 cell 内部包裹的值
    printf("Value in cell is: %d\n", c.get());
}
```

### `RefCell<T>`

`RefCell<T>` 用于获取它内部包裹的值的可变或不可变借用，它实际上**并没有解决可变借用和不可变借用可以共存的问题，只是将报错从编译期推迟到运行时 abort**。

通过使用 `RefCell<T>` 的 borrow_mut 和 borrow_immut 方法，来尝试在运行时获取借用，如果借用成功，返回 `RefMut<T>` 或 `RefImmut<T>` 类型的值，否则会导致运行时 abort。规则是：

1. borrow_mut 方法用于获取 `RefMut<T>`，如果当前作用域内存在**任何其它借用**，则借用失败
2. borrow_immut 方法用于获取 `RefImmut<T>`，如果当前作用域存在**任何其它可变借用**，则借用失败

`RefMut<T>` 和 `RefImmut<T>` 是 `RefCell<T>` 的辅助数据结构，它们的 deref 方法可以分别获取 RefCell 内部值的可变和不可变借用。

```C
void refcell_example() {
    RefCell<int> c = RefCell<int>::new(5);

    // 通过 RefCell 的 borrow_mut 方法获得 RefMut：
    RefMut<int> ref_mut_c1 = c.borrow_mut();

    // 通过 RefMut 的 deref 方法获得 RefCell 内部值的可变借用：
    int* _Borrow mut_p1 = ref_mut_c.deref();

    // 通过可变借用修改内部值：
    *mut_p1 = 10;  

    // 再次尝试获取可变借用
    RefMut<int> ref_mut_c2 = c.borrow_mut(); //运行时abort，因为同时存在两个可变借用
}
```

## Rc 和 RefCell 相结合来实现共享数据的修改

上面的读取学生成绩的案例我们只对数据进行了读操作，有了 RefCell，我们可以实现对共享数据的修改，以下例子实现了对成绩数据的读取和更新：

```C
#include "rc.hbs"
#include "cell.hbs"

_Owned struct ScoreRecord {  //学生成绩记录文件
    FILE *fp;
    ~ScoreRecord(This this) {
        fclose(this.fp);
    }
};

_Owned struct Student {
    unsigned student_id;
    Rc<RefCell<ScoreRecord>> scores; //将需要被修改的共享数据用 RefCell 进行包装
};

// 打开文件
ScoreRecord ScoreRecord::open(String filename);
// 读取文件第 id 行内容
String ScoreRecord::read(const This *_Borrow this, unsigned id);
// 更新文件第 id 行内容
void ScoreRecord::update(This *_Borrow this, unsigned id, String new);

// 获取 id 号学生的成绩
String Student::get_score(const This *_Borrow this) {
    // RefCell 的 borrow_immut 方法获取不可变借用
    RefImmut<ScoreRecord> ref_scores = this->scores.deref()->borrow_immut(); 
    const ScoreRecord *_Borrow scores = ref_scores.deref();
    return scores->read(this->id);
}

// 更新 id 号学生的成绩
String Student::update_score(const This *_Borrow this, String new_score) {
    // RefCell 的 borrow_mut 方法获取可变借用
    RefMut<ScoreRecord> ref_scores = this->scores.deref()->borrow_mut(); 
    ScoreRecord *_Borrow scores = ref_scores.deref();
    return scores->update(this->id, new_score);
}

int main() {
    ScoreRecord scores = ScoreRecord::open(String::from("scores.txt"));

    Rc<RefCell<ScoreRecord>> rc1 = Rc<RefCell<ScoreRecord>>::new(RefCell<ScoreRecord>::new(scores));
    Rc<RefCell<ScoreRecord>> rc2 = rc1.clone()；
    Student student1 = { .id = 1, .scores = rc1 };
    Student student2 = { .id = 2, .scores = rc2 };

    student1.update_score(String::from("100"));
    student2.update_score(String::from("99"));

    print("学生1的成绩为：" + student1.get_score());
    print("学生2的成绩为：" + student2.get_score());

    return 0;
}
```

## `Weak<T>` 解决循环引用问题

尽管 BSC 有着完善的内存安全保障机制，但是不代表不会产生内存泄漏。一个典型的例子就是同时使用 `Rc<T>` 和  `RefCell<T>` 创建循环引用，由于引用计数无法被归零，因此 `Rc<T>` 指向的数据也就不会被释放清理。

### 制造循环引用

```C
#include "rc.hbs"
#include "cell.hbs"

_Owned struct B;

_Owned struct A {
_Public:
    int value;
    RefCell<Rc<B>> c;
    ~A(A this) {
        printf("A is destructed, value = %d\n", this.value);
    }
};

_Owned struct B {
_Public:
    int value;
    RefCell<Rc<A>> d;
    ~B(B this) {
        printf("B is destructed, value = %d\n", this.value);
    }
};

int main() {
    A a = { .value = 5 };
    Rc<A> ap = Rc<A>::new(a);      //ap 拥有 a

    B b = { .value = 10 };
    RefMut<Rc<A>> rm_b = b.d.borrow_mut();
    *(rm_b.deref()) = ap.clone();  //通过 ap 的克隆，此处 b.d 拥有了 a
    Rc<B> bp = Rc<B>::new(b);      //bp 拥有 b

    RefMut<Rc<B>> rm_a = ap.deref()->c.borrow_mut();
    *(rm_a.deref()) = bp.clone();  //通过 bp 的克隆，此处 a.c 拥有了 b

    printf("ref count of a : %d\n", ap.strong_ref_count());
    printf("ref count of b : %d\n", bp.strong_ref_count());

    return 0;
}
```

这段代码会打印出：

```text
ref count of a : 2
ref count of b : 2
```

在函数结束前，a 和 b 的引用计数都为 2，其中 a 为 ap 和 b.d 所共有，b 为 bp 和 a.c 所共有。

在函数结束处：

1. 对 a 来说，ap 走出作用域后 a 的引用计数由 2 变为 1，只有当 b.d 析构会使 a 的引用计数减为 0 从而触发 a 的析构
2. 对 b 来说，bp 走出作用域后 b 的引用计数由 2 变为 1，只有当 a.c 析构会使 b 的引用计数减为 0 从而触发 b 的析构

a 的析构需要 b 先被析构，而 b 的析构需要 a 先被析构，结果就是 a 和 b 都没有被析构，内存泄漏了！

### 使用 `Weak<T>` 解决循环引用

为解决这一问题，BSC 引入 `Weak<T>` 类型，与 Rc 持有所有权不同，Weak 不持有所有权，它仅仅保存一份指向数据的弱引用，**建立弱引用不会增加引用计数，它也无法阻止所引用的数据被清理掉**。

`Weak<T>` 的使用方法是：

1. 调用 new 方法，通过一个已有的 `Rc<T>` 创建 `Weak<T>`
2. 无法通过 `Weak<T>` 直接访问被引用数据，如果想要访问数据，需要将 Weak 升级为 Rc，这通过 Weak 的 upgrade 方法实现，该方法返回一个类型为 `Option<Rc<T>>` 的值，如果被引用数据已经被清理释放，则 Option 的值是 None，也就是说，Weak 本身不对被引用数据的存在性做任何担保。

### 循环引用场景1

我们以上一小节中 a 和 b 循环引用导致内存泄漏的例子来学习`Weak<T>`的使用：

```C
#include "rc.hbs"
#include "cell.hbs"

_Owned struct B;

_Owned struct A {
_Public:
    int value;
    RefCell<Rc<B>> c;     //A 通过 Rc 引用 B
    ~A(A this) {
        printf("A is destructed, value = %d\n", this.value);
    }
};

_Owned struct B {
_Public:
    int value;
    RefCell<Weak<A>> d;  //B 通过 Weak 引用 A
    ~B(B this) {
        printf("B is destructed, value = %d\n", this.value);
    }
};

int main() {
    A a = { .value = 5 };
    Rc<A> ap = Rc<A>::new(a);           //ap 拥有 a

    B b = { .value = 10 };
    RefMut<Weak<A>> rm_b = b.d.borrow_mut();
    *(rm_b.deref()) = Weak<A>::new(ap); //通过 ap 创建弱引用，此处 b.d 弱引用 a
    Rc<B> bp = Rc<B>::new(b);           //bp 拥有 b

    RefMut<Rc<B>> rm_a = ap.deref()->c.borrow_mut();
    *(rm_a.deref()) = bp.clone();       //通过 bp 的克隆，此处 a.c 拥有了 b

    printf("ref count of a : %d\n", ap.strong_ref_count());
    printf("ref count of b : %d\n", bp.strong_ref_count());

    return 0;
}
```

这段代码会打印出：

```text
ref count of a : 1
ref count of b : 2
B is destructed, value = 10
A is destructed, value = 5
```

通过打印结果，我们看到 a 和 b 都被析构了，分析这段代码：

1. 在函数结束前，a 为 ap 所拥有，引用计数为 1，b 为 bp 和 a.c 所共有，引用计数为 2；
2. 在函数结束处，ap 走出作用域后 a 的引用计数由 1 变为 0，从而触发 a 的析构，因为 a.c 拥有 b，随着 a 的析构，b 的引用计数由 2 减为 1；随后 bp 走出作用域后 b 的引用计数由 1 变为 0，从而触发 b 的析构。

### 循环引用场景2

Weak 常常被应用于父子循环引用关系中，做法是：让父节点通过 Rc 来引用子节点，然后让子节点通过 Weak 来引用父节点。接下来我们构造一个简单的 tree，它有 1 个 root 节点和 2 个 leaf 节点：

```C
_Owned struct Node {
_Public:
    int value;
    RefCell<Weak<Node>> parent;      //子结点通过 Weak 引用父节点
    RefCell<Vec<Rc<Node>>> children; //父结点通过 Rc 引用子节点

    ~Node(Node this) {
        printf("node %d is destructed\n", this.value);
    }
};

void tree() {
    // 构造叶子节点
    Node temp_leaf1 = { .value = 5, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    Node temp_leaf2 = { .value = 6, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    Rc<Node> leaf1 = Rc<Node>::new(temp_leaf1);
    Rc<Node> leaf2 = Rc<Node>::new(temp_leaf2);

    printf("ref count of leaf1 after init : %d\n", leaf1.strong_ref_count());
    printf("ref count of leaf2 after init : %d\n", leaf2.strong_ref_count());

    // 构造父节点，并让父结点通过 Rc 引用子节点
    Node temp_root = { .value = 10, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    RefMut<Vec<Rc<Node>>> rm_root_children = temp_root.children.borrow_mut();
    rm_root_children.deref()->push(leaf1.clone());
    rm_root_children.deref()->push(leaf2.clone());
    Rc<Node> root = Rc<Node>::new(temp_root);

    printf("ref count of root after init : %d\n", root.strong_ref_count());
    printf("ref count of leaf1 after root points to it : %d\n", leaf1.strong_ref_count());
    printf("ref count of leaf2 after root points to it : %d\n", leaf2.strong_ref_count());

    // 子结点通过 Weak 引用父节点
    RefMut<Weak<Node>> rm_leaf1_parent = leaf1.deref()->parent.borrow_mut();
    *(rm_leaf1_parent.deref()) = Weak<Node>::new(&_Const root);
    RefMut<Weak<Node>> rm_leaf2_parent = leaf2.deref()->parent.borrow_mut();
    *(rm_leaf2_parent.deref()) = Weak<Node>::new(&_Const root);

    printf("ref count of root after leaf1 and leaf2 weakly points to it : %d\n", root.strong_ref_count());
}
```

这段代码会打印出：

```text
ref count of leaf1 after init : 1
ref count of leaf2 after init : 1
ref count of root after init : 1
ref count of leaf1 after root points to it : 2
ref count of leaf2 after root points to it : 2
ref count of root after leaf1 and leaf2 weakly points to it : 1
node 5 is destructed
node 6 is destructed
node 10 is destructed
```

通过打印结果可以看到所有的节点数据都被清理释放。

## 相关 API

### `Rc<T>` 对外接口

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe Rc<T> Rc<T>::new(T data)` | 构造一个 Rc，开辟堆内存存储 data | `Rc<int> rc = Rc<int>::new(5);` |
| `_Safe Rc<T> Rc<T>::clone(const This * _Borrow this)` | 通过一个已有 Rc，克隆出另一个 Rc，会使引用计数加 1 | `Rc<int> rc2 = rc.clone();` |
| `_Safe unsigned Rc<T>::strong_ref_count(const This * _Borrow this)` | 获取强引用计数 | `unsigned count = rc.strong_ref_count();` |
| `_Safe unsigned Rc<T>::weak_ref_count(const This * _Borrow this)` | 获取弱引用计数 | `unsigned count = rc.weak_ref_count();` |
| `_Safe const T * _Borrow Rc<T>::deref(const This * _Borrow this)` | 获取 Rc 指向数据的不可变借用 | `const int * _Borrow data = rc.deref();` |

### `Weak<T>` 对外接口

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe Weak<T> Weak<T>::new(const Rc<T> * _Borrow rc)` | 通过一个已有 Rc，构造出一个 Weak | `Weak<int> w = Weak<int>::new(&_Const rc);`|
| `_Safe Weak<T> Weak<T>::clone(const This * _Borrow this)` | 通过一个已有 Weak，克隆出另一个 Weak | `Weak<int> w2 = w.clone();`|
| `_Safe unsigned Weak<T>::strong_ref_count(const This * _Borrow this)` | 获取强引用计数 | `unsigned count = w.strong_ref_count();`|
| `_Safe unsigned Weak<T>::weak_ref_count(const This * _Borrow this)` | 获取弱引用计数 | `unsigned count = w.weak_ref_count();`|
| `_Safe Option<Rc<T>> Weak<T>::upgrade(const This * _Borrow this)` | 将 Weak 转化为对应的 Rc，转化失败时返回 None | `Option<Rc<int>> rc = w.upgrade();`|

### `Cell<T>` 对外接口

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe Cell<T> Cell<T>::new(T value)` | 用所给 value 构造 Cell | `Cell<int> c = Cell<int>::new(5);` |
| `_Safe T Cell<T>::get(const This * _Borrow this)` | 获得 Cell 内部值的拷贝 | `int a = c.get();` |
| `_Safe void Cell<T>::set(const This * _Borrow this, T val)` | 将 Cell 内部值替换为 val | `c.set(10);` |

### `RefCell<T>` 对外接口

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe RefCell<T> RefCell<T>::new(T value)` | 用所给 value 构造 RefCell | `RefCell<int> c = RefCell<int>::new(5);`|
| `_Safe RefImmut<T> RefCell<T>::borrow_immut(const This * _Borrow this)` | 获得 RefCell 内部值的`RefImmut<T>`，失败会导致 abort | `RefImmut<int> ref = c.borrow_immut();`|
| `_Safe Option<RefImmut<T>> RefCell<T>::try_borrow_immut(const This * _Borrow this)` | 获得 RefCell 内部值的`RefImmut<T>`，失败会返回 None | `Option<RefImmut<int>> ref = c.try_borrow_immut();` |
| `_Safe RefMut<T> RefCell<T>::borrow_mut(const This * _Borrow this)` | 获得 RefCell 内部值的`RefMut<T>`，失败会导致 abort | `RefMut<int> ref = c.borrow_mut();` |
| `_Safe Option<RefMut<T>> RefCell<T>::try_borrow_mut(const This * _Borrow this)` | 获得 RefCell 内部值的`RefMut<T>`，失败会返回 None | `Option<RefMut<int>> ref = c.try_borrow_mut();` |

### `RefImmut<T>/RefMut<T>` 对外接口

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe const T * _Borrow RefImmut<T>::deref(const This * _Borrow this)` | 获取 RefCell 内部值的不可变借用 | `const int * _Borrow p = immut_ref.deref();` |
| `_Safe T * _Borrow RefMut<T>::deref(This * _Borrow this)` | 获取 RefCell 内部值的可变借用 | `int * _Borrow p = mut_ref.deref();` |
