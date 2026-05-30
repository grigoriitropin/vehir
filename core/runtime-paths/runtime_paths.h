#pragma once
#include <stddef.h>

#define VEHIR_PATH_MAX 1024

typedef struct {
    char root[VEHIR_PATH_MAX];
    char bin[VEHIR_PATH_MAX];
    char scratch[VEHIR_PATH_MAX];
    char config[VEHIR_PATH_MAX];
    char state[VEHIR_PATH_MAX];
    int  container;
} vehir_paths;

int vehir_paths_init(vehir_paths *p);
int vehir_paths_socket(const vehir_paths *p, const char *service,
                       char *dst, size_t dstsz);
int vehir_paths_subpath(const vehir_paths *p, const char *rel,
                        char *dst, size_t dstsz);
