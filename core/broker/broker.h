// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir
// broker.h — secrets and configuration broker

#ifndef VEHIR_BROKER_H
#define VEHIR_BROKER_H

typedef struct {
    char *key;
    char *val;
} br_kv_t;

typedef struct {
    br_kv_t *entries;
    int count;
} br_cfg_t;

/* Load config from ~/.config/vehir/env or path_override. Returns NULL if not found or bad perms. */
br_cfg_t *broker_load(const char *path_override);

/* Get value by key. Returns NULL if not found. */
const char *broker_get(const br_cfg_t *cfg, const char *key);

/* Free loaded config */
void broker_free(br_cfg_t *cfg);

/* Override config directory (relative to HOME). Call before broker_load(). */
void broker_set_dir(const char *dir);

#endif
