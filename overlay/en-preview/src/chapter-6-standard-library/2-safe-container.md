# Safe Containers

## `Vec`

`Vec` is the safe dynamic array type provided by the BiSheng C language. The data of the array elements is stored on the heap. It is a generic data structure that takes a type parameter T representing the type of the data stored internally. An example of its usage is as follows:

```c
#include "vec.hbs"

_Safe void example(void) {
    Vec<int> vec = Vec<int>::new(); // allocate an empty dynamic array
    vec.push(1); // insert an element into the array
    vec.push(2); // insert an element into the array

    size_t len = vec.length();
    int elem = *vec.get(0); // elem is 1

    vec.set(0, 7); // set the value at index 0 to 7
    elem = *vec.get(0); // elem is 7

    vec.shrink_to_fit();
    // when vec goes out of scope, the destructor is called automatically to free the heap memory; no manual release is needed
}
```

Indexing:
`Vec` currently does not support access directly via indexing, but it provides methods such as `set` and `get`, to which you can pass the index of the element you want to access, accessing it via a function call.

Capacity and reallocation:
The capacity of a `Vec` is the size of the memory space pre-allocated for any elements that will be added to the `Vec` in the future. Do not confuse it with the length of the `Vec`; the length of a `Vec` represents the number of elements actually stored in the `Vec` at the moment.
If the length of the `Vec` exceeds its capacity, the capacity will automatically grow, and its elements will also need to be reallocated (this is handled by the internal implementation of Vec).

Note: When using `Vec<T>`, you need to ensure that T is a type with copy semantics or an _Owned struct type. Otherwise, a compile-time error will occur, because for other types the compiler cannot know how to clean up the memory.

The external interfaces provided by `Vec` and the corresponding usage examples are as follows:

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `_Safe size_t Vec<T>::capacity(const Vec<T>* _Borrow this)` | Returns the capacity of the array | `size_t cap = vec.capacity();` |
| `_Safe void Vec<T>::clear(Vec<T>* _Borrow this)` | Clears all elements of the array | `vec.clear();` |
| `_Safe const T* _Borrow Vec<T>::get(const Vec<T>* _Borrow this, size_t index)` | Gets an immutable borrow of the element at index `index` in the array (**performs bounds checking**) | `const int* _Borrow elem = vec.get(2);` |
| `_Safe T* _Borrow Vec<T>::get_mut(Vec<T>* _Borrow this, size_t index)` | Gets a mutable borrow of the element at index `index` in the array (**performs bounds checking**) | `int* _Borrow elem = vec.get_mut(2);` |
| `_Safe _Bool Vec<T>::is_empty(const Vec<T>* _Borrow this)` | Determines whether the array is empty | `_Bool flag = vec.is_empty();` |
| `_Safe size_t Vec<T>::length(const Vec<T>* _Borrow this)` | Returns the length of the array | `size_t len = vec.length();` |
| `_Safe Vec<T> Vec<T>::new(void)` | Creates an empty array | `Vec<int> vec = Vec<int>::new();` |
| `_Safe T Vec<T>::pop(Vec<T>* _Borrow this)` | Pops the last element of the array (**performs bounds checking**) | `int last = vec.pop();` |
| `_Safe void Vec<T>::push(Vec<T>* _Borrow this, T value)` | Inserts an element with value `value` at the end of the array | `vec.push(2);` |
| `_Safe T Vec<T>::remove(Vec<T>* _Borrow this, size_t index)` | Removes the element at index `index` from the array (**performs bounds checking**) | `int m = vec.remove(3);` |
| `_Safe void Vec<T>::set(Vec<T>* _Borrow this, size_t index, T value)` | Sets the element at index `index` in the array to `value` (**performs bounds checking**) | `vec.set(3, 5);` |
| `_Safe void Vec<T>::shrink_to_fit(Vec<T>* _Borrow this)` | Adjusts the memory space occupied by the array, shrinking the capacity down to the length of the array | `vec.shrink_to_fit();` |
| `_Safe Vec<T> Vec<T>::with_capacity(size_t cap)` | Creates an empty array with capacity `cap` | `Vec<int> vec = Vec<int>::with_capacity(120);` |

