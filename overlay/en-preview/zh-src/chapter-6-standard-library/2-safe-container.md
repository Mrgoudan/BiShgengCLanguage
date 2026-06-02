# 安全容器

## `Vec`

`Vec`是 BiShengC 语言提供的安全动态数组类型,数组元素的数据存储在堆上，它是一个泛型数据结构，接受一个类型参数 T 表示其内部存储的数据的类型，其使用示例如下：

```c
#include "vec.hbs"

_Safe void example(void) {
    Vec<int> vec = Vec<int>::new(); // 分配一个空的动态数组
    vec.push(1); // 向数组中插入元素
    vec.push(2); // 向数组中插入元素

    size_t len = vec.length();
    int elem = *vec.get(0); // elem is 1

    vec.set(0, 7); // 将索引为0处的值置为7
    elem = *vec.get(0); // elem is 7

    vec.shrink_to_fit();
    // vec作用域结束时自动调用析构函数释放堆内存空间,无需手动释放
}
```

索引：
`Vec`目前不支持直接使用索引进行访问，但其提供了`set`和`get`等方法,可以传入想要访问的元素的索引，通过函数调用的方式进行访问。

容量及重新分配：
`Vec`的容量是为将来添加到`Vec`中的任何元素预先分配的内存空间大小，不要将其与`Vec`的长度向混淆，`Vec`的长度表示的是当前`Vec`内实际存储的元素的数量。
如果`Vec`的长度超过其容量，则容量会自动增长，且其元素也需要重新分配(这由Vec内部实现)。

注意：在使用`Vec<T>`时，需要保证 T 是 copy 语义的类型或 _Owned struct 类型，否则会编译期报错，因为对于其它类型，编译器无法知道应该如何清理内存。

`Vec`提供的对外接口及相应的使用用例如下：

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe size_t Vec<T>::capacity(const Vec<T>* _Borrow this)` | 返回数组的容量 | `size_t cap = vec.capacity();` |
| `_Safe void Vec<T>::clear(Vec<T>* _Borrow this)` | 清空数组的所有元素 | `vec.clear();` |
| `_Safe const T* _Borrow Vec<T>::get(const Vec<T>* _Borrow this, size_t index)` | 获取数组中下标为index的元素的不可变借用(**要做边界检查**) | `const int* _Borrow elem = vec.get(2);` |
| `_Safe T* _Borrow Vec<T>::get_mut(Vec<T>* _Borrow this, size_t index)` | 获取数组中下标为index的元素的可变借用(**要做边界检查**) | `int* _Borrow elem = vec.get_mut(2);` |
| `_Safe _Bool Vec<T>::is_empty(const Vec<T>* _Borrow this)` | 判断数组是否为空 | `_Bool flag = vec.is_empty();` |
| `_Safe size_t Vec<T>::length(const Vec<T>* _Borrow this)` | 返回数组的长度 | `size_t len = vec.length();` |
| `_Safe Vec<T> Vec<T>::new(void)` | 创建一个空的数组 | `Vec<int> vec = Vec<int>::new();` |
| `_Safe T Vec<T>::pop(Vec<T>* _Borrow this)` | 弹出数组中最后一个元素(**要做边界检查**) | `int last = vec.pop();` |
| `_Safe void Vec<T>::push(Vec<T>* _Borrow this, T value)` | 向数组的尾部插入一个值为value的元素 | `vec.push(2);` |
| `_Safe T Vec<T>::remove(Vec<T>* _Borrow this, size_t index)` | 从数组中移除下标为index的元素(**要做边界检查**) | `int m = vec.remove(3);` |
| `_Safe void Vec<T>::set(Vec<T>* _Borrow this, size_t index, T value)` | 将数组中下标为index的元素置为value(**要做边界检查**) | `vec.set(3, 5);` |
| `_Safe void Vec<T>::shrink_to_fit(Vec<T>* _Borrow this)` | 调整数组占用的内存空间,将容量缩减到数组的长度 | `vec.shrink_to_fit();` |
| `_Safe Vec<T> Vec<T>::with_capacity(size_t cap)` | 创建一个容量为cap的空数组 | `Vec<int> vec = Vec<int>::with_capacity(120);` |

注：`Vec`提供的安全 API 内部实现了严格的边界访问检查逻辑，确保在使用这些 API 时不会出现越界访问的问题。当发生越界访问时，程序打印当前的函数调用栈，并终止执行。

## `String`

`String`是 BiShengC 语言提供的 C 风格的安全字符串类型，用于安全地管理分配在堆上的字符串。它拥有字符串内容的所有权，字符串的内容存储在堆分配的缓冲区中。其使用示例如下：

```c
String hello = String::from("Hello, world!");

