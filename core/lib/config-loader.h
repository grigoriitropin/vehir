// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#pragma once
#include <stddef.h>

typedef struct { char *key; char *val; } cfg_kv_t;

typedef struct { cfg_kv_t *entries; int count; } cfg_t;

cfg_t      *config_load(const char *path, const char *progname);
const char *config_get(const cfg_t *cfg, const char *key);
const char *config_require(const cfg_t *cfg, const char *key, const char *progname);
void        config_free(cfg_t *cfg);
char       *config_default_path(const char *progname);
void        config_die(const char *progname, const char *msg);
