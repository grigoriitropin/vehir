// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <strings.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "lib/config-loader.h"

#define PROGNAME "mail"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buf_t;

typedef struct {
    const char *data;
    size_t len;
    size_t pos;
} upload_t;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    buf_t *buf = userdata;
    size_t total = size * nmemb;
    if (buf->len + total + 1 > buf->cap) {
        size_t newcap = (buf->cap + total + 1) * 2;
        char *tmp = realloc(buf->data, newcap);
        if (!tmp) return 0;
        buf->data = tmp;
        buf->cap = newcap;
    }
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

static buf_t buf_new(void) {
    buf_t b = {0};
    b.cap = 4096;
    b.data = malloc(b.cap);
    if (!b.data) config_die(PROGNAME, "malloc failed");
    b.data[0] = '\0';
    return b;
}

static void buf_append(buf_t *b, const char *s) {
    size_t slen = strlen(s);
    if (b->len + slen + 1 > b->cap) {
        size_t newcap = (b->cap + slen + 1) * 2;
        char *tmp = realloc(b->data, newcap);
        if (!tmp) config_die(PROGNAME, "realloc failed");
        b->data = tmp;
        b->cap = newcap;
    }
    memcpy(b->data + b->len, s, slen);
    b->len += slen;
    b->data[b->len] = '\0';
}

static size_t read_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    upload_t *ud = userdata;
    size_t room = size * nmemb;
    size_t remaining = ud->len - ud->pos;
    size_t copy = (remaining < room) ? remaining : room;
    if (copy == 0) return 0;
    memcpy(ptr, ud->data + ud->pos, copy);
    ud->pos += copy;
    return copy;
}

static char *read_stdin(void) {
    buf_t b = buf_new();
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
        if (b.len + n + 1 > b.cap) {
            size_t newcap = (b.cap + n + 1) * 2;
            char *t = realloc(b.data, newcap);
            if (!t) { free(b.data); config_die(PROGNAME, "realloc failed"); }
            b.data = t;
            b.cap = newcap;
        }
        memcpy(b.data + b.len, tmp, n);
        b.len += n;
        b.data[b.len] = '\0';
    }
    if (ferror(stdin)) { free(b.data); config_die(PROGNAME, "failed to read stdin"); }
    return b.data;
}

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "cannot open file: %s: %s", path, strerror(errno));
        config_die(PROGNAME, msg);
    }
    buf_t b = buf_new();
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) {
        if (b.len + n + 1 > b.cap) {
            size_t newcap = (b.cap + n + 1) * 2;
            char *t = realloc(b.data, newcap);
            if (!t) { free(b.data); fclose(f); config_die(PROGNAME, "realloc failed"); }
            b.data = t;
            b.cap = newcap;
        }
        memcpy(b.data + b.len, tmp, n);
        b.len += n;
        b.data[b.len] = '\0';
    }
    int err = ferror(f);
    fclose(f);
    if (err) { free(b.data); config_die(PROGNAME, "read error on body file"); }
    return b.data;
}

