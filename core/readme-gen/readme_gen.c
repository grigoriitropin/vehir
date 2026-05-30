// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MARKER "<!-- IDEA-GRAPH -->"
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

static char *run_idea_show(int *out_len) {
    FILE *p = popen("idea show --md 2>/dev/null", "r");
    if (!p) return NULL;
    char *buf = malloc(BUF_SZ);
    if (!buf) { pclose(p); return NULL; }
    *out_len = fread(buf, 1, BUF_SZ - 1, p);
    buf[*out_len] = '\0';
    pclose(p);
    return buf;
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "README.md";

    size_t len;
    char *content = read_file(path, &len);
    if (!content) {
        fprintf(stderr, "readme-gen: cannot read %s\n", path);
        return 1;
    }

    char *marker = strstr(content, MARKER);
    if (!marker) {
        printf("%s", content);
        free(content);
        return 0;
    }

    int graph_len;
    char *graph = run_idea_show(&graph_len);
    if (!graph) {
        fprintf(stderr, "readme-gen: idea show failed\n");
        free(content);
        return 1;
    }

    while (graph_len > 0 && graph[graph_len-1] == '\n')
        graph[--graph_len] = '\0';

    size_t prefix_len = (size_t)(marker - content);
    const char *suffix = marker + strlen(MARKER);

    printf("%.*s\n%s\n%s", (int)prefix_len, content, graph, suffix);

    free(graph);
    free(content);
    return 0;
}