hello.push('w');
hello.set(0, 'k');

String world = String::from("hello bishengc");
hello.equals(&_Const world);

String new_s = world.slice(1, 4);
```

内部表示：
`String`内部由三个部分组成：指向某些字节的指针、长度和容量。该指针指向`String`用于存储其数据的内部缓冲区，长度是当前存储在缓冲区中的字节数，容量是当前缓冲区的大小（以字节为单位）。因此，长度会始终小于或等于容量。存储字节的缓冲区始终分配在堆上。

`String`提供的对外接口及相应的使用用例如下：

|对外接口|接口功能|代码示例|
|---|---|---|
|`_Safe char* _Borrow String::as_mut_str(String* _Borrow this)`|返回对字符串的可变借用|char* _Borrow str = s.as_mut_str();|
|`_Safe const char* _Borrow String::as_str(const String* _Borrow this)`|返回对字符串的不可变借用|const char* _Borrow str = s.as_str();|
|`_Safe char String::at(const String* _Borrow this, size_t index)`|返回字符串的下标为index处的值（**要做边界检查**）|char c = s.at(2);|
|`_Safe size_t String::capacity(const String* _Borrow this)`|返回字符串的容量|size_t cap = s.capacity();|
|`_Safe _Bool String::equals(const String* _Borrow this, const String* _Borrow other);`|比较两个字符串是否相等|_Bool flag = str1.equals(str2);|
|`_Safe size_t String::find(const String* _Borrow this, char c)`|查找字符串中是否有字符c，返回第一次找到时的下标，如果没有找到，则返回bsc_string_no_pos|size_t pos = s.find('A');|
|`_Unsafe String String::from(const char* str)`|根据字符串字面量创建字符串|String s = String::from("hello");|
|`_Safe const T* _Borrow String::get(const String* _Borrow this, size_t index)`|获取字符串中下标为index的元素的不可变借用(**要做边界检查**)|const char* _Borrow elem = s.get(2);|
|`_Safe T* _Borrow String::get_mut(String* _Borrow this, size_t index)`|获取字符串中下标为index的元素的可变借用(**要做边界检查**)|char* _Borrow elem = s.get_mut(2);|
|`_Safe _Bool String::is_empty(const String* _Borrow this)`|判断字符串是否为空|_Bool flag = s.is_empty();|
|`_Safe size_t String::length(const String* _Borrow this)`|返回字符串的长度|size_t len = s.length();|
|`_Safe String String::new(void)`|创建一个空的字符串|String s = String::new();|
|`_Safe void String::push(String* _Borrow this, char value)`|向字符串的尾部插入一个值为value的元素|s.push('h');|
|`_Safe void String::set(String* _Borrow this, size_t index, char value)`|将字符串中下标为index的元素置为value(**要做边界检查**)|s.set(3, '5');|
|`_Safe void String::shrink_to_fit(String* _Borrow this)`|调整字符串占用的内存空间,将容量缩减到字符串的长度|s.shrink_to_fit();|
|`_Safe String String::slice(const String* _Borrow this, size_t start, size_t length);`|字符串切片，如果 start 大于字符串的长度，则会触发越界；对于 length，如果start + length 大于字符串的长度，则字符串切片只会取到字符串的结尾处|String new_string = s.slice(0, 5);|
|`_Safe String String::with_capacity(size_t cap)`|创建一个容量为cap的空字符串|String s = String::with_capacity(20);|

注：`bsc_string_no_pos`实际上是一个很大的 size_t 类型的值，即 SIZE_MAX。

## `Slice`

`Slice`是 BiShengC 语言提供的不可变切片类型，用于对一段连续的且类型相同的内存进行读操作。其使用示例如下：

```c
#include "slice.hbs"