Note: The safe APIs provided by `Vec` internally implement strict bounds-checking logic, ensuring that out-of-bounds access does not occur when using these APIs. When an out-of-bounds access occurs, the program prints the current function call stack and terminates execution.

## `String`

`String` is the C-style safe string type provided by the BiSheng C language, used to safely manage strings allocated on the heap. It owns the contents of the string, and the contents of the string are stored in a heap-allocated buffer. An example of its usage is as follows:

```c
String hello = String::from("Hello, world!");

hello.push('w');
hello.set(0, 'k');

String world = String::from("hello bishengc");
hello.equals(&_Const world);

String new_s = world.slice(1, 4);
```

Internal representation:
A `String` internally consists of three parts: a pointer to some bytes, a length, and a capacity. The pointer points to the internal buffer that the `String` uses to store its data; the length is the number of bytes currently stored in the buffer; and the capacity is the current size of the buffer (in bytes). Therefore, the length will always be less than or equal to the capacity. The buffer storing the bytes is always allocated on the heap.

The external interfaces provided by `String` and the corresponding usage examples are as follows:

|External Interface|Interface Function|Code Example|
|---|---|---|
|`_Safe char* _Borrow String::as_mut_str(String* _Borrow this)`|Returns a mutable borrow of the string|char* _Borrow str = s.as_mut_str();|
|`_Safe const char* _Borrow String::as_str(const String* _Borrow this)`|Returns an immutable borrow of the string|const char* _Borrow str = s.as_str();|
|`_Safe char String::at(const String* _Borrow this, size_t index)`|Returns the value at index `index` in the string (**performs bounds checking**)|char c = s.at(2);|
|`_Safe size_t String::capacity(const String* _Borrow this)`|Returns the capacity of the string|size_t cap = s.capacity();|
|`_Safe _Bool String::equals(const String* _Borrow this, const String* _Borrow other);`|Compares whether two strings are equal|_Bool flag = str1.equals(str2);|
|`_Safe size_t String::find(const String* _Borrow this, char c)`|Searches for whether the character c exists in the string, returning the index where it is first found; if not found, returns bsc_string_no_pos|size_t pos = s.find('A');|
|`_Unsafe String String::from(const char* str)`|Creates a string from a string literal|String s = String::from("hello");|
|`_Safe const T* _Borrow String::get(const String* _Borrow this, size_t index)`|Gets an immutable borrow of the element at index `index` in the string (**performs bounds checking**)|const char* _Borrow elem = s.get(2);|
|`_Safe T* _Borrow String::get_mut(String* _Borrow this, size_t index)`|Gets a mutable borrow of the element at index `index` in the string (**performs bounds checking**)|char* _Borrow elem = s.get_mut(2);|
|`_Safe _Bool String::is_empty(const String* _Borrow this)`|Determines whether the string is empty|_Bool flag = s.is_empty();|
|`_Safe size_t String::length(const String* _Borrow this)`|Returns the length of the string|size_t len = s.length();|
|`_Safe String String::new(void)`|Creates an empty string|String s = String::new();|
|`_Safe void String::push(String* _Borrow this, char value)`|Inserts an element with value `value` at the end of the string|s.push('h');|
|`_Safe void String::set(String* _Borrow this, size_t index, char value)`|Sets the element at index `index` in the string to `value` (**performs bounds checking**)|s.set(3, '5');|
|`_Safe void String::shrink_to_fit(String* _Borrow this)`|Adjusts the memory space occupied by the string, shrinking the capacity down to the length of the string|s.shrink_to_fit();|
|`_Safe String String::slice(const String* _Borrow this, size_t start, size_t length);`|Slices the string. If `start` is greater than the length of the string, an out-of-bounds error is triggered; for `length`, if `start + length` is greater than the length of the string, the string slice will only extend to the end of the string|String new_string = s.slice(0, 5);|
|`_Safe String String::with_capacity(size_t cap)`|Creates an empty string with capacity `cap`|String s = String::with_capacity(20);|

