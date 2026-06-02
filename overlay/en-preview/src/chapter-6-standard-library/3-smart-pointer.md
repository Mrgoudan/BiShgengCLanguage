# Smart Pointers

## Overview

BiSheng C's ownership mechanism requires that a value can have only one owner. However, consider the following scenarios:

1. In a graph data structure, multiple edges may point to the same node. This node is shared by all the edges pointing to it, and the node cannot be released and cleaned up until no edge points to it any longer.
2. When we want to allocate some memory on the heap for multiple parts of the program to read, and we cannot determine at compile time which part of the program will finish using it last, this piece of memory cannot be released and cleaned up until the last part finishes using it.

To solve such problems, we need to allow a value to have multiple owners at the same time. Therefore, BSC **provides the `Rc<T>` type in the standard library to implement shared ownership. It is a smart pointer that points to the shared data.** To improve the safety of Rc, BSC also provides `Weak<T>` to avoid the memory leak problem caused by circular references, and provides `Cell<T>` and `RefCell<T>` to be used together with Rc to enable mutability of the data that Rc points to.

## Using `Rc<T>` to share data

Rc is the abbreviation for reference counting. By recording the number of times data is referenced, it determines whether the data is still in use; the number of references is equivalent to the number of owners. **If the reference count drops to 0, it means the referenced data can be cleaned up, at which point Rc will automatically call the data's destructor to avoid a memory leak.**

Note: When using `Rc<T>`, you must ensure that T is a copy-semantics type or an _Owned struct type, otherwise a compile-time error will occur, because for other types the compiler cannot know how to clean up the memory.

The way to use `Rc<T>` is:

1. Call the new method to create an Rc. This method allocates heap memory and stores the data to be referenced on the heap.
2. Call the clone method to obtain another Rc from an existing Rc. This method does not perform a deep copy; it merely creates another pointer pointing to the referenced data.
3. Call the deref method to obtain an immutable borrow of the data that Rc points to.

We learn how to use Rc through the following example:
Suppose a class has several students who share a single grade report. This grade record file is the data resource that needs to be shared. Now we read this file to obtain the grades of student 1 and student 2:

```C
#include "rc.hbs"

_Owned struct ScoreRecord {  // student grade record file
    FILE *fp;
    ~ScoreRecord(This this) {
        fclose(this.fp);
    }
};

_Owned struct Student {
    unsigned student_id;    // student id
    Rc<ScoreRecord> scores; // assume the student grade record file is shared by all students
};

// open the file
ScoreRecord ScoreRecord::open(String filename);
// read the contents of line id of the file
String ScoreRecord::read(const This *_Borrow this, unsigned id);

// get the grade of student number id
String Student::get_score(const This *_Borrow this) {
    const ScoreRecord *_Borrow scores = this->scores.deref(); 
    return scores->read(this->id);
}

int main() {
    ScoreRecord scores = ScoreRecord::open(String::from("scores.txt"));

    // student1 and student2 jointly own the grade file
    Rc<ScoreRecord> rc1 = Rc<ScoreRecord>::new(scores);
    Rc<ScoreRecord> rc2 = rc1.clone()；
    Student student1 = { .id = 1, .scores = rc1 };
    Student student2 = { .id = 2, .scores = rc2 };
    // at this point the reference count of the shared file is 2, because it has 2 owners

    print("Student 1's grade is: " + student1.get_score());
    print("Student 2's grade is: " + student2.get_score());

    return 0;
}   // the shared file has been fully used, the reference count drops to 0, at which point Rc automatically calls ~ScoreRecord to close the file
```

## Reference counting

Let us observe the changes in the reference count of `Rc<T>` when it is created, cloned, and goes out of scope through the following case.

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

This code will print:

```text
ref count after creating rc1 = 1
ref count after creating rc2 = 2
ref count after creating rc3 = 3
ref count after rc3 goes out of scope = 2
```

We can see that the initial reference count of rc1 is 1, and with each call to clone, the count increases by 1. When rc3 leaves its scope, the count decreases by 1. There is no need to call a function to decrease the count the way clone is called to increase the reference count, because the implementation of the `Rc<T>` destructor guarantees that the reference count is automatically decreased when an Rc leaves its scope.