int main() {
  int arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  Slice<int> slice = Slice<int>::new(&_Const *arr, 5);
  const int *_Borrow _Nonnull first = slice.get(2);
  if (*first == 3) {
    printf("The third element of the slice is 3\n");
  }
  return 0;
}
```

索引：
`Slice`目前不支持直接使用索引进行访问，但其提供了`get`等方法,可以传入想要访问的元素的索引，通过函数调用的方式进行访问。在访问`Slice`的元素时，会做**边界检查**。

`Slice`提供的对外接口及相应的使用用例如下：

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Unsafe Slice<T> Slice<T>::new(const T *_Borrow _Nonnull ptr, size_t len)` | 根据指针和长度创建切片 | `Slice<int> slice = Slice<int>::new(&_Const *arr, 5);` |
| `_Safe Slice<T> Slice<T>::sub(This this, size_t start, size_t end)` | 从现有的切片中创建子切片 | `Slice<int> sub = slice.sub(0, 5);` |
| `_Safe const T *_Borrow _Nonnull Slice<T>::get(This this, size_t index)` | 返回切片中下标为index的元素的不可变借用 | `const int *_Borrow _Nonnull first = slice.get(0);` |
| `_Safe size_t Slice<T>::length(This this)` | 返回切片长度 | `size_t len = slice.length();` |
| `_Safe _Bool Slice<T>::is_empty(This this)` | 判断切片是否为空 | `_Bool is_empty = slice.is_empty();` |
| `_Unsafe const T *_Nonnull Slice<T>::as_ptr(This this)` | 返回切片中保存的裸指针 | `const T *_Nonnull ptr = slice.as_ptr();` |

## `SliceMut`

`SliceMut`是 BiShengC 语言提供的可变切片类型，用于对一段连续的且类型相同的内存进行读写操作。其使用示例如下：

```c
#include "slice.hbs"

int main() {
  int arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  SliceMut<int> slice = SliceMut<int>::new(&_Const *arr, 5);
  const int *_Borrow _Nonnull first = slice.get(2);
  *first = 5;
  if (*first == 5) {
    printf("The third element of the slice is 5\n");
  }
  return 0;
}
```

索引：
`SliceMut`目前不支持直接使用索引进行访问，但其提供了`set`和`get`等方法,可以传入想要访问的元素的索引，通过函数调用的方式进行访问。在访问`SliceMut`的元素时，会做**边界检查**。