Note: `bsc_string_no_pos` is actually a very large value of type size_t, namely SIZE_MAX.

## `Slice`

`Slice` is the immutable slice type provided by the BiSheng C language, used for read operations on a contiguous region of memory of the same type. An example of its usage is as follows:

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

Indexing:
`Slice` currently does not support access directly via indexing, but it provides methods such as `get`, to which you can pass the index of the element you want to access, accessing it via a function call. When accessing an element of a `Slice`, **bounds checking** is performed.

The external interfaces provided by `Slice` and the corresponding usage examples are as follows:

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `_Unsafe Slice<T> Slice<T>::new(const T *_Borrow _Nonnull ptr, size_t len)` | Creates a slice from a pointer and a length | `Slice<int> slice = Slice<int>::new(&_Const *arr, 5);` |
| `_Safe Slice<T> Slice<T>::sub(This this, size_t start, size_t end)` | Creates a sub-slice from an existing slice | `Slice<int> sub = slice.sub(0, 5);` |
| `_Safe const T *_Borrow _Nonnull Slice<T>::get(This this, size_t index)` | Returns an immutable borrow of the element at index `index` in the slice | `const int *_Borrow _Nonnull first = slice.get(0);` |
| `_Safe size_t Slice<T>::length(This this)` | Returns the length of the slice | `size_t len = slice.length();` |
| `_Safe _Bool Slice<T>::is_empty(This this)` | Determines whether the slice is empty | `_Bool is_empty = slice.is_empty();` |
| `_Unsafe const T *_Nonnull Slice<T>::as_ptr(This this)` | Returns the raw pointer held in the slice | `const T *_Nonnull ptr = slice.as_ptr();` |

## `SliceMut`

`SliceMut` is the mutable slice type provided by the BiSheng C language, used for read and write operations on a contiguous region of memory of the same type. An example of its usage is as follows:

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

Indexing:
`SliceMut` currently does not support access directly via indexing, but it provides methods such as `set` and `get`, to which you can pass the index of the element you want to access, accessing it via a function call. When accessing an element of a `SliceMut`, **bounds checking** is performed.

The external interfaces provided by `SliceMut` and the corresponding usage examples are as follows:

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `_Unsafe SliceMut<T> SliceMut<T>::new(T *_Borrow _Nonnull ptr, size_t len)` | Creates a mutable slice from a pointer and a length | `SliceMut<int> slice = SliceMut<int>::new(&_Mut *arr, 5);` |
| `_Safe Slice<T> SliceMut<T>::sub(This this, size_t start, size_t end)` | Creates an immutable sub-slice from an existing slice | `Slice<int> sub = slice.sub(0, 4);` |
| `_Safe SliceMut<T> SliceMut<T>::sub_mut(This this, size_t start, size_t end)` | Creates a mutable sub-slice from an existing slice | `SliceMut<int> sub_mut = slice.sub_mut(0, 4);` |
| `_Safe const T *_Borrow _Nonnull SliceMut<T>::get(This this, size_t index)` | Returns an immutable borrow of the element at index `index` in the slice | `const int *_Borrow _Nonnull first = slice.get(0);` |
| `_Safe T *_Borrow _Nonnull SliceMut<T>::get_mut(This this, size_t index)` | Returns a mutable borrow of the element at index `index` in the slice | `int *_Borrow _Nonnull first = slice.get_mut(0);` |
| `_Safe size_t SliceMut<T>::length(This this)` | Returns the length of the slice | `size_t len = slice.length();` |
| `_Safe _Bool SliceMut<T>::is_empty(This this)` | Determines whether the slice is empty | `_Bool is_empty = slice.is_empty();` |
| `_Unsafe const T *_Nonnull SliceMut<T>::as_ptr(This this)` | Returns the raw pointer held in the slice | `const T *_Nonnull ptr = slice.as_ptr();` |
| `_Unsafe T *_Nonnull SliceMut<T>::as_mut_ptr(This this)` | Returns the raw pointer held in the slice | `T *_Nonnull ptr = slice.as_mut_ptr();` |