What we cannot see from this example is that at the end of the ref_count function, as rc1 and rc2 leave their scope, the count drops to 0, and the data they point to is also cleaned up; this cleanup operation is likewise automatically performed through the destructor of `Rc<T>`.

## `Cell<T>`/`RefCell<T>` and interior mutability

The deref method can be used to obtain an immutable borrow of the data that Rc points to, which allows data to be shared read-only among multiple parts of the program. If `Rc<T>` also allowed multiple mutable borrows, it would violate BSC's borrowing rules: multiple mutable borrows of the same data cannot exist at the same time. However, in real-world application scenarios, modifying data is unavoidable. In this part, we will discuss the interior mutability pattern and the Cell/RefCell types, which **can be used together with Rc to modify the data that Rc points to**.

The BSC compiler strictly checks the borrowing rules at compile time. It may reject a correct program, which often causes users a great deal of pain. Therefore, the standard library provides `Cell<T>` and `RefCell<T>` to implement interior mutability: modifying the target data while holding an immutable borrow! For a normal code implementation, this is impossible to achieve (either one mutable borrow, or multiple immutable borrows).

### `Cell<T>`

`Cell<T>` is used to obtain a copy of the value it wraps internally or to directly modify the internal value. It is suitable for Copy-semantics types, such as basic data types like int and float, or struct types.

```C
void cell_example() {
    Cell<int> c = Cell<int>::new(5);
    printf("Value in cell is: %d\n", c.get()); // the get method can obtain a copy of the value wrapped inside the cell
    c.set(10);   // the set method can directly modify the value wrapped inside the cell
    printf("Value in cell is: %d\n", c.get());
}
```

### `RefCell<T>`

`RefCell<T>` is used to obtain a mutable or immutable borrow of the value it wraps internally. It actually **does not solve the problem of a mutable borrow and an immutable borrow being able to coexist; it merely defers the error from compile time to a runtime abort**.

By using the borrow_mut and borrow_immut methods of `RefCell<T>`, you attempt to obtain a borrow at runtime. If the borrow succeeds, a value of type `RefMut<T>` or `RefImmut<T>` is returned; otherwise it causes a runtime abort. The rules are:

1. The borrow_mut method is used to obtain a `RefMut<T>`; if **any other borrow** exists within the current scope, the borrow fails.
2. The borrow_immut method is used to obtain a `RefImmut<T>`; if **any other mutable borrow** exists within the current scope, the borrow fails.

`RefMut<T>` and `RefImmut<T>` are auxiliary data structures of `RefCell<T>`; their deref methods can respectively obtain a mutable and an immutable borrow of the RefCell's internal value.

```C
void refcell_example() {
    RefCell<int> c = RefCell<int>::new(5);

    // obtain a RefMut through RefCell's borrow_mut method:
    RefMut<int> ref_mut_c1 = c.borrow_mut();

    // obtain a mutable borrow of RefCell's internal value through RefMut's deref method:
    int* _Borrow mut_p1 = ref_mut_c.deref();

    // modify the internal value through the mutable borrow:
    *mut_p1 = 10;  

    // attempt to obtain a mutable borrow again
    RefMut<int> ref_mut_c2 = c.borrow_mut(); // runtime abort, because two mutable borrows exist at the same time
}
```

## Combining Rc and RefCell to modify shared data

In the example above of reading student grades, we only performed read operations on the data. With RefCell, we can modify shared data. The following example implements reading and updating grade data:

