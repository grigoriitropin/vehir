// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include "config-loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>

_Noreturn void config_die(const char *progname, const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "%s: FATAL: cJSON alloc failed\n", progname);
        fprintf(stderr, "{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", msg);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { fprintf(stderr, "%s\n", s); free(s); }
    cJSON_Delete(root);
    fprintf(stderr, "%s: %s\n", progname, msg);
    exit(1);
}

char *config_default_path(const char *progname)
{
    const char *home = getenv("HOME");
    if (!home || !home[0])
        config_die(progname, "HOME is not set");
    char *path = malloc(strlen(home) + 32);
    if (!path) config_die(progname, "malloc failed");
    sprintf(path, "%s/.config/vehir/env", home);
    return path;
}

cfg_t *config_load(const char *path, const char *progname)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "config not found: %s: %s", path, strerror(errno));
        config_die(progname, msg);
    }
    if (st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "config %s readable by group/other (mode %04o) — chmod 600 %s",
                 path, (int)(st.st_mode & 0777), path);
        config_die(progname, msg);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "cannot open config: %s: %s", path, strerror(errno));
        config_die(progname, msg);
    }

    cfg_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) { fclose(f); config_die(progname, "malloc failed"); }

    int cap = 8;
    cfg->entries = malloc((size_t)cap * sizeof(cfg_kv_t));
    if (!cfg->entries) { free(cfg); fclose(f); config_die(progname, "malloc failed"); }

    char line[2048];
    while (fgets(line, (int)sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        size_t klen = strlen(key);
        while (klen > 0 && (key[klen-1] == ' ' || key[klen-1] == '\t'))
            key[--klen] = '\0';
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r'
               || val[vlen-1] == ' ' || val[vlen-1] == '\t'))
            val[--vlen] = '\0';
        if (cfg->count >= cap) {
            cap *= 2;
            cfg_kv_t *tmp = realloc(cfg->entries, (size_t)cap * sizeof(cfg_kv_t));
            if (!tmp) {
                config_free(cfg);
                fclose(f);
                config_die(progname, "realloc failed");
            }
            cfg->entries = tmp;
        }
        cfg->entries[cfg->count].key = strdup(key);
        cfg->entries[cfg->count].val = strdup(val);
        if (!cfg->entries[cfg->count].key || !cfg->entries[cfg->count].val) {
            config_free(cfg);
            fclose(f);
            config_die(progname, "strdup failed");
        }
        cfg->count++;
    }
    if (ferror(f)) {
        config_free(cfg);
        fclose(f);
        config_die(progname, "read error on config file");
    }
    fclose(f);
    return cfg;
}

const char *config_get(const cfg_t *cfg, const char *key)
{
    for (int i = 0; i < cfg->count; i++)
        if (strcmp(cfg->entries[i].key, key) == 0)
            return cfg->entries[i].val;
    return NULL;
}

const char *config_require(const cfg_t *cfg, const char *key, const char *progname)
{
    const char *v = config_get(cfg, key);
    if (!v) {
        char msg[256];
        snprintf(msg, sizeof(msg), "key %s not found in config", key);
        config_die(progname, msg);
    }
    return v;
}

void config_free(cfg_t *cfg)
{
    if (!cfg) return;
    for (int i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].key);
        free(cfg->entries[i].val);
    }
    free(cfg->entries);
    free(cfg);
}