`SliceMut`提供的对外接口及相应的使用用例如下：

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Unsafe SliceMut<T> SliceMut<T>::new(T *_Borrow _Nonnull ptr, size_t len)` | 根据指针和长度创建可变切片 | `SliceMut<int> slice = SliceMut<int>::new(&_Mut *arr, 5);` |
| `_Safe Slice<T> SliceMut<T>::sub(This this, size_t start, size_t end)` | 从现有的切片中创建不可变子切片 | `Slice<int> sub = slice.sub(0, 4);` |
| `_Safe SliceMut<T> SliceMut<T>::sub_mut(This this, size_t start, size_t end)` | 从现有的切片中创建可变子切片 | `SliceMut<int> sub_mut = slice.sub_mut(0, 4);` |
| `_Safe const T *_Borrow _Nonnull SliceMut<T>::get(This this, size_t index)` | 返回切片中下标为index的元素的不可变借用 | `const int *_Borrow _Nonnull first = slice.get(0);` |
| `_Safe T *_Borrow _Nonnull SliceMut<T>::get_mut(This this, size_t index)` | 返回切片中下标为index的元素的可变借用 | `int *_Borrow _Nonnull first = slice.get_mut(0);` |
| `_Safe size_t SliceMut<T>::length(This this)` | 返回切片长度 | `size_t len = slice.length();` |
| `_Safe _Bool SliceMut<T>::is_empty(This this)` | 判断切片是否为空 | `_Bool is_empty = slice.is_empty();` |
| `_Unsafe const T *_Nonnull SliceMut<T>::as_ptr(This this)` | 返回切片中保存的裸指针 | `const T *_Nonnull ptr = slice.as_ptr();` |
| `_Unsafe T *_Nonnull SliceMut<T>::as_mut_ptr(This this)` | 返回切片中保存的裸指针 | `T *_Nonnull ptr = slice.as_mut_ptr();` |

## LinkedList

### 概述

LinkedList是由双向链表来实现的，支持前后两种移动方向。

注意：在使用`LinkedList<T>`时，需要保证 T 是 copy 语义的类型或 _Owned struct 类型，否则会编译期报错，因为对于其它类型，编译器无法知道应该如何清理内存。

### 头文件

```c
#include "list.hbs"
```

### API

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `LinkedList<T> LinkedList<T>::new()` | 创建一个链表对象。 | `// 新增int类型链表对象`<br />`LinkedList<int> list = LinkedList<int>::new();` |
| `void LinkedList<T>::push_back(T el)` | 向链表尾端插入一个数据。 | `list.push_back(5); // 向链表尾插入数据5` |
| `void LinkedList<T>::push_front(T el)` | 向链表表头插入一个数据。 | `list.push_front(6); // 向链表头插入数据6` |
| `size_t LinkedList<T>::length()` | 返回链表中已有数据个数。 | `assert(2 == list.length());` |
| `_Bool LinkedList<T>::is_empty()` | 判断表是否为空表。 | `list.push_back(4);`<br />`assert(0 == list.is_empty())` |
| `T LinkedList<T>::pop_back()` | 返回表尾数据，并将该数据从链表中删除。表不可以为空表。 | `assert(5 == list.pop_back());` |
| `T LinkedList<T>::pop_front()` | 返回表头数据，并将该数据从链表中删除。表不可以为空表。 | `assert(6 == list.pop_front());` |
| `void LinkedList<T>::remove(T el)` | 删除链表中所有指定数据。 | `list.push_back(4);`<br />`list.remove(4); // 删除链表中所有值为4的元素` |
| `void LinkedList<T>::clear()` | 删除链表中所有数据。 | `list.clear();` |
| `T LinkedList<T>::front()` | 返回表头数据，表不可以为空表。 | `list.push_front(3);`<br />`assert(3 == list.front())` |
| `T LinkedList<T>::back()` | 返回表尾数据，表不可以为空表。 | `list.push_back(3);`<br />`assert(3 == list.back())` |
| `_Bool LinkedList<T>::contains(T el)` | 检查表是否包含指定数据。 | `list.push_back(6);`<br />`assert(1 == list.contains(6))` |
| `LinkedList<T> LinkedList<T>::split_off(size_t index)` | 在索引处将链表拆分为两个链表，<br />索引值不能大于链表中数据个数，<br />新链表包含索引值以及往后的数据。<br />若索引值为0，则旧链表将为空表，<br />若索引值等于表数据个数，则新链表将为空。 | `LinkedList<int> l = LinkedList<int>::new();`<br />`l.push_back(5);`<br />`l.push_back(6);`<br />`l.push_front(4);`<br />`l.push_front(3);`<br />`LinkedList<int> j = l.split_off(2);`<br />`// 链表被拆为l和j两个表`<br />`assert(2 == l.length());      // 3 4`<br />`assert(2 == j.length());      // 5 6` |

## `Option`

`Option`是 BiShengC 语言提供的用于处理可选值的一个安全数据结构，用来表示一个值可能存在或不存在这两种情况。
它是一个泛型数据结构，具有一个泛型参数`T`，用于表示其存储的可选值的类型。使用`Option`可以安全地处理一些函数的返回值，避免使用空指针等数据结构来表示一个值不存在的情况。
`Option`有两种状态，即`Some`和`None`，`Some`状态中会包含所需要的类型的值，而`None`状态中没有存储相应的值。
它的一个使用示例如下：