```C
#include "rc.hbs"
#include "cell.hbs"

_Owned struct ScoreRecord {  // student grade record file
    FILE *fp;
    ~ScoreRecord(This this) {
        fclose(this.fp);
    }
};

_Owned struct Student {
    unsigned student_id;
    Rc<RefCell<ScoreRecord>> scores; // wrap the shared data that needs to be modified with RefCell
};

// open the file
ScoreRecord ScoreRecord::open(String filename);
// read the contents of line id of the file
String ScoreRecord::read(const This *_Borrow this, unsigned id);
// update the contents of line id of the file
void ScoreRecord::update(This *_Borrow this, unsigned id, String new);

// get the grade of student number id
String Student::get_score(const This *_Borrow this) {
    // RefCell's borrow_immut method obtains an immutable borrow
    RefImmut<ScoreRecord> ref_scores = this->scores.deref()->borrow_immut(); 
    const ScoreRecord *_Borrow scores = ref_scores.deref();
    return scores->read(this->id);
}

// update the grade of student number id
String Student::update_score(const This *_Borrow this, String new_score) {
    // RefCell's borrow_mut method obtains a mutable borrow
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

    print("Student 1's grade is: " + student1.get_score());
    print("Student 2's grade is: " + student2.get_score());

    return 0;
}
```

## `Weak<T>` solving the circular reference problem

Although BSC has a comprehensive memory safety guarantee mechanism, this does not mean memory leaks cannot occur. A typical example is creating a circular reference using both `Rc<T>` and `RefCell<T>`; because the reference count cannot be reduced to zero, the data pointed to by `Rc<T>` will not be released and cleaned up.

### Creating a circular reference

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
    Rc<A> ap = Rc<A>::new(a);      // ap owns a

    B b = { .value = 10 };
    RefMut<Rc<A>> rm_b = b.d.borrow_mut();
    *(rm_b.deref()) = ap.clone();  // through the clone of ap, here b.d owns a
    Rc<B> bp = Rc<B>::new(b);      // bp owns b

    RefMut<Rc<B>> rm_a = ap.deref()->c.borrow_mut();
    *(rm_a.deref()) = bp.clone();  // through the clone of bp, here a.c owns b

    printf("ref count of a : %d\n", ap.strong_ref_count());
    printf("ref count of b : %d\n", bp.strong_ref_count());

    return 0;
}
```

This code will print:

```text
ref count of a : 2
ref count of b : 2
```

Before the function ends, the reference counts of both a and b are 2, where a is shared by ap and b.d, and b is shared by bp and a.c.

At the end of the function:

1. For a, after ap goes out of scope, a's reference count goes from 2 to 1; only when b.d is destructed will a's reference count be reduced to 0, thereby triggering the destruction of a.
2. For b, after bp goes out of scope, b's reference count goes from 2 to 1; only when a.c is destructed will b's reference count be reduced to 0, thereby triggering the destruction of b.

The destruction of a requires b to be destructed first, while the destruction of b requires a to be destructed first. The result is that neither a nor b is destructed, and memory is leaked!

### Using `Weak<T>` to solve circular references

To solve this problem, BSC introduces the `Weak<T>` type. Unlike Rc, which holds ownership, Weak does not hold ownership; it merely keeps a weak reference pointing to the data. **Establishing a weak reference does not increase the reference count, and it also cannot prevent the referenced data from being cleaned up.**

The way to use `Weak<T>` is:

1. Call the new method to create a `Weak<T>` from an existing `Rc<T>`.
2. The referenced data cannot be accessed directly through `Weak<T>`. If you want to access the data, you need to upgrade the Weak to an Rc, which is done through Weak's upgrade method. This method returns a value of type `Option<Rc<T>>`; if the referenced data has already been cleaned up and released, the value of the Option is None. In other words, Weak itself makes no guarantee about the existence of the referenced data.

### Circular reference scenario 1

We learn how to use `Weak<T>` through the example from the previous subsection where the circular reference of a and b caused a memory leak:

```C
#include "rc.hbs"
#include "cell.hbs"

_Owned struct B;

_Owned struct A {
_Public:
    int value;
    RefCell<Rc<B>> c;     // A references B through Rc
    ~A(A this) {
        printf("A is destructed, value = %d\n", this.value);
    }
};

_Owned struct B {
_Public:
    int value;
    RefCell<Weak<A>> d;  // B references A through Weak
    ~B(B this) {
        printf("B is destructed, value = %d\n", this.value);
    }
};

