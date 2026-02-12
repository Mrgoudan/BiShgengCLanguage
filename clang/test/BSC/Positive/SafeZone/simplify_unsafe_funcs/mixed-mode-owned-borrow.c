// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Tests for mixed mode with owned/borrow pointer parameters and returns

safe void drop(int* owned p);
safe void drop_buffer(void* owned p);

// Owned pointer returns
unsafe int* allocate_int(void);
safe int* owned allocate_int(void);

void test_owned_return_unsafe(void) {
  int* owned ptr = allocate_int();
  drop(ptr);
}

safe void test_owned_return_safe(void) {
  int* owned ptr = allocate_int();
  drop(ptr);
}

// Simple parameters
unsafe int read_int(int x);
safe int read_int(int x);

void test_param_unsafe(void) {
  int x = 42;
  int value = read_int(x);
  (void)value;
}

safe void test_param_safe(void) {
  int x = 42;
  int value = read_int(x);
  (void)value;
}

// Owned pointer parameters
unsafe void consume_int(int* p);
safe void consume_int(int* owned p);

void test_owned_param(void) {
  int* owned ptr = allocate_int();
  consume_int(ptr);
}

// Multiple parameters
unsafe void copy_data(int* dest, int src_val, int n);
safe void copy_data(int* owned dest, int src_val, int n);

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

unsafe struct Node get_node(int val);
safe struct Node get_node(int val);

void test_struct_return(void) {
  struct Node node = get_node(42);
  (void)node;
}

// Chain of owned operations
unsafe int* create_buffer(int size);
safe int* owned create_buffer(int size);

unsafe int* resize_buffer(int* buf, int new_size);
safe int* owned resize_buffer(int* owned buf, int new_size);

unsafe void free_buffer(int* buf);
safe void free_buffer(int* owned buf);

void test_owned_chain(void) {
  int* owned buf = create_buffer(10);
  buf = resize_buffer(buf, 20);
  free_buffer(buf);
}

// Owned with values
unsafe void update_value(int* dest, int src);
safe void update_value(int* owned dest, int src);

void test_owned_value(void) {
  int* owned dest = allocate_int();
  int src = 200;
  update_value(dest, src);
}

// Array of pointers
unsafe int** allocate_array(int count);
safe int** owned allocate_array(int count);

unsafe void free_array(int** arr, int count);
safe void free_array(int** owned arr, int count);

void test_array_ptrs(void) {
  int** owned arr = allocate_array(5);
  free_array(arr, 5);
}

// Struct with owned
struct Buffer {
  int* data;
  int size;
};

unsafe struct Buffer* create_buffer_struct(int size);
safe struct Buffer* owned create_buffer_struct(int size);

unsafe void destroy_buffer_struct(struct Buffer* buf);
safe void destroy_buffer_struct(struct Buffer* owned buf);

void test_struct_owned(void) {
  struct Buffer* owned buf = create_buffer_struct(100);
  destroy_buffer_struct(buf);
}

// Conditional with owned
unsafe int* maybe_allocate(int condition);
safe int* owned maybe_allocate(int condition);

void test_conditional_owned(int should_alloc) {
  int* owned ptr = maybe_allocate(should_alloc);
  drop(ptr);
}

// Owned in loop
unsafe int* get_next_item(void);
safe int* owned get_next_item(void);

void test_loop_owned(void) {
  for (int i = 0; i < 3; i++) {
    int* owned item = get_next_item();
    drop(item);
  }
}

// Value in safe context
unsafe void process_value(int p);
safe void process_value(int p);

safe void test_value_safe(void) {
  int x = 123;
  process_value(x);
}

// Multiple owned returns
unsafe int* alloc_a(void);
safe int* owned alloc_a(void);

unsafe int* alloc_b(void);
safe int* owned alloc_b(void);

void test_multi_owned_returns(int which) {
  int* owned ptr = which ? alloc_a() : alloc_b();
  drop(ptr);
}

// Container return
struct Container {
  int* data;
};

unsafe struct Container create_container(void);
safe struct Container create_container(void);

void test_container_return(void) {
  struct Container c = create_container();
  (void)c;
}

// Nested owned transform
unsafe int* transform(int* input);
safe int* owned transform(int* owned input);

void test_nested_transform(void) {
  int* owned ptr = allocate_int();
  ptr = transform(ptr);
  ptr = transform(ptr);
  drop(ptr);
}

// Typedef owned
typedef int* IntPtr;

unsafe IntPtr make_int(void);
safe IntPtr owned make_int(void);

unsafe void take_int(IntPtr p);
safe void take_int(IntPtr owned p);

void test_typedef_owned(void) {
  IntPtr owned ptr = make_int();
  take_int(ptr);
}

// Multiple values
unsafe int compute_sum(int a, int b, int c);
safe int compute_sum(int a, int b, int c);

void test_multi_values(void) {
  int sum = compute_sum(1, 2, 3);
  (void)sum;
}

// Owned function chain
unsafe int* step_a(int* p);
safe int* owned step_a(int* owned p);

unsafe int* step_b(int* p);
safe int* owned step_b(int* owned p);

void test_owned_chain_calls(void) {
  int* owned ptr = allocate_int();
  ptr = step_b(step_a(ptr));
  drop(ptr);
}

// Owned with values
unsafe void set_values(int* dest, int val1, int val2);
safe void set_values(int* owned dest, int val1, int val2);

void test_set_values(void) {
  int* owned dest = allocate_int();
  int a = 10, b = 20;
  set_values(dest, a, b);
}

// Owned in expression
unsafe int* make_ptr(int value);
safe int* owned make_ptr(int value);

unsafe int deref_and_free(int* p);
safe int deref_and_free(int* owned p);

void test_owned_expression(void) {
  int value = deref_and_free(make_ptr(42));
  (void)value;
}

// expected-no-diagnostics
