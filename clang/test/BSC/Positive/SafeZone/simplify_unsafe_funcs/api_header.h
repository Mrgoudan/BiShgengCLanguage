// Header file with _Unsafe declarations (public API)
// This simulates a C library header that users include

#ifndef API_HEADER_H
#define API_HEADER_H

// Memory management API
_Unsafe int* create_object(int value);
_Unsafe void destroy_object(int* obj);
_Unsafe int* clone_object(int* obj);

// Data processing API
_Unsafe int process_data(int* data, int size);
_Unsafe void transform_data(int* data, int size);

// String operations
_Unsafe char* allocate_string(int length);
_Unsafe void free_string(char* str);
_Unsafe char* duplicate_string(const char* src);
_Unsafe int string_length(const char* str);

// Buffer operations
_Unsafe void* allocate_buffer(int size);
_Unsafe void free_buffer(void* buf);
_Unsafe void* resize_buffer(void* buf, int old_size, int new_size);

// List operations
struct ListNode {
  int data;
  struct ListNode* next;
};

_Unsafe struct ListNode* list_create(void);
_Unsafe void list_append(struct ListNode* list, int value);
_Unsafe struct ListNode* list_find(struct ListNode* list, int value);
_Unsafe void list_destroy(struct ListNode* list);

// Configuration
_Unsafe void set_config(const char* key, const char* value);
_Unsafe char* get_config(const char* key);

#endif // API_HEADER_H