```c
#include "option.hbs" // Option类型声明在该头文件中，使用时需包含该头文件

Option<size_t> string_find(String str) {
    size_t pos = str.find('k');
    if (pos == bsc_string_no_pos) {
        return Option<size_t>::None(); // 如果没有找到k，则返回None
    } else {
        return Option<size_t>::Some(pos); // 如果找到了k，则返回Some，且Some中包含相应的值pos
    }
}

void use_option() {
    size_t pos = -1;
    String s = String::from("hello");
    Option<size_t> res = string_find(s);
    if (res.is_some()) { // 判断res是Some还是None
        pos = option_unwrap(res); // 对于Some，则调用option_unwrap获取其中的值
    } else {
        return; // 对于None，则不做处理
    }
}
```

这个例子中首先定义了一个函数`string_find`，对`String`的`find`方法进行了封装，用于处理找到字符k和没有找到字符k的情况。接下来在使用处，可以使用`is_some`方法处理返回的`Option<size_t>`类型的值，从而获取里面的数据或者不做处理。这种写法可以安全地处理一些函数的返回值，提高程序的安全性。

`Option`是一个 _Owned struct类型的数据结构，因此其自带析构函数，可以安全地释放其所占用的内存空间，无需使用者进行手动内存释放。

注意：在使用`Option<T>`时，需要保证 T 是 copy 语义的类型或 _Owned struct 类型，否则会编译期报错，因为对于其它类型，编译器无法知道应该如何清理内存。

`Option`提供的对外接口及相应的使用用例如下：

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe _Bool Option<T>::is_none(const Option<T>* _Borrow this)` | 判断`Option`是否为`None`，若是则返回1，否则返回0 | `_Bool flag = option.is_none();` |
| `_Safe _Bool Option<T>::is_some(const Option<T>* _Borrow this)` | 判断`Option`是否为`Some`，若是则返回1，否则返回0 | `_Bool falg = option.is_some();` |
| `_Safe Option<T> Option<T>::Some(T t)` | 创建一个状态为`Some`的`Option`值，其中包含的`T`类型的结果的值为`t` | `Option<size_t> option = Option<size_t>::Some(3);` |
| `_Safe Option<T> Option<T>::None(void)` | 创建一个状态为`None`的`Option`值，其中不包含`T`的类型的结果值 | `Option<size_t> option = Option<size_t>::None();` |
| `_Safe T option_unwrap<T>(Option<T> option)` | 展开option，从中取出存储的T类型的值，对于`Some`可以成功取出`T`类型的值，对于`None`则会导致运行时终止 | `size_t index = option_unwrap(option);` |

注：`option_unwrap`函数并非`Option`类型的成员方法，且其再展开`None`时会运行时终止，因此建议在调用该函数前使用`is_some`或`is_none`对`Option`的状态先进行判断。

## `HashMap`

`HashMap`是 BiShengC 语言提供的安全哈希表类型，哈希表的键和值都存储在堆上，它是一个泛型数据结构，具有三个泛型参数`K`、`V`和`S`。
其中`K`代表的是键的类型，`V`代表的是值的类型，而`S`代表的是该`HashMap`所使用的哈希函数。

我们可以用各种类型来作为键的类型，包含除float和double以外的各种基本类型，以及自定义的数据类型。
在将一个类型作为键的类型时，该类型必须实现了`hash`和`equals`这两个方法。BiShengC 已经为基本类型提供了这两个方法，无需使用者再次手动实现。

注意：在使用`HashMap<K, V>`时，需要保证 K 和 V 是 copy 语义的类型或 _Owned struct 类型，否则会编译期报错，因为对于其它类型，编译器无法知道应该如何清理内存。

对于哈希函数，BiShengC 提供了一个`SipHasher13`作为默认的哈希函数。该类型有两个方法用于创建其实例：

|对外接口|接口功能|代码示例|
|---|---|---|
|`SipHasher13 SipHasher13::new()`|创建默认的`SipHasher13`|SipHasher13 sh = SipHasher13::new();|
|`SipHasher13 SipHasher13::new_with_keys(uint64_t key0, uint64_t key1)`|使用`key0`和`key1`创建`SipHasher13`，BiShengC 还提供了`RandomState`类型，用于创建随机的`key0`和`key1`值|RandomState rs = RandomState::new();<br/>SipHasher13 sh = SipHasher13::new_with_keys(rs.k0, rs.k1);|

`HashMap`的一个使用实例如下：

```c
#include "hash_map.hbs" // HashMap类型声明在该头文件中，使用时需包含该头文件

