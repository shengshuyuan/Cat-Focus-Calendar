#pragma once
#include <stdint.h>
typedef int nvs_handle_t;
#define ESP_OK 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *, int, nvs_handle_t *);
int nvs_get_i64(nvs_handle_t, const char *, int64_t *);
int nvs_set_i64(nvs_handle_t, const char *, int64_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
