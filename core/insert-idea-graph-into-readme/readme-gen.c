// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)
// readme-gen — idempotent marker-based graph injection for README.md

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SZ 262144

/* ── file I/O ─────────────────────────────────────────────────────────── */

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

static int write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    return (w == len) ? 0 : -1;
}

/* ── shell command capture ────────────────────────────────────────────── */

static char *run_ipm_show(const char *flags, int *out_len) {
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/readme-gen-%d.mermaid", getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ipm show --md %s > %s 2>/dev/null", flags ? flags : "--short", tmp_path);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "readme-gen: ipm show failed (rc=%d, cmd=%s)\n", rc, cmd);
        return NULL;
    }
    char *buf = malloc(BUF_SZ);
    if (!buf) return NULL;
    FILE *f = fopen(tmp_path, "r");
    if (!f) { free(buf); return NULL; }
    *out_len = (int)fread(buf, 1, BUF_SZ - 1, f);
    fclose(f);
    buf[*out_len] = '\0';
    unlink(tmp_path);
    return buf;
}

/* ── mermaid wrapper strip ────────────────────────────────────────────── */

static char *strip_to_raw_mermaid(char *raw, int *len) {
    /* Strip all leading "# Graph\n" / "# Ideas\n" / "# Programs\n" headers */
    while (*len > 0 && raw[0] == '#') {
        char *nl = memchr(raw, '\n', (size_t)*len);
        if (!nl) break;
        int skip = (int)(nl - raw) + 1;
        memmove(raw, raw + skip, (size_t)(*len - skip));
        *len -= skip;
        raw[*len] = '\0';
    }
    /* Strip leading blank lines */
    while (*len > 0 && raw[0] == '\n') { memmove(raw, raw+1, (size_t)(--(*len))); raw[*len]='\0'; }
    /* Strip opening ```mermaid... fence */
    if (*len > 10 && !strncmp(raw, "```mermaid", 10)) {
        char *nl = memchr(raw, '\n', (size_t)*len);
        if (nl) { int skip = (int)(nl - raw) + 1; memmove(raw, raw+skip, (size_t)(*len-skip)); *len -= skip; raw[*len]='\0'; }
    }
    /* Strip trailing ``` */
    while (*len > 0 && raw[*len-1] == '\n') raw[--(*len)] = '\0';
    if (*len > 3 && !strncmp(raw + *len - 3, "```", 3)) {
        *len -= 3;
        while (*len > 0 && raw[*len-1] == '\n') raw[--(*len)] = '\0';
        raw[*len] = '\0';
    }
    return raw;
}

/* ── idempotent marker block processor ────────────────────────────────── */

static char *process_marker_block(char *content, size_t *content_len,
                                   const char *start_tag, size_t start_tag_len,
                                   const char *end_tag,   size_t end_tag_len,
                                   const char *flags,     const char *label) {
    char *start = strstr(content, start_tag);
    if (!start) return content; /* marker not found — skip */

    char *end = strstr(start + start_tag_len, end_tag);
    if (!end) return content; /* unclosed marker — skip */

    /* Generate fresh graph */
    int raw_len = 0;
    char *raw = run_ipm_show(flags, &raw_len);
    if (!raw) { fprintf(stderr, "readme-gen: ipm show failed for %s\n", label); return content; }
    char *graph = strip_to_raw_mermaid(raw, &raw_len);

    /* Build replacement: start_tag + newline + graph + newline + end_tag */
    size_t insert_len = start_tag_len + 1 + (size_t)raw_len + 1 + end_tag_len;
    char *insert = malloc(insert_len + 2);
    if (!insert) { free(raw); return content; }
    size_t off = 0;
    memcpy(insert + off, start_tag, start_tag_len); off += start_tag_len;
    insert[off++] = '\n';
    if (raw_len > 0) { memcpy(insert + off, graph, (size_t)raw_len); off += (size_t)raw_len; }
    if (off > 0 && insert[off-1] != '\n') insert[off++] = '\n';
    memcpy(insert + off, end_tag, end_tag_len); off += end_tag_len;
    insert[off] = '\0';
    insert_len = off;
    free(raw);

    /* Delete old block, insert new */
    size_t pre_len  = (size_t)(start - content);
    size_t old_block_len = (size_t)(end + end_tag_len - start);
    if (pre_len + old_block_len > *content_len) { free(insert); return content; }
    size_t post_len = *content_len - pre_len - old_block_len;
    size_t new_len  = pre_len + insert_len + post_len;

    char *new_content = malloc(new_len + 1);
    if (!new_content) { free(insert); return content; }
    /* Use volatile copy length to suppress GCC false-positive overflow */
    volatile size_t v_pre = pre_len, v_ins = insert_len, v_post = post_len;
    memcpy(new_content, content, v_pre);
    memcpy(new_content + pre_len, insert, v_ins);
    memcpy(new_content + pre_len + insert_len, start + old_block_len, v_post);

    free(insert);
    free(content);
    *content_len = new_len;
    return new_content;
}

