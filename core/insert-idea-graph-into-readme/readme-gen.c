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

static char *run_ipm_show(int *out_len) {
    FILE *fp = popen("ipm show --md", "r");
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
    if (!content) {
        fprintf(stderr, "readme-gen: cannot read %s\n", path);
        return 1;
    }

    int graph_len;
    char *graph = run_ipm_show(&graph_len);
    if (!graph) {
        fprintf(stderr, "readme-gen: ipm show --md failed\n");
        free(content);
        return 1;
    }

    while (graph_len > 0 && graph[graph_len-1] == '\n')
        graph[--graph_len] = '\0';

    replace_marker(&content, &len, "<!-- IDEA-GRAPH -->", graph, graph_len);
    replace_marker(&content, &len, "<!-- VEHIR-GRAPH -->", graph, graph_len);

    printf("%s\n", content);

    free(graph);
    free(content);
    return 0;
}