static void cmd_send(const char *smtp_url, const char *user, const char *pass,
                      const char *to, const char *subject, const char *body) {
    char date_buf[128];
    time_t now = time(NULL);
    if (now == (time_t)-1) config_die(PROGNAME, "time() failed");
    struct tm tm_buf;
    if (!localtime_r(&now, &tm_buf)) config_die(PROGNAME, "localtime_r failed");
    if (strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S %z", &tm_buf) == 0)
        config_die(PROGNAME, "strftime failed");

    buf_t msg = buf_new();
    char hdr[2048];
    snprintf(hdr, sizeof(hdr),
        "Date: %s\r\n"
        "From: %s\r\n"
        "To: %s\r\n"
        "Subject: %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "MIME-Version: 1.0\r\n"
        "\r\n",
        date_buf, user, to, subject);
    buf_append(&msg, hdr);
    if (body) buf_append(&msg, body);

    upload_t ud = { .data = msg.data, .len = msg.len, .pos = 0 };

    CURL *curl = curl_easy_init();
    if (!curl) { free(msg.data); config_die(PROGNAME, "curl_easy_init failed"); }

    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, user);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

    struct curl_slist *rcpt = NULL;
    rcpt = curl_slist_append(rcpt, to);
    if (!rcpt) { free(msg.data); curl_easy_cleanup(curl); config_die(PROGNAME, "curl_slist_append failed"); }
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA, &ud);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(rcpt);
    curl_easy_cleanup(curl);
    free(msg.data);

    if (res != CURLE_OK) {
        char errmsg[512];
        snprintf(errmsg, sizeof(errmsg), "SMTP send failed: %s", curl_easy_strerror(res));
        config_die(PROGNAME, errmsg);
    }

    cJSON *out = cJSON_CreateObject();
    if (!out) config_die(PROGNAME, "cJSON alloc failed");
    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "send");
    cJSON_AddStringToObject(out, "to", to);
    cJSON_AddStringToObject(out, "subject", subject);
    cJSON_AddStringToObject(out, "date", date_buf);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!s) config_die(PROGNAME, "cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static int *parse_search_uids(const char *data, int *count) {
    *count = 0;
    const char *p = strstr(data, "SEARCH");
    if (!p) return NULL;
    p += 6;

    int cap = 64;
    int *uids = malloc((size_t)cap * sizeof(int));
    if (!uids) config_die(PROGNAME, "malloc failed");

    while (*p) {
        while (*p == ' ' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0' || *p == '*') break;
        char *end = NULL;
        long val = strtol(p, &end, 10);
        if (end == p) break;
        if (*count >= cap) {
            cap *= 2;
            int *tmp = realloc(uids, (size_t)cap * sizeof(int));
            if (!tmp) { free(uids); config_die(PROGNAME, "realloc failed"); }
            uids = tmp;
        }
        uids[*count] = (int)val;
        (*count)++;
        p = end;
    }
    return uids;
}

static char *extract_header(const char *headers, const char *name) {
    size_t nlen = strlen(name);
    const char *p = headers;
    while (*p) {
        if (strncasecmp(p, name, nlen) == 0 && p[nlen] == ':') {
            const char *val = p + nlen + 1;
            while (*val == ' ' || *val == '\t') val++;
            const char *end = val;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - val);
            char *result = malloc(vlen + 1);
            if (!result) config_die(PROGNAME, "malloc failed");
            memcpy(result, val, vlen);
            result[vlen] = '\0';
            return result;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return NULL;
}

static void cmd_fetch(const char *imap_url, const char *user, const char *pass,
                       const char *mailbox, int unseen_only) {
    char search_url[1024];
    snprintf(search_url, sizeof(search_url), "%s/%s", imap_url, mailbox);

    CURL *curl = curl_easy_init();
    if (!curl) config_die(PROGNAME, "curl_easy_init failed");

    buf_t resp = buf_new();
    curl_easy_setopt(curl, CURLOPT_URL, search_url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, unseen_only ? "SEARCH UNSEEN" : "SEARCH ALL");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "IMAP SEARCH failed: %s", curl_easy_strerror(res));
        free(resp.data);
        config_die(PROGNAME, msg);
    }

    int uid_count = 0;
    int *uids = parse_search_uids(resp.data, &uid_count);
    free(resp.data);

    int limit = 50;
    int start = (uid_count > limit) ? uid_count - limit : 0;

    cJSON *messages = cJSON_CreateArray();
    if (!messages) { free(uids); config_die(PROGNAME, "cJSON alloc failed"); }

    for (int i = start; i < uid_count; i++) {
        char fetch_url[1024];
        snprintf(fetch_url, sizeof(fetch_url), "%s/%s/;UID=%d",
                 imap_url, mailbox, uids[i]);

        CURL *fc = curl_easy_init();
        if (!fc) { free(uids); cJSON_Delete(messages); config_die(PROGNAME, "curl_easy_init failed"); }

        buf_t fbuf = buf_new();
        curl_easy_setopt(fc, CURLOPT_URL, fetch_url);
        curl_easy_setopt(fc, CURLOPT_USERNAME, user);
        curl_easy_setopt(fc, CURLOPT_PASSWORD, pass);
        curl_easy_setopt(fc, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(fc, CURLOPT_WRITEDATA, &fbuf);

        CURLcode fres = curl_easy_perform(fc);
        curl_easy_cleanup(fc);

        if (fres != CURLE_OK) {
            free(fbuf.data);
            continue;
        }

        char *from = extract_header(fbuf.data, "From");
        char *subj = extract_header(fbuf.data, "Subject");
        char *date = extract_header(fbuf.data, "Date");
        free(fbuf.data);

        cJSON *entry = cJSON_CreateObject();
        if (!entry) { free(from); free(subj); free(date); free(uids); cJSON_Delete(messages); config_die(PROGNAME, "cJSON alloc failed"); }
        cJSON_AddNumberToObject(entry, "uid", uids[i]);
        cJSON_AddStringToObject(entry, "from", from ? from : "");
        cJSON_AddStringToObject(entry, "subject", subj ? subj : "");
        cJSON_AddStringToObject(entry, "date", date ? date : "");
        cJSON_AddItemToArray(messages, entry);

        free(from);
        free(subj);
        free(date);
    }

    free(uids);

    cJSON *out = cJSON_CreateObject();
    if (!out) { cJSON_Delete(messages); config_die(PROGNAME, "cJSON alloc failed"); }
    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "fetch");
    cJSON_AddStringToObject(out, "mailbox", mailbox);
    cJSON_AddBoolToObject(out, "unseen_only", unseen_only);
    cJSON_AddNumberToObject(out, "count", cJSON_GetArraySize(messages));
    cJSON_AddItemToObject(out, "messages", messages);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!s) config_die(PROGNAME, "cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--config <path>] <command> [options]\n"
        "\n"
        "Email client (SMTP send + IMAP fetch) via libcurl.\n"
        "\n"
        "Commands:\n"
        "  send --to <addr> --subject <s> [--body-file <f>]    Send email (body from file or stdin)\n"
        "  fetch [--mailbox INBOX] [--unseen]                  Fetch message list (headers)\n"
        "\n"
        "Output: JSON {\"ok\":true/false, ...}\n"
        "Exit:   0 = success, 1 = error, 2 = usage error\n"
        "\n"
        "Config: ~/.config/vehir/env (override with --config <path>)\n"
        "  MAIL_SMTP_URL=<url>      SMTP server (e.g. smtp://smtp.example.com:587)\n"
        "  MAIL_IMAP_URL=<url>      IMAP server (e.g. imaps://imap.example.com:993)\n"
        "  MAIL_USER=<addr>         Email address / username\n"
        "  MAIL_PASS=<pass>         Email password\n"
        "  File must be chmod 600 (owner-only). Credentials never touch process env.\n",
        prog);
    exit(2);
}

