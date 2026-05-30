#pragma once
#include <stddef.h>

#define VEHR_PAIN_MAX 64

typedef struct vehir_db vehir_db;

typedef struct {
    char domain[64];
    char signal[64];
    double severity;
    char description[512];
} vehir_pain_entry;

typedef struct {
    int count;
    double max_pain;
    vehir_pain_entry entries[VEHR_PAIN_MAX];
} vehir_pain_state;

int vehir_pain_signal(vehir_db *db, const char *domain, const char *signal,
                      double severity, const char *source,
                      const char *description);

int vehir_pain_ok(vehir_db *db, const char *domain, const char *signal);

int vehir_pain_compute(vehir_db *db, vehir_pain_state *out);

const char *vehir_pain_mode(double pain);
