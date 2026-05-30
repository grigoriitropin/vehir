// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include "runtime_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int safe_copy(char *dst, size_t dstsz, const char *src) {
    int n = snprintf(dst, dstsz, "%s", src);
    return (n > 0 && (size_t)n < dstsz) ? 0 : -1;
}

static int join2(char *dst, size_t dstsz, const char *a, const char *b) {
    int n = snprintf(dst, dstsz, "%s/%s", a, b);
    return (n > 0 && (size_t)n < dstsz) ? 0 : -1;
}

static int resolve_bin(vehir_paths *p) {
    char exe[VEHIR_PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return -1;
    exe[n] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash) return -1;
    *slash = '\0';
    return safe_copy(p->bin, sizeof(p->bin), exe);
}

static int resolve_root(vehir_paths *p) {
    const char *env = getenv("VEHIR_ROOT");
    if (env && env[0])
        return safe_copy(p->root, sizeof(p->root), env);

    if (p->bin[0] && strncmp(p->bin, "/nix/store", 10) != 0) {
        char tmp[VEHIR_PATH_MAX];
        if (safe_copy(tmp, sizeof(tmp), p->bin) != 0) return -1;
        char *slash = strrchr(tmp, '/');
        if (slash && slash != tmp) {
            *slash = '\0';
            return safe_copy(p->root, sizeof(p->root), tmp);
        }
    }

    if (!getcwd(p->root, sizeof(p->root))) return -1;
    return 0;
}

static int resolve_xdg(char *dst, size_t dstsz,
                        const char *env_override,
                        const char *xdg_var,
                        const char *xdg_default_suffix,
                        const char *subdir) {
    const char *env = getenv(env_override);
    if (env && env[0])
        return safe_copy(dst, dstsz, env);

    const char *xdg = getenv(xdg_var);
    if (xdg && xdg[0])
        return join2(dst, dstsz, xdg, subdir);

    const char *home = getenv("HOME");
    if (!home || !home[0]) return -1;

    char base[VEHIR_PATH_MAX];
    if (join2(base, sizeof(base), home, xdg_default_suffix) != 0) return -1;
    return join2(dst, dstsz, base, subdir);
}

static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        mkdir(path, 0755);
}

int vehir_paths_init(vehir_paths *p) {
    if (!p) return -1;
    memset(p, 0, sizeof(*p));

    resolve_bin(p);
    if (resolve_root(p) != 0) return -1;

    const char *scratch_env = getenv("VEHIR_SCRATCH");
    if (scratch_env && scratch_env[0]) {
        if (safe_copy(p->scratch, sizeof(p->scratch), scratch_env) != 0)
            return -1;
    } else {
        if (join2(p->scratch, sizeof(p->scratch), p->root, "scratch") != 0)
            return -1;
    }

    if (!p->bin[0]) {
        if (join2(p->bin, sizeof(p->bin), p->root, "bin") != 0)
            return -1;
    }

    if (resolve_xdg(p->config, sizeof(p->config),
                     "VEHIR_CONFIG", "XDG_CONFIG_HOME",
                     ".config", "vehir") != 0)
        return -1;

    if (resolve_xdg(p->state, sizeof(p->state),
                     "VEHIR_STATE_DIR", "XDG_STATE_HOME",
                     ".local/state", "vehir") != 0)
        return -1;

    const char *ct = getenv("VEHIR_CONTAINER");
    p->container = (ct && strcmp(ct, "1") == 0) ? 1 : 0;

    ensure_dir(p->scratch);
    return 0;
}

int vehir_paths_socket(const vehir_paths *p, const char *service,
                       char *dst, size_t dstsz) {
    if (!p || !service || !dst || !p->scratch[0]) return -1;
    int n = snprintf(dst, dstsz, "%s/%s.sock", p->scratch, service);
    return (n > 0 && (size_t)n < dstsz) ? 0 : -1;
}

int vehir_paths_subpath(const vehir_paths *p, const char *rel,
                        char *dst, size_t dstsz) {
    if (!p || !rel || !dst || !p->root[0]) return -1;
    return join2(dst, dstsz, p->root, rel);
}