int main() {
    A a = { .value = 5 };
    Rc<A> ap = Rc<A>::new(a);           // ap owns a

    B b = { .value = 10 };
    RefMut<Weak<A>> rm_b = b.d.borrow_mut();
    *(rm_b.deref()) = Weak<A>::new(ap); // create a weak reference through ap, here b.d weakly references a
    Rc<B> bp = Rc<B>::new(b);           // bp owns b

    RefMut<Rc<B>> rm_a = ap.deref()->c.borrow_mut();
    *(rm_a.deref()) = bp.clone();       // through the clone of bp, here a.c owns b

    printf("ref count of a : %d\n", ap.strong_ref_count());
    printf("ref count of b : %d\n", bp.strong_ref_count());

    return 0;
}
```

This code will print:

```text
ref count of a : 1
ref count of b : 2
B is destructed, value = 10
A is destructed, value = 5
```

From the printed results, we see that both a and b were destructed. Analyzing this code:

1. Before the function ends, a is owned by ap with a reference count of 1, and b is shared by bp and a.c with a reference count of 2;
2. At the end of the function, after ap goes out of scope, a's reference count goes from 1 to 0, thereby triggering the destruction of a; because a.c owns b, as a is destructed, b's reference count is reduced from 2 to 1; subsequently, after bp goes out of scope, b's reference count goes from 1 to 0, thereby triggering the destruction of b.

### Circular reference scenario 2

Weak is often applied in parent-child circular reference relationships. The approach is: let the parent node reference the child nodes through Rc, and then let the child nodes reference the parent node through Weak. Next we construct a simple tree with 1 root node and 2 leaf nodes:

```C
_Owned struct Node {
_Public:
    int value;
    RefCell<Weak<Node>> parent;      // a child node references its parent node through Weak
    RefCell<Vec<Rc<Node>>> children; // a parent node references its child nodes through Rc

    ~Node(Node this) {
        printf("node %d is destructed\n", this.value);
    }
};

