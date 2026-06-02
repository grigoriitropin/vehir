// SPDX-License-Identifier: Apache-2.0
#define _POSIX_C_SOURCE 200809L
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lib/safe-exec.h"

static char *env_get(const char *path, const char *key, char *out, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[512];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n') continue;
        size_t ll = strlen(p);
        while (ll > 0 && (p[ll-1]=='\n'||p[ll-1]=='\r')) p[--ll]=0;
        if (ll <= kl || p[kl] != '=') continue;
        if (strncmp(p, key, kl) != 0) continue;
        const char *v = p + kl + 1;
        while (*v == ' ' || *v == '\t') v++;
        snprintf(out, sz, "%s", v);
        fclose(f);
        return out;
    }
    fclose(f);
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    const char *cfg = getenv("VEHIR_CONFIG");
    char cfg_path[1024];
    if (!cfg || !cfg[0]) {
        const char *xdg = getenv("XDG_CONFIG_HOME");
        const char *home = getenv("HOME");
        if (xdg && xdg[0])
            snprintf(cfg_path, sizeof(cfg_path), "%s/vehir/env", xdg);
        else if (home && home[0])
            snprintf(cfg_path, sizeof(cfg_path), "%s/.config/vehir/env", home);
        else {
            fprintf(stderr, "git-identity: no config path\n");
            return 1;
        }
    } else {
        snprintf(cfg_path, sizeof(cfg_path), "%s/env", cfg);
    }

    char name[128], email[128];
    int have_name = env_get(cfg_path, "GIT_USER_NAME", name, sizeof(name)) != NULL;
    int have_email = env_get(cfg_path, "GIT_USER_EMAIL", email, sizeof(email)) != NULL;

    if (!have_name && !have_email) {
        fprintf(stderr,
            "git-identity: GIT_USER_NAME or GIT_USER_EMAIL not set in config\n");
        return 1;
    }

    if (have_name) {
        const char *gargs[] = { "git", "config", "--local", "user.name", name, NULL };
        if (!safe_exec(gargs, NULL))
            fprintf(stderr, "git-identity: warning: git config user.name failed\n");
    }
    if (have_email) {
        const char *gargs[] = { "git", "config", "--local", "user.email", email, NULL };
        if (!safe_exec(gargs, NULL))
            fprintf(stderr, "git-identity: warning: git config user.email failed\n");
    }

    printf("{\"ok\":true,\"name\":\"%s\",\"email\":\"%s\"}\n",
           have_name ? name : "(unchanged)",
           have_email ? email : "(unchanged)");
    return 0;
}
