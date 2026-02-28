// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Demonstrates realistic API pattern: _Unsafe declarations in header, _Safe in implementation

#include "api_header.h"

// Safe implementations for all API functions
_Safe int* _Owned create_object(int value);
_Safe void destroy_object(int* _Owned obj);
_Safe int* _Owned clone_object(int* p);

_Safe int process_data(int* _Owned p, int size);
_Safe void transform_data(int* _Owned data, int size);

_Safe char* _Owned allocate_string(int length);
_Safe void free_string(char* _Owned str);
_Safe char* _Owned duplicate_string(const char* src);
_Safe int string_length(const char* str);

_Safe void* _Owned allocate_buffer(int size);
_Safe void free_buffer(void* _Owned buf);
_Safe void* _Owned resize_buffer(void* _Owned buf, int old_size, int new_size);

_Safe struct ListNode* _Owned list_create(void);
_Safe void list_append(struct ListNode* _Owned list, int value);
_Safe struct ListNode* list_find(struct ListNode* _Owned list, int value);
_Safe void list_destroy(struct ListNode* _Owned list);

_Safe void set_config(const char* key, const char* value);
_Safe char* _Owned get_config(const char* key);

// Calls from _Unsafe context
void test_from_unsafe(void) {
  int* _Owned obj = create_object(42);
  destroy_object(obj);
}

// Calls from _Safe context
_Safe void test_from_safe(void) {
  int* _Owned obj = create_object(100);
  destroy_object(obj);
}

// String operations
void test_strings(void) {
  char* _Owned str1 = allocate_string(100);
  int len = string_length("test");
  (void)len;
  free_string(str1);
}

// Buffer operations
void test_buffers(void) {
  void* _Owned buf = allocate_buffer(1024);
  buf = resize_buffer(buf, 1024, 2048);
  free_buffer(buf);
}

// List operations
void test_lists(void) {
  struct ListNode* _Owned list = list_create();
  list_append(list, 1);
}

// Configuration
void test_config(void) {
  set_config("debug", "true");
  set_config("max_connections", "100");

  char* _Owned value = get_config("debug");
  free_string(value);
}

// Chained operations
void test_chained(void) {
  int* _Owned a = create_object(1);
  destroy_object(a);
}

// Safe context with ownership
_Safe void test_safe_ownership(void) {
  int* _Owned obj = create_object(99);
  destroy_object(obj);
}

// Conditional with _Owned
void test_conditional(int which) {
  int* _Owned obj;
  if (which) {
    obj = create_object(1);
  } else {
    obj = create_object(2);
  }
  destroy_object(obj);
}

// Loop with _Owned
void test_loop(void) {
  for (int i = 0; i < 3; i++) {
    int* _Owned obj = create_object(i);
    destroy_object(obj);
  }
}

// Multiple lists
void test_multi_lists(void) {
  struct ListNode* _Owned list1 = list_create();
  struct ListNode* _Owned list2 = list_create();

  list_destroy(list1);
  list_destroy(list2);
}

// Buffer resize chain
void test_buffer_chain(void) {
  void* _Owned buf = allocate_buffer(100);
  buf = resize_buffer(buf, 100, 200);
  buf = resize_buffer(buf, 200, 400);
  buf = resize_buffer(buf, 400, 800);
  free_buffer(buf);
}

// Config in _Safe context
_Safe void test_config_safe(void) {
  set_config("timeout", "30");
  char* _Owned timeout_val = get_config("timeout");
  free_string(timeout_val);
}

// Data processing
void test_data(void) {
  int* _Owned data = create_object(100);
  int result = process_data(data, 1);
  (void)result;
}

// Transform consumes _Owned
void test_transform(void) {
  int* _Owned data = create_object(50);
  transform_data(data, 1);
}

// String duplication
void test_string_dup(void) {
  char* _Owned s1 = duplicate_string("hello");
  char* _Owned s2 = duplicate_string("world");

  free_string(s1);
  free_string(s2);
}

// Multiple creates and destroys
void test_multi_create(void) {
  int* _Owned obj1 = create_object(1);
  int* _Owned obj2 = create_object(2);
  int* _Owned obj3 = create_object(3);

  destroy_object(obj1);
  destroy_object(obj2);
  destroy_object(obj3);
}

// Safe context with buffer
_Safe void test_safe_buffer(void) {
  void* _Owned buf = allocate_buffer(512);
  free_buffer(buf);
}

// Nested calls
void test_nested(void) {
  int* _Owned obj = create_object(42);
  int result = process_data(obj, 1);
  (void)result;
}

// Mixed operations
void test_mixed(void) {
  int* _Owned obj = create_object(77);
  char* _Owned str = allocate_string(50);
  void* _Owned buf = allocate_buffer(256);

  int len = string_length("test");
  (void)len;

  destroy_object(obj);
  free_string(str);
  free_buffer(buf);
}

// expected-no-diagnostics