// 使用HashMap来统计一个数组中各个值出现的次数
void hash_map_example(Vec<int> vec) {
    HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::with_hasher(SipHasher13::new()); // 创建HashMap

    for (size_t i = 0; i < vec.length(); i++) {
        const int* _Borrow cur = vec.get(i);
        if (map.contains_key(cur)) { // 判断map中是否已经有cur
            int* _Borrow v = map.get_mut(cur); // 获取键对应的值的可变借用
            *v = *v + 1; // 更新相应的值
        } else {
            int k = *cur;
            map.insert(k, 1); // 向map中插入键值对
        }
    }

    _Bool empty_flag = map.is_empty(); // 判断map是否为空
    int x = 3;
    _Bool key_flag = map.contains_key(&_Const x); // 判断map是否有键3
    Option<int> removed = map.remove(&_Const x);
    if (removed.is_some()) {
        int v = option_unwrap(removed);
    }
    map.clear(); // 清空map的所有元素
}
```

`HashMap`提供的对外接口及相应的使用用例如下：

| 对外接口 | 接口功能 | 代码示例 |
| --- | --- | --- |
| `_Safe size_t HashMap<K, V, S>::capacity(const HashMap<K, V, S>* _Borrow this)` | 返回哈希表的容量 | `size_t cap = map.capacity();` |
| `_Safe void HashMap<K, V, S>::clear(HashMap<K, V, S>* _Borrow this)` | 清空哈希表的所有键值对，但表的容量保持不变 | `map.clear();` |
| `_Safe _Bool HashMap<K, V, S>::contains_key(const HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | 判断哈希表是否存在值为\*k的键 | `int x = 1;<br/>_Bool flag = map.contains_key(&_Const x);` |
| `_Safe Option<V* _Borrow> HashMap<K, V, S>::get_mut(HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | 获取值为\*k的键对应的值的可变借用，若表中不存在键\*k，则返回None | `int x = 1;<br/>int* _Borrow v = map.get_mut(&_Const x);` |
| `_Safe void HashMap<K, V, S>::reserve(HashMap<K, V, S>* _Borrow this, size_t additional)` | 调整哈希表的容量，使其调整后在不扩容的情况下能至少再存入additional个元素（一般来说，该方法无需手动调用，在调用insert时会被隐式调用一次） | `map.reserve(1);` |
| `_Safe Option<V> HashMap<K, V, S>::insert(HashMap<K, V, S>* _Borrow this, K k, V v)` | 向哈希表中插入键为k、值为v的键值对 | `map.insert(1, 2);` |
| `_Safe _Bool HashMap<K, V, S>::is_empty(const HashMap<K, V, S>* _Borrow this)` | 判断哈希表是否为空 | `_Bool empty = map.is_empty();` |
| `_Safe size_t HashMap<K, V, S>::length(const HashMap<K, V, S>* _Borrow this)` | 返回哈希表中存储的键值对的数量 | `size_t len = map.length();` |
| `_Safe Option<V> HashMap<K, V, S>::remove(HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | 从哈希表中移除键为\*k的键值对，并返回对应的值，如果哈希表不存在键\*k，则返回None | `int x = 3;<br/>Option<int> removed = map.remove(&_Const x);<br/>if (removed.is_some()) { int y = option_unwrap(removed); }` |
| `_Safe HashMap<K, V, S> HashMap<K, V, S>::with_capacity_and_hasher(size_t cap, S hash_builder)` | 创建初始容量为cap、哈希函数为hash_builder的哈希表 | `HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::new_with_cap_and_hasher(3, SipHasher13::new());` |
| `_Safe HashMap<K, V, S> HashMap<K, V, S>::with_hasher(S hash_builder)` | 创建初始容量为0，哈希函数为hash_builder的哈希表 | `HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::new_with_hasher(SipHasher13::new());` |
