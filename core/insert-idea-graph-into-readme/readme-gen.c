// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SZ 131072

static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = malloc(BUF_SZ);
    if (!buf) { fclose(f); return NULL; }
    *len = fread(buf, 1, BUF_SZ - 1, f);
    buf[*len] = '\0';
    fclose(f);
    return buf;
}

static char *run_ipm_show(const char *args, int *out_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ipm show --md %s", args);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char *buf = malloc(BUF_SZ);
    if (!buf) { pclose(fp); return NULL; }
    *out_len = (int)fread(buf, 1, BUF_SZ - 1, fp);
    buf[*out_len] = '\0';
    pclose(fp);
    return buf;
}

static void replace_marker(char **content, size_t *len, const char *marker,
                           const char *graph, int graph_len) {
    char *pos = strstr(*content, marker);
    if (!pos) return;

    size_t pre_len = (size_t)(pos - *content);
    size_t post_len = *len - pre_len - strlen(marker);
    size_t new_len = pre_len + (size_t)graph_len + post_len;

    char *new_buf = malloc(new_len + 1);
    if (!new_buf) return;

    memcpy(new_buf, *content, pre_len);
    memcpy(new_buf + pre_len, graph, (size_t)graph_len);
    memcpy(new_buf + pre_len + (size_t)graph_len, pos + strlen(marker), post_len);
    new_buf[new_len] = '\0';

    free(*content);
    *content = new_buf;
    *len = new_len;
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "README.md";

    size_t len;
    char *content = read_file(path, &len);
    if (!content) { fprintf(stderr, "readme-gen: cannot read %s\n", path); return 1; }

    /* Combined graph */
    char *combined = NULL; int clen = 0;
    if (strstr(content, "<!-- IDEA-GRAPH -->")) {
        combined = run_ipm_show("--short", &clen);
        if (!combined) { fprintf(stderr, "readme-gen: ipm show --md failed\n"); free(content); return 1; }
        while (clen > 0 && combined[clen-1] == '\n') combined[--clen] = '\0';
        replace_marker(&content, &len, "<!-- IDEA-GRAPH -->", combined, clen);
        replace_marker(&content, &len, "<!-- VEHIR-GRAPH -->", combined, clen);
    }

    /* Ideas graph */
    int ilen;
    char *ideas = run_ipm_show("--ideas --short", &ilen);
    if (ideas) {
        while (ilen > 0 && ideas[ilen-1] == '\n') ideas[--ilen] = '\0';
        replace_marker(&content, &len, "<!-- IDEA-GRAPH: ideas -->", ideas, ilen);
    }

    /* Programs graph */
    int plen;
    char *progs = run_ipm_show("--programs --short", &plen);
    if (progs) {
        while (plen > 0 && progs[plen-1] == '\n') progs[--plen] = '\0';
        replace_marker(&content, &len, "<!-- IDEA-GRAPH: programs -->", progs, plen);
    }

    printf("%s\n", content);

    free(combined); free(ideas); free(progs);
    free(content);
    return 0;
}