static const char *find_opt(int argc, char *argv[], int start, const char *name) {
    for (int i = start; i < argc - 1; i++) {
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    }
    return NULL;
}

static int has_flag(int argc, char *argv[], int start, const char *name) {
    for (int i = start; i < argc; i++) {
        if (strcmp(argv[i], name) == 0) return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
        usage(argv[0]);

    int arg_off = 1;
    char *cfg_path = NULL;

    if (strcmp(argv[1], "--config") == 0) {
        if (argc < 3) usage(argv[0]);
        cfg_path = strdup(argv[2]);
        if (!cfg_path) config_die(PROGNAME, "strdup failed");
        arg_off = 3;
    }

    if (arg_off >= argc) usage(argv[0]);

    if (!cfg_path) cfg_path = config_default_path(PROGNAME);

    cfg_t *cfg = config_load(cfg_path, PROGNAME);
    free(cfg_path);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (strcmp(argv[arg_off], "send") == 0) {
        const char *smtp_url = config_require(cfg, "MAIL_SMTP_URL", PROGNAME);
        const char *user = config_require(cfg, "MAIL_USER", PROGNAME);
        const char *pass = config_require(cfg, "MAIL_PASS", PROGNAME);

        const char *to = find_opt(argc, argv, arg_off + 1, "--to");
        const char *subject = find_opt(argc, argv, arg_off + 1, "--subject");
        if (!to || !subject) {
            fprintf(stderr, "mail: --to and --subject are required for send\n");
            config_free(cfg);
            curl_global_cleanup();
            return 2;
        }

        const char *body_file = find_opt(argc, argv, arg_off + 1, "--body-file");
        char *body = NULL;
        if (body_file) {
            body = read_file_contents(body_file);
        } else {
            body = read_stdin();
        }

        cmd_send(smtp_url, user, pass, to, subject, body);
        free(body);
    } else if (strcmp(argv[arg_off], "fetch") == 0) {
        const char *imap_url = config_require(cfg, "MAIL_IMAP_URL", PROGNAME);
        const char *user = config_require(cfg, "MAIL_USER", PROGNAME);
        const char *pass = config_require(cfg, "MAIL_PASS", PROGNAME);

        const char *mailbox = find_opt(argc, argv, arg_off + 1, "--mailbox");
        if (!mailbox) mailbox = "INBOX";
        int unseen = has_flag(argc, argv, arg_off + 1, "--unseen");

        cmd_fetch(imap_url, user, pass, mailbox, unseen);
    } else {
        fprintf(stderr, "mail: unknown command '%s'\n", argv[arg_off]);
        config_free(cfg);
        curl_global_cleanup();
        return 2;
    }

    config_free(cfg);
    curl_global_cleanup();
    return 0;
}