/* ── parse flags from marker attribute ────────────────────────────────── */
/* Marker format: <!-- vehir:graph:<mode> [--short] [--<extra>] --> */
/* End marker:    <!-- /vehir:graph --> */

static const char *MARKER_START = "<!-- vehir:graph:";
static const char *MARKER_END   = "<!-- /vehir:graph -->";

static void parse_mode_and_flags(const char *start_tag, char *mode, size_t mode_sz,
                                  char *flags, size_t flags_sz) {
    const char *p = start_tag + strlen(MARKER_START);
    /* extract mode — word before first space or "--" */
    const char *mode_end = p;
    while (*mode_end && *mode_end != ' ' && *mode_end != '-' && *mode_end != '>') mode_end++;
    if (mode_end > p && (size_t)(mode_end - p) < mode_sz) {
        memcpy(mode, p, (size_t)(mode_end - p));
        mode[mode_end - p] = '\0';
    } else {
        snprintf(mode, mode_sz, "combined");
    }
    /* extract flags — everything after mode until " -->" */
    const char *flag_start = mode_end;
    while (*flag_start == ' ') flag_start++;
    char *flag_out = flags;
    size_t remaining = flags_sz;
    while (*flag_start && !(flag_start[0] == '-' && flag_start[1] == '-' && flag_start[2] == '>')) {
        if (remaining > 1) { *flag_out++ = *flag_start; remaining--; }
        flag_start++;
    }
    *flag_out = '\0';
    /* If no flags specified, default to --short for all modes */
    if (!flags[0]) {
        if (!strcmp(mode, "combined")) snprintf(flags, flags_sz, "--short");
        else if (!strcmp(mode, "ideas")) snprintf(flags, flags_sz, "--ideas --short");
        else if (!strcmp(mode, "programs")) snprintf(flags, flags_sz, "--programs --short");
        else snprintf(flags, flags_sz, "--short");
    }
}

/* ── main ─────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "README.md";

    size_t len;
    char *content = read_file(path, &len);
    if (!content) { fprintf(stderr, "readme-gen: cannot read %s\n", path); return 1; }

    /* Find all vehir:graph markers and process them idempotently */
    int iter = 0;
    while (iter++ < 10) {
        char *tag_start = strstr(content, MARKER_START);
        if (!tag_start) break;
        char *tag_end = strstr(tag_start, "-->");
        if (!tag_end) break;
        size_t tag_len = (size_t)(tag_end + 3 - tag_start);

        /* Build full start_tag string */
        char start_tag[256];
        if (tag_len >= sizeof(start_tag)) break;
        memcpy(start_tag, tag_start, tag_len);
        start_tag[tag_len] = '\0';

        /* Parse mode and flags from the tag itself */
        char mode[64] = {0}, flags[256] = {0};
        parse_mode_and_flags(start_tag, mode, sizeof(mode), flags, sizeof(flags));

        /* Map mode to ipm show flags (if not already specified in extras) */
        char ipm_flags[384];
        if (flags[0]) {
            snprintf(ipm_flags, sizeof(ipm_flags), "%s", flags);
        } else if (!strcmp(mode, "ideas")) {
            snprintf(ipm_flags, sizeof(ipm_flags), "--ideas --short");
        } else if (!strcmp(mode, "programs")) {
            snprintf(ipm_flags, sizeof(ipm_flags), "--programs --short");
        } else {
            snprintf(ipm_flags, sizeof(ipm_flags), "--short");
        }

        content = process_marker_block(content, &len,
                                        start_tag, tag_len,
                                        MARKER_END, strlen(MARKER_END),
                                        ipm_flags, mode);
    }

    write_file(path, content, len);
    free(content);
    return 0;
}