void tree() {
    // construct the leaf nodes
    Node temp_leaf1 = { .value = 5, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    Node temp_leaf2 = { .value = 6, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    Rc<Node> leaf1 = Rc<Node>::new(temp_leaf1);
    Rc<Node> leaf2 = Rc<Node>::new(temp_leaf2);

    printf("ref count of leaf1 after init : %d\n", leaf1.strong_ref_count());
    printf("ref count of leaf2 after init : %d\n", leaf2.strong_ref_count());

    // construct the parent node, and let the parent node reference the child nodes through Rc
    Node temp_root = { .value = 10, .children = RefCell<Vec<Rc<Node>>>::new(Vec<Rc<Node>>::new()) };
    RefMut<Vec<Rc<Node>>> rm_root_children = temp_root.children.borrow_mut();
    rm_root_children.deref()->push(leaf1.clone());
    rm_root_children.deref()->push(leaf2.clone());
    Rc<Node> root = Rc<Node>::new(temp_root);

    printf("ref count of root after init : %d\n", root.strong_ref_count());
    printf("ref count of leaf1 after root points to it : %d\n", leaf1.strong_ref_count());
    printf("ref count of leaf2 after root points to it : %d\n", leaf2.strong_ref_count());

    // a child node references its parent node through Weak
    RefMut<Weak<Node>> rm_leaf1_parent = leaf1.deref()->parent.borrow_mut();
    *(rm_leaf1_parent.deref()) = Weak<Node>::new(&_Const root);
    RefMut<Weak<Node>> rm_leaf2_parent = leaf2.deref()->parent.borrow_mut();
    *(rm_leaf2_parent.deref()) = Weak<Node>::new(&_Const root);

    printf("ref count of root after leaf1 and leaf2 weakly points to it : %d\n", root.strong_ref_count());
}
```

This code will print:

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

From the printed results, you can see that all the node data was cleaned up and released.

## Related APIs

### `Rc<T>` external interfaces

| External interface | Interface function | Code example |
| --- | --- | --- |
| `_Safe Rc<T> Rc<T>::new(T data)` | Constructs an Rc, allocating heap memory to store data | `Rc<int> rc = Rc<int>::new(5);` |
| `_Safe Rc<T> Rc<T>::clone(const This * _Borrow this)` | Clones another Rc from an existing Rc, increasing the reference count by 1 | `Rc<int> rc2 = rc.clone();` |
| `_Safe unsigned Rc<T>::strong_ref_count(const This * _Borrow this)` | Gets the strong reference count | `unsigned count = rc.strong_ref_count();` |
| `_Safe unsigned Rc<T>::weak_ref_count(const This * _Borrow this)` | Gets the weak reference count | `unsigned count = rc.weak_ref_count();` |
| `_Safe const T * _Borrow Rc<T>::deref(const This * _Borrow this)` | Gets an immutable borrow of the data Rc points to | `const int * _Borrow data = rc.deref();` |

### `Weak<T>` external interfaces

| External interface | Interface function | Code example |
| --- | --- | --- |
| `_Safe Weak<T> Weak<T>::new(const Rc<T> * _Borrow rc)` | Constructs a Weak from an existing Rc | `Weak<int> w = Weak<int>::new(&_Const rc);`|
| `_Safe Weak<T> Weak<T>::clone(const This * _Borrow this)` | Clones another Weak from an existing Weak | `Weak<int> w2 = w.clone();`|
| `_Safe unsigned Weak<T>::strong_ref_count(const This * _Borrow this)` | Gets the strong reference count | `unsigned count = w.strong_ref_count();`|
| `_Safe unsigned Weak<T>::weak_ref_count(const This * _Borrow this)` | Gets the weak reference count | `unsigned count = w.weak_ref_count();`|
| `_Safe Option<Rc<T>> Weak<T>::upgrade(const This * _Borrow this)` | Converts the Weak into the corresponding Rc, returning None on conversion failure | `Option<Rc<int>> rc = w.upgrade();`|

### `Cell<T>` external interfaces

| External interface | Interface function | Code example |
| --- | --- | --- |
| `_Safe Cell<T> Cell<T>::new(T value)` | Constructs a Cell with the given value | `Cell<int> c = Cell<int>::new(5);` |
| `_Safe T Cell<T>::get(const This * _Borrow this)` | Gets a copy of the Cell's internal value | `int a = c.get();` |
| `_Safe void Cell<T>::set(const This * _Borrow this, T val)` | Replaces the Cell's internal value with val | `c.set(10);` |

### `RefCell<T>` external interfaces

| External interface | Interface function | Code example |
| --- | --- | --- |
| `_Safe RefCell<T> RefCell<T>::new(T value)` | Constructs a RefCell with the given value | `RefCell<int> c = RefCell<int>::new(5);`|
| `_Safe RefImmut<T> RefCell<T>::borrow_immut(const This * _Borrow this)` | Gets the `RefImmut<T>` of the RefCell's internal value; failure causes an abort | `RefImmut<int> ref = c.borrow_immut();`|
| `_Safe Option<RefImmut<T>> RefCell<T>::try_borrow_immut(const This * _Borrow this)` | Gets the `RefImmut<T>` of the RefCell's internal value; failure returns None | `Option<RefImmut<int>> ref = c.try_borrow_immut();` |
| `_Safe RefMut<T> RefCell<T>::borrow_mut(const This * _Borrow this)` | Gets the `RefMut<T>` of the RefCell's internal value; failure causes an abort | `RefMut<int> ref = c.borrow_mut();` |
| `_Safe Option<RefMut<T>> RefCell<T>::try_borrow_mut(const This * _Borrow this)` | Gets the `RefMut<T>` of the RefCell's internal value; failure returns None | `Option<RefMut<int>> ref = c.try_borrow_mut();` |

### `RefImmut<T>/RefMut<T>` external interfaces

| External interface | Interface function | Code example |
| --- | --- | --- |
| `_Safe const T * _Borrow RefImmut<T>::deref(const This * _Borrow this)` | Gets an immutable borrow of the RefCell's internal value | `const int * _Borrow p = immut_ref.deref();` |
| `_Safe T * _Borrow RefMut<T>::deref(This * _Borrow this)` | Gets a mutable borrow of the RefCell's internal value | `int * _Borrow p = mut_ref.deref();` |
