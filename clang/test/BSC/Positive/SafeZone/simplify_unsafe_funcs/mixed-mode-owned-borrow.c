// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Tests for mixed mode with owned/borrow pointer parameters and returns

_Safe void drop(int* owned p);
_Safe void drop_buffer(void* owned p);

// Owned pointer returns
_Unsafe int* allocate_int(void);
_Safe int* owned allocate_int(void);

void test_owned_return_unsafe(void) {
  int* owned ptr = allocate_int();
  drop(ptr);
}

_Safe void test_owned_return_safe(void) {
  int* owned ptr = allocate_int();
  drop(ptr);
}

// Simple parameters
_Unsafe int read_int(int x);
_Safe int read_int(int x);

void test_param_unsafe(void) {
  int x = 42;
  int value = read_int(x);
  (void)value;
}

_Safe void test_param_safe(void) {
  int x = 42;
  int value = read_int(x);
  (void)value;
}

// Owned pointer parameters
_Unsafe void consume_int(int* p);
_Safe void consume_int(int* owned p);

void test_owned_param(void) {
  int* owned ptr = allocate_int();
  consume_int(ptr);
}

// Multiple parameters
_Unsafe void copy_data(int* dest, int src_val, int n);
_Safe void copy_data(int* owned dest, int src_val, int n);

void test_multi_params(void) {
  int* owned dest = allocate_int();
  int src = 100;
  copy_data(dest, src, 1);
}

// Struct return
struct Node {
  int value;
  int next_value;
};

_Unsafe struct Node get_node(int val);
_Safe struct Node get_node(int val);

void test_struct_return(void) {
  struct Node node = get_node(42);
  (void)node;
}

// Chain of owned operations
_Unsafe int* create_buffer(int size);
_Safe int* owned create_buffer(int size);

_Unsafe int* resize_buffer(int* buf, int new_size);
_Safe int* owned resize_buffer(int* owned buf, int new_size);

_Unsafe void free_buffer(int* buf);
_Safe void free_buffer(int* owned buf);

void test_owned_chain(void) {
  int* owned buf = create_buffer(10);
  buf = resize_buffer(buf, 20);
  free_buffer(buf);
}

// Owned with values
_Unsafe void update_value(int* dest, int src);
_Safe void update_value(int* owned dest, int src);

void test_owned_value(void) {
  int* owned dest = allocate_int();
  int src = 200;
  update_value(dest, src);
}

// Array of pointers
_Unsafe int** allocate_array(int count);
_Safe int** owned allocate_array(int count);

_Unsafe void free_array(int** arr, int count);
_Safe void free_array(int** owned arr, int count);

void test_array_ptrs(void) {
  int** owned arr = allocate_array(5);
  free_array(arr, 5);
}

// Struct with owned
struct Buffer {
  int* data;
  int size;
};

_Unsafe struct Buffer* create_buffer_struct(int size);
_Safe struct Buffer* owned create_buffer_struct(int size);

_Unsafe void destroy_buffer_struct(struct Buffer* buf);
_Safe void destroy_buffer_struct(struct Buffer* owned buf);

void test_struct_owned(void) {
  struct Buffer* owned buf = create_buffer_struct(100);
  destroy_buffer_struct(buf);
}

// Conditional with owned
_Unsafe int* maybe_allocate(int condition);
_Safe int* owned maybe_allocate(int condition);

void test_conditional_owned(int should_alloc) {
  int* owned ptr = maybe_allocate(should_alloc);
  drop(ptr);
}

// Owned in loop
_Unsafe int* get_next_item(void);
_Safe int* owned get_next_item(void);

void test_loop_owned(void) {
  for (int i = 0; i < 3; i++) {
    int* owned item = get_next_item();
    drop(item);
  }
}

// Value in _Safe context
_Unsafe void process_value(int p);
_Safe void process_value(int p);

_Safe void test_value_safe(void) {
  int x = 123;
  process_value(x);
}

// Multiple owned returns
_Unsafe int* alloc_a(void);
_Safe int* owned alloc_a(void);

_Unsafe int* alloc_b(void);
_Safe int* owned alloc_b(void);

void test_multi_owned_returns(int which) {
  int* owned ptr = which ? alloc_a() : alloc_b();
  drop(ptr);
}

// Container return
struct Container {
  int* data;
};

_Unsafe struct Container create_container(void);
_Safe struct Container create_container(void);

void test_container_return(void) {
  struct Container c = create_container();
  (void)c;
}

// Nested owned transform
_Unsafe int* transform(int* input);
_Safe int* owned transform(int* owned input);

void test_nested_transform(void) {
  int* owned ptr = allocate_int();
  ptr = transform(ptr);
  ptr = transform(ptr);
  drop(ptr);
}

// Typedef owned
typedef int* IntPtr;

_Unsafe IntPtr make_int(void);
_Safe IntPtr owned make_int(void);

_Unsafe void take_int(IntPtr p);
_Safe void take_int(IntPtr owned p);

void test_typedef_owned(void) {
  IntPtr owned ptr = make_int();
  take_int(ptr);
}

// Multiple values
_Unsafe int compute_sum(int a, int b, int c);
_Safe int compute_sum(int a, int b, int c);

void test_multi_values(void) {
  int sum = compute_sum(1, 2, 3);
  (void)sum;
}

// Owned function chain
_Unsafe int* step_a(int* p);
_Safe int* owned step_a(int* owned p);

_Unsafe int* step_b(int* p);
_Safe int* owned step_b(int* owned p);

void test_owned_chain_calls(void) {
  int* owned ptr = allocate_int();
  ptr = step_b(step_a(ptr));
  drop(ptr);
}

// Owned with values
_Unsafe void set_values(int* dest, int val1, int val2);
_Safe void set_values(int* owned dest, int val1, int val2);

void test_set_values(void) {
  int* owned dest = allocate_int();
  int a = 10, b = 20;
  set_values(dest, a, b);
}

// Owned in expression
_Unsafe int* make_ptr(int value);
_Safe int* owned make_ptr(int value);

_Unsafe int deref_and_free(int* p);
_Safe int deref_and_free(int* owned p);

void test_owned_expression(void) {
  int value = deref_and_free(make_ptr(42));
  (void)value;
}

// expected-no-diagnostics
