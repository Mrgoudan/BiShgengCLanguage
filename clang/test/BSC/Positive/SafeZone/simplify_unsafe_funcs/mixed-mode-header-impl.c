// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Demonstrates realistic API pattern: unsafe declarations in header, safe in implementation

#include "api_header.h"

// Safe implementations for all API functions
safe int* owned create_object(int value);
safe void destroy_object(int* owned obj);
safe int* owned clone_object(int* p);

safe int process_data(int* owned p, int size);
safe void transform_data(int* owned data, int size);

safe char* owned allocate_string(int length);
safe void free_string(char* owned str);
safe char* owned duplicate_string(const char* src);
safe int string_length(const char* str);

safe void* owned allocate_buffer(int size);
safe void free_buffer(void* owned buf);
safe void* owned resize_buffer(void* owned buf, int old_size, int new_size);

safe struct ListNode* owned list_create(void);
safe void list_append(struct ListNode* owned list, int value);
safe struct ListNode* list_find(struct ListNode* owned list, int value);
safe void list_destroy(struct ListNode* owned list);

safe void set_config(const char* key, const char* value);
safe char* owned get_config(const char* key);

// Calls from unsafe context
void test_from_unsafe(void) {
  int* owned obj = create_object(42);
  destroy_object(obj);
}

// Calls from safe context
safe void test_from_safe(void) {
  int* owned obj = create_object(100);
  destroy_object(obj);
}

// String operations
void test_strings(void) {
  char* owned str1 = allocate_string(100);
  int len = string_length("test");
  (void)len;
  free_string(str1);
}

// Buffer operations
void test_buffers(void) {
  void* owned buf = allocate_buffer(1024);
  buf = resize_buffer(buf, 1024, 2048);
  free_buffer(buf);
}

// List operations
void test_lists(void) {
  struct ListNode* owned list = list_create();
  list_append(list, 1);
}

// Configuration
void test_config(void) {
  set_config("debug", "true");
  set_config("max_connections", "100");

  char* owned value = get_config("debug");
  free_string(value);
}

// Chained operations
void test_chained(void) {
  int* owned a = create_object(1);
  destroy_object(a);
}

// Safe context with ownership
safe void test_safe_ownership(void) {
  int* owned obj = create_object(99);
  destroy_object(obj);
}

// Conditional with owned
void test_conditional(int which) {
  int* owned obj;
  if (which) {
    obj = create_object(1);
  } else {
    obj = create_object(2);
  }
  destroy_object(obj);
}

// Loop with owned
void test_loop(void) {
  for (int i = 0; i < 3; i++) {
    int* owned obj = create_object(i);
    destroy_object(obj);
  }
}

// Multiple lists
void test_multi_lists(void) {
  struct ListNode* owned list1 = list_create();
  struct ListNode* owned list2 = list_create();

  list_destroy(list1);
  list_destroy(list2);
}

// Buffer resize chain
void test_buffer_chain(void) {
  void* owned buf = allocate_buffer(100);
  buf = resize_buffer(buf, 100, 200);
  buf = resize_buffer(buf, 200, 400);
  buf = resize_buffer(buf, 400, 800);
  free_buffer(buf);
}

// Config in safe context
safe void test_config_safe(void) {
  set_config("timeout", "30");
  char* owned timeout_val = get_config("timeout");
  free_string(timeout_val);
}

// Data processing
void test_data(void) {
  int* owned data = create_object(100);
  int result = process_data(data, 1);
  (void)result;
}

// Transform consumes owned
void test_transform(void) {
  int* owned data = create_object(50);
  transform_data(data, 1);
}

// String duplication
void test_string_dup(void) {
  char* owned s1 = duplicate_string("hello");
  char* owned s2 = duplicate_string("world");

  free_string(s1);
  free_string(s2);
}

// Multiple creates and destroys
void test_multi_create(void) {
  int* owned obj1 = create_object(1);
  int* owned obj2 = create_object(2);
  int* owned obj3 = create_object(3);

  destroy_object(obj1);
  destroy_object(obj2);
  destroy_object(obj3);
}

// Safe context with buffer
safe void test_safe_buffer(void) {
  void* owned buf = allocate_buffer(512);
  free_buffer(buf);
}

// Nested calls
void test_nested(void) {
  int* owned obj = create_object(42);
  int result = process_data(obj, 1);
  (void)result;
}

// Mixed operations
void test_mixed(void) {
  int* owned obj = create_object(77);
  char* owned str = allocate_string(50);
  void* owned buf = allocate_buffer(256);

  int len = string_length("test");
  (void)len;

  destroy_object(obj);
  free_string(str);
  free_buffer(buf);
}

// expected-no-diagnostics