## LinkedList

### Overview

LinkedList is implemented as a doubly linked list, supporting both forward and backward traversal directions.

Note: When using `LinkedList<T>`, you need to ensure that T is a type with copy semantics or an _Owned struct type. Otherwise, a compile-time error will occur, because for other types the compiler cannot know how to clean up the memory.

### Header File

```c
#include "list.hbs"
```

### API

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `LinkedList<T> LinkedList<T>::new()` | Creates a linked list object. | `// create a new int-type linked list object`<br />`LinkedList<int> list = LinkedList<int>::new();` |
| `void LinkedList<T>::push_back(T el)` | Inserts a piece of data at the tail of the linked list. | `list.push_back(5); // insert data 5 at the tail of the list` |
| `void LinkedList<T>::push_front(T el)` | Inserts a piece of data at the head of the linked list. | `list.push_front(6); // insert data 6 at the head of the list` |
| `size_t LinkedList<T>::length()` | Returns the number of data items already in the linked list. | `assert(2 == list.length());` |
| `_Bool LinkedList<T>::is_empty()` | Determines whether the list is empty. | `list.push_back(4);`<br />`assert(0 == list.is_empty())` |
| `T LinkedList<T>::pop_back()` | Returns the tail data and removes it from the linked list. The list must not be empty. | `assert(5 == list.pop_back());` |
| `T LinkedList<T>::pop_front()` | Returns the head data and removes it from the linked list. The list must not be empty. | `assert(6 == list.pop_front());` |
| `void LinkedList<T>::remove(T el)` | Removes all occurrences of the specified data in the linked list. | `list.push_back(4);`<br />`list.remove(4); // remove all elements with value 4 from the list` |
| `void LinkedList<T>::clear()` | Removes all data from the linked list. | `list.clear();` |
| `T LinkedList<T>::front()` | Returns the head data. The list must not be empty. | `list.push_front(3);`<br />`assert(3 == list.front())` |
| `T LinkedList<T>::back()` | Returns the tail data. The list must not be empty. | `list.push_back(3);`<br />`assert(3 == list.back())` |
| `_Bool LinkedList<T>::contains(T el)` | Checks whether the list contains the specified data. | `list.push_back(6);`<br />`assert(1 == list.contains(6))` |
| `LinkedList<T> LinkedList<T>::split_off(size_t index)` | Splits the linked list into two linked lists at the index.<br />The index value must not be greater than the number of data items in the list.<br />The new list contains the data at the index value and onward.<br />If the index value is 0, the old list becomes empty;<br />if the index value equals the number of data items in the list, the new list becomes empty. | `LinkedList<int> l = LinkedList<int>::new();`<br />`l.push_back(5);`<br />`l.push_back(6);`<br />`l.push_front(4);`<br />`l.push_front(3);`<br />`LinkedList<int> j = l.split_off(2);`<br />`// the list is split into the two lists l and j`<br />`assert(2 == l.length());      // 3 4`<br />`assert(2 == j.length());      // 5 6` |

## `Option`

`Option` is a safe data structure provided by the BiSheng C language for handling optional values, used to represent the two cases in which a value may or may not exist.
It is a generic data structure with one generic parameter `T`, used to represent the type of the optional value it stores. Using `Option`, you can safely handle the return values of some functions, avoiding the use of data structures such as null pointers to represent the case where a value does not exist.
`Option` has two states, namely `Some` and `None`. The `Some` state contains a value of the required type, while the `None` state does not store a corresponding value.
An example of its usage is as follows:

