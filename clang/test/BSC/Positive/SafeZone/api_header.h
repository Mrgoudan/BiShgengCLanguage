// Header file with unsafe declarations (public API)
// This simulates a C library header that users include

#ifndef API_HEADER_H
#define API_HEADER_H

// Memory management API
unsafe int* create_object(int value);
unsafe void destroy_object(int* obj);
unsafe int* clone_object(int* obj);

// Data processing API
unsafe int process_data(int* data, int size);
unsafe void transform_data(int* data, int size);

// String operations
unsafe char* allocate_string(int length);
unsafe void free_string(char* str);
unsafe char* duplicate_string(const char* src);
unsafe int string_length(const char* str);

// Buffer operations
unsafe void* allocate_buffer(int size);
unsafe void free_buffer(void* buf);
unsafe void* resize_buffer(void* buf, int old_size, int new_size);

// List operations
struct ListNode {
  int data;
  struct ListNode* next;
};

unsafe struct ListNode* list_create(void);
unsafe void list_append(struct ListNode* list, int value);
unsafe struct ListNode* list_find(struct ListNode* list, int value);
unsafe void list_destroy(struct ListNode* list);

// Configuration
unsafe void set_config(const char* key, const char* value);
unsafe char* get_config(const char* key);

#endif // API_HEADER_H
