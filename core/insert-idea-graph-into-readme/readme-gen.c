// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)
// readme-gen — idempotent marker-based graph injection for README.md

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
    char cmd[512];
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

    /* Build replacement: start_tag + "\n" + graph + "\n" + end_tag */
    size_t insert_len = start_tag_len + 1 + (size_t)raw_len + 1 + end_tag_len;
    char *insert = malloc(insert_len + 1);
    if (!insert) { free(raw); return content; }
    size_t off = 0;
    memcpy(insert + off, start_tag, start_tag_len); off += start_tag_len;
    insert[off++] = '\n';
    memcpy(insert + off, graph, (size_t)raw_len); off += (size_t)raw_len;
    if (off > 0 && insert[off-1] != '\n') insert[off++] = '\n';
    memcpy(insert + off, end_tag, end_tag_len); off += end_tag_len;
    insert[off] = '\0';
    insert_len = off;
    free(raw);

    /* Delete old block: everything from start_tag through end_tag+end_tag_len */
    size_t pre_len  = (size_t)(start - content);
    size_t old_block_len = (size_t)(end + end_tag_len - start);
    size_t post_len = *content_len - pre_len - old_block_len;
    size_t new_len  = pre_len + insert_len + post_len;

    char *new_content = malloc(new_len + 1);
    if (!new_content) { free(insert); return content; }
    memcpy(new_content, content, pre_len);
    memcpy(new_content + pre_len, insert, insert_len);
    memcpy(new_content + pre_len + insert_len, start + old_block_len, post_len);
    new_content[new_len] = '\0';

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

/* ── generate interactive HTML ────────────────────────────────────────── */

static int generate_interactive_html(const char *path) {
    char *flags_arr[] = { "--short", "--ideas --short", "--programs --short" };
    char *titles[] = { "Combined", "Ideas", "Programs" };
    int n = 3;

    /* Collect all 3 graphs */
    char *graphs[3]; int glens[3];
    for (int i = 0; i < n; i++) {
        graphs[i] = run_ipm_show(flags_arr[i], &glens[i]);
        if (!graphs[i]) glens[i] = 0;
        else { char *g = strip_to_raw_mermaid(graphs[i], &glens[i]); if (g != graphs[i]) memmove(graphs[i], g, (size_t)glens[i]); }
    }

    FILE *f = fopen(path, "w");
    if (!f) { for (int i=0;i<n;i++) free(graphs[i]); return -1; }
    fprintf(f, "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
            "<title>Vehir Interactive Graph</title>"
            "<script src=https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.min.js></script>"
            "<style>"
            "body{background:#0d1117;color:#c9d1d9;font-family:monospace;margin:0;padding:10px}"
            "h2{color:#58a6ff;font-size:13px;margin:16px 0 4px}"
            ".wrap{overflow:hidden;width:100%%;position:relative;cursor:grab;"
            "border:1px solid #30363d;border-radius:6px;background:#161b22}"
            ".mermaid{display:block!important;transform-origin:top left!important}"
            ".mermaid svg{display:block!important;max-width:none!important}"
            "nav{position:sticky;top:0;z-index:10;background:#0d1117;padding:8px 0;"
            "border-bottom:1px solid #30363d;margin-bottom:8px}"
            "nav a{color:#58a6ff;text-decoration:none;margin-right:12px;font-size:12px}"
            "</style></head><body>"
            "<nav><a href=#combined>Combined</a><a href=#ideas>Ideas</a><a href=#programs>Programs</a></nav>");

    char *ids[] = {"combined", "ideas", "programs"};
    for (int i = 0; i < n; i++) {
        fprintf(f, "<h2 id=%s>%s</h2><div class=wrap id=wrap-%s>"
                "<pre class=mermaid>\n%s\n</pre></div>", ids[i], titles[i], ids[i],
                glens[i] ? graphs[i] : "graph TD\n  empty[\"no data\"]");
    }

    fprintf(f, "<script>"
            "mermaid.initialize({startOnLoad:false,securityLevel:'loose',theme:'dark'});"
            "mermaid.run().then(function(){"
            "var ws=document.querySelectorAll('.wrap');"
            "ws.forEach(function(w){"
            "var m=w.querySelector('.mermaid'),s=m&&m.querySelector('svg');"
            "if(!s)return;"
            "var x=0,y=0,z=1,d=false,ax,ay;"
            "function u(){m.style.transform='translate('+x+'px,'+y+'px) scale('+z+')';}"
            "w.addEventListener('wheel',function(e){"
            "e.preventDefault();var f=e.deltaY<0?1.1:0.9;"
            "if(z*f<.01||z*f>30)return;"
            "var R=w.getBoundingClientRect();"
            "x=e.clientX-R.left-(e.clientX-R.left-x)*f;"
            "y=e.clientY-R.top-(e.clientY-R.top-y)*f;z*=f;u();},{passive:false});"
            "w.addEventListener('mousedown',function(e){d=true;ax=e.clientX-x;ay=e.clientY-y;w.style.cursor='grabbing';});"
            "window.addEventListener('mouseup',function(){d=false;w.style.cursor='grab';});"
            "window.addEventListener('mousemove',function(e){if(!d)return;x=e.clientX-ax;y=e.clientY-ay;u();});"
            "});});</script></body></html>");

    fclose(f);
    for (int i = 0; i < n; i++) free(graphs[i]);
    return 0;
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "README.md";

    size_t len;
    char *content = read_file(path, &len);
    if (!content) { fprintf(stderr, "readme-gen: cannot read %s\n", path); return 1; }

    /* Find all vehir:graph markers and process them idempotently */
    while (1) {
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

    /* Insert interactive link before first mermaid block if not already present */
    if (!strstr(content, "interactive-graph.html")) {
        char *first_graph = strstr(content, "```mermaid");
        if (first_graph) {
            char *line_start = first_graph;
            while (line_start > content && line_start[-1] != '\n') line_start--;
            size_t pos = (size_t)(line_start - content);
            char link_line[256];
            snprintf(link_line, sizeof(link_line),
                     "[Open interactive graph (pan/zoom)](./docs/interactive-graph.html)\n\n");
            size_t link_len = strlen(link_line);
            char *new_cont = malloc(len + link_len + 1);
            if (new_cont) {
                memcpy(new_cont, content, pos);
                memcpy(new_cont + pos, link_line, link_len);
                memcpy(new_cont + pos + link_len, content + pos, len - pos);
                new_cont[len + link_len] = '\0';
                free(content);
                content = new_cont;
                len += link_len;
            }
        }
    }

    write_file(path, content, len);

    /* Generate interactive HTML */
    struct stat st;
    if (stat("docs", &st) != 0) mkdir("docs", 0755);
    generate_interactive_html("docs/interactive-graph.html");

    free(content);
    return 0;
}