```c
#include "option.hbs" // the Option type is declared in this header file; include this header file when using it

Option<size_t> string_find(String str) {
    size_t pos = str.find('k');
    if (pos == bsc_string_no_pos) {
        return Option<size_t>::None(); // if k is not found, return None
    } else {
        return Option<size_t>::Some(pos); // if k is found, return Some, and Some contains the corresponding value pos
    }
}

void use_option() {
    size_t pos = -1;
    String s = String::from("hello");
    Option<size_t> res = string_find(s);
    if (res.is_some()) { // determine whether res is Some or None
        pos = option_unwrap(res); // for Some, call option_unwrap to get the value inside
    } else {
        return; // for None, do nothing
    }
}
```

In this example, a function `string_find` is first defined, which wraps the `find` method of `String` to handle the cases of finding and not finding the character k. Next, at the call site, the `is_some` method can be used to handle the returned value of type `Option<size_t>`, thereby getting the data inside or doing nothing. This style allows the return values of some functions to be handled safely, improving the safety of the program.

`Option` is a data structure of the _Owned struct type, so it comes with its own destructor, which can safely free the memory space it occupies, without the user needing to free memory manually.

Note: When using `Option<T>`, you need to ensure that T is a type with copy semantics or an _Owned struct type. Otherwise, a compile-time error will occur, because for other types the compiler cannot know how to clean up the memory.

The external interfaces provided by `Option` and the corresponding usage examples are as follows:

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `_Safe _Bool Option<T>::is_none(const Option<T>* _Borrow this)` | Determines whether the `Option` is `None`; returns 1 if so, otherwise 0 | `_Bool flag = option.is_none();` |
| `_Safe _Bool Option<T>::is_some(const Option<T>* _Borrow this)` | Determines whether the `Option` is `Some`; returns 1 if so, otherwise 0 | `_Bool falg = option.is_some();` |
| `_Safe Option<T> Option<T>::Some(T t)` | Creates an `Option` value in the `Some` state, in which the contained result value of type `T` is `t` | `Option<size_t> option = Option<size_t>::Some(3);` |
| `_Safe Option<T> Option<T>::None(void)` | Creates an `Option` value in the `None` state, which does not contain a result value of type `T` | `Option<size_t> option = Option<size_t>::None();` |
| `_Safe T option_unwrap<T>(Option<T> option)` | Unwraps the option, taking out the stored value of type T; for `Some` the value of type `T` can be successfully taken out, while for `None` it causes runtime termination | `size_t index = option_unwrap(option);` |

Note: The `option_unwrap` function is not a member method of the `Option` type, and unwrapping `None` causes runtime termination, so it is recommended to use `is_some` or `is_none` to first check the state of the `Option` before calling this function.

## `HashMap`

`HashMap` is the safe hash map type provided by the BiSheng C language. Both the keys and the values of the hash map are stored on the heap. It is a generic data structure with three generic parameters `K`, `V`, and `S`.
Here `K` represents the type of the key, `V` represents the type of the value, and `S` represents the hash function used by this `HashMap`.

We can use various types as the key type, including all basic types except float and double, as well as custom data types.
When using a type as the key type, that type must implement the two methods `hash` and `equals`. BiSheng C already provides these two methods for the basic types, so the user does not need to implement them manually again.

Note: When using `HashMap<K, V>`, you need to ensure that K and V are types with copy semantics or _Owned struct types. Otherwise, a compile-time error will occur, because for other types the compiler cannot know how to clean up the memory.

As for the hash function, BiSheng C provides a `SipHasher13` as the default hash function. This type has two methods for creating instances of it:

|External Interface|Interface Function|Code Example|
|---|---|---|
|`SipHasher13 SipHasher13::new()`|Creates a default `SipHasher13`|SipHasher13 sh = SipHasher13::new();|
|`SipHasher13 SipHasher13::new_with_keys(uint64_t key0, uint64_t key1)`|Creates a `SipHasher13` using `key0` and `key1`. BiSheng C also provides the `RandomState` type, used to create random `key0` and `key1` values|RandomState rs = RandomState::new();<br/>SipHasher13 sh = SipHasher13::new_with_keys(rs.k0, rs.k1);|

An example of using `HashMap` is as follows:

