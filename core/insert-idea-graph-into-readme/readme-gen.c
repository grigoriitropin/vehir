// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SZ 131072
#define MARKER_PREFIX "<!-- INSERT-IDEA-GRAPH"
#define MARKER_SUFFIX "-->"

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

/* Resolves marker type to ipm show --md flags */
static const char *mode_to_flags(const char *mode) {
    if (!mode || !mode[0])         return "--short";
    if (!strcmp(mode, "ideas"))    return "--ideas --short";
    if (!strcmp(mode, "programs")) return "--programs --short";
    if (!strcmp(mode, "full"))     return "";
    if (!strcmp(mode, "ideas-full"))    return "--ideas";
    if (!strcmp(mode, "programs-full")) return "--programs";
    return "--short"; /* unknown → default combined short */
}

static char *run_ipm_show(const char *flags, int *out_len) {
    char cmd[384];
    snprintf(cmd, sizeof(cmd), "ipm show --md %s", flags ? flags : "--short");
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char *buf = malloc(BUF_SZ);
    if (!buf) { pclose(fp); return NULL; }
    *out_len = (int)fread(buf, 1, BUF_SZ - 1, fp);
    buf[*out_len] = '\0';
    pclose(fp);
    return buf;
}

/* Strip "# Graph\n" header and ```mermaid wrapper, keep pure mermaid content */
static char *strip_mermaid_wrapper(char *raw, int *out_len) {
    int len = *out_len;
    /* skip "# Graph\n" if present */
    if (len > 8 && !strncmp(raw, "# Graph\n", 8)) {
        memmove(raw, raw + 8, (size_t)(len - 8));
        len -= 8;
        raw[len] = '\0';
    }
    /* skip leading blank line + ```mermaid\n if present */
    while (len > 0 && raw[0] == '\n') { memmove(raw, raw + 1, (size_t)(--len)); raw[len] = '\0'; }
    if (len > 11 && !strncmp(raw, "```mermaid\n", 11)) {
        memmove(raw, raw + 11, (size_t)(len - 11));
        len -= 11;
        raw[len] = '\0';
    }
    /* strip trailing \n``` */
    while (len > 0 && raw[len-1] == '\n') raw[--len] = '\0';
    if (len > 3 && !strncmp(raw + len - 3, "```", 3)) {
        len -= 3;
        while (len > 0 && raw[len-1] == '\n') raw[--len] = '\0';
        raw[len] = '\0';
    }
    *out_len = len;
    return raw;
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

/* Parse "<!-- INSERT-IDEA-GRAPH: mode -->" — returns ": mode" part or NULL */
static char *find_marker(char *content) {
    char *start = strstr(content, MARKER_PREFIX);
    if (!start) return NULL;
    char *end = strstr(start, MARKER_SUFFIX);
    if (!end) return NULL;
    size_t total = (size_t)(end + strlen(MARKER_SUFFIX) - start);
    char *marker = malloc(total + 1);
    if (!marker) return NULL;
    memcpy(marker, start, total);
    marker[total] = '\0';
    return marker;
}

static char *extract_mode(const char *marker) {
    const char *p = marker + strlen(MARKER_PREFIX);
    while (*p == ' ' || *p == ':') p++;
    if (!*p || !strcmp(p, "-->")) return NULL;
    char *mode = strdup(p);
    /* trim trailing space/--> */
    char *e = strstr(mode, " -->");
    if (!e) e = strstr(mode, "-->");
    if (e) *e = '\0';
    while (e > mode && e[-1] == ' ') *--e = '\0';
    return mode;
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "README.md";

    size_t len;
    char *content = read_file(path, &len);
    if (!content) { fprintf(stderr, "readme-gen: cannot read %s\n", path); return 1; }

    /* Find all markers and process them */
    char *work = content;
    while (1) {
        char *raw_marker = find_marker(work);
        if (!raw_marker) break;

        char *mode = extract_mode(raw_marker);
        const char *flags = mode_to_flags(mode);
        int glen;
        char *raw = run_ipm_show(flags, &glen);
        if (!raw) { fprintf(stderr, "readme-gen: ipm show --md %s failed\n", flags); free(raw_marker); free(mode); free(content); return 1; }
        char *graph = strip_mermaid_wrapper(raw, &glen);

        replace_marker(&content, &len, raw_marker, graph, glen);

        free(raw_marker);
        free(mode);
        /* content was reallocated, restart search from beginning */
        work = content;
    }

    printf("%s\n", content);
    free(content);
    return 0;
}