```c
#include "hash_map.hbs" // the HashMap type is declared in this header file; include this header file when using it

// use a HashMap to count the number of occurrences of each value in an array
void hash_map_example(Vec<int> vec) {
    HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::with_hasher(SipHasher13::new()); // create the HashMap

    for (size_t i = 0; i < vec.length(); i++) {
        const int* _Borrow cur = vec.get(i);
        if (map.contains_key(cur)) { // determine whether cur is already in map
            int* _Borrow v = map.get_mut(cur); // get a mutable borrow of the value corresponding to the key
            *v = *v + 1; // update the corresponding value
        } else {
            int k = *cur;
            map.insert(k, 1); // insert a key-value pair into map
        }
    }

    _Bool empty_flag = map.is_empty(); // determine whether map is empty
    int x = 3;
    _Bool key_flag = map.contains_key(&_Const x); // determine whether map has the key 3
    Option<int> removed = map.remove(&_Const x);
    if (removed.is_some()) {
        int v = option_unwrap(removed);
    }
    map.clear(); // clear all elements of map
}
```

The external interfaces provided by `HashMap` and the corresponding usage examples are as follows:

| External Interface | Interface Function | Code Example |
| --- | --- | --- |
| `_Safe size_t HashMap<K, V, S>::capacity(const HashMap<K, V, S>* _Borrow this)` | Returns the capacity of the hash map | `size_t cap = map.capacity();` |
| `_Safe void HashMap<K, V, S>::clear(HashMap<K, V, S>* _Borrow this)` | Clears all key-value pairs of the hash map, but the capacity of the map remains unchanged | `map.clear();` |
| `_Safe _Bool HashMap<K, V, S>::contains_key(const HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | Determines whether the hash map has a key equal to \*k | `int x = 1;<br/>_Bool flag = map.contains_key(&_Const x);` |
| `_Safe Option<V* _Borrow> HashMap<K, V, S>::get_mut(HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | Gets a mutable borrow of the value corresponding to the key equal to \*k; if the key \*k does not exist in the map, returns None | `int x = 1;<br/>int* _Borrow v = map.get_mut(&_Const x);` |
| `_Safe void HashMap<K, V, S>::reserve(HashMap<K, V, S>* _Borrow this, size_t additional)` | Adjusts the capacity of the hash map so that after adjustment it can store at least `additional` more elements without growing (generally, this method does not need to be called manually; it is implicitly called once when insert is called) | `map.reserve(1);` |
| `_Safe Option<V> HashMap<K, V, S>::insert(HashMap<K, V, S>* _Borrow this, K k, V v)` | Inserts a key-value pair with key k and value v into the hash map | `map.insert(1, 2);` |
| `_Safe _Bool HashMap<K, V, S>::is_empty(const HashMap<K, V, S>* _Borrow this)` | Determines whether the hash map is empty | `_Bool empty = map.is_empty();` |
| `_Safe size_t HashMap<K, V, S>::length(const HashMap<K, V, S>* _Borrow this)` | Returns the number of key-value pairs stored in the hash map | `size_t len = map.length();` |
| `_Safe Option<V> HashMap<K, V, S>::remove(HashMap<K, V, S>* _Borrow this, const K* _Borrow k)` | Removes the key-value pair with key equal to \*k from the hash map and returns the corresponding value; if the key \*k does not exist in the hash map, returns None | `int x = 3;<br/>Option<int> removed = map.remove(&_Const x);<br/>if (removed.is_some()) { int y = option_unwrap(removed); }` |
| `_Safe HashMap<K, V, S> HashMap<K, V, S>::with_capacity_and_hasher(size_t cap, S hash_builder)` | Creates a hash map with initial capacity `cap` and hash function `hash_builder` | `HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::new_with_cap_and_hasher(3, SipHasher13::new());` |
| `_Safe HashMap<K, V, S> HashMap<K, V, S>::with_hasher(S hash_builder)` | Creates a hash map with initial capacity 0 and hash function `hash_builder` | `HashMap<int, int, SipHasher13> map = HashMap<int, int, SipHasher13>::new_with_hasher(SipHasher13::new());` |
