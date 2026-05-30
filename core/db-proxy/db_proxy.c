// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include <ctype.h>
#include <libpq-fe.h>
#include <cjson/cJSON.h>
#include "json_esc.h"

#define PATH_SZ     1024
#define REQ_SZ     65536
#define RESP_SZ   524288
#define PARAM_MAX     32
#define PARAM_SZ    4096

static PGconn *db_conn = NULL;
static volatile sig_atomic_t running = 1;
static char sock_path[PATH_SZ];

_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wformat-truncation\"")

static void set_sock_default(void) {
    const char *env = getenv("VEHIR_DB_PROXY_SOCK");
    if (env && env[0]) {
        snprintf(sock_path, sizeof(sock_path), "%s", env);
        return;
    }
    const char *scratch = getenv("VEHIR_SCRATCH");
    char scratch_buf[PATH_SZ];
    if (!scratch || !scratch[0]) {
        const char *home = getenv("HOME");
        snprintf(scratch_buf, sizeof(scratch_buf),
                 "%s/.local/state/vehir/scratch",
                 home && home[0] ? home : "/tmp");
        scratch = scratch_buf;
    }
    snprintf(sock_path, sizeof(sock_path), "%s/db-proxy.sock", scratch);
}

_Pragma("GCC diagnostic pop")

static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        mkdir(path, 0755);
}

static char *build_connstr(void) {
    static char buf[2048];

    const char *pg_conn = getenv("VEHIR_PG_CONN");
    if (pg_conn && pg_conn[0]) {
        snprintf(buf, sizeof(buf), "%s", pg_conn);
        return buf;
    }

    const char *db_url = getenv("DATABASE_URL");
    if (db_url && db_url[0]) {
        snprintf(buf, sizeof(buf), "%s", db_url);
        return buf;
    }

    const char *state  = getenv("VEHIR_STATE_DIR");
    const char *home   = getenv("HOME");
    char state_buf[PATH_SZ];

    if (!state || !state[0]) {
        snprintf(state_buf, sizeof(state_buf),
                 "%s/.local/state/vehir",
                 home && home[0] ? home : "/tmp");
        state = state_buf;
    }

    const char *pg_host = getenv("PGHOST");
    const char *pg_port = getenv("PGPORT");
    const char *pg_user = getenv("PGUSER");
    const char *pg_db   = getenv("PGDATABASE");
    const char *port = pg_port && pg_port[0] ? pg_port : "5432";
    const char *user = pg_user && pg_user[0] ? pg_user : "vehir";
    const char *dbname = pg_db && pg_db[0] ? pg_db : "vehir_memory";

    if (pg_host && pg_host[0]) {
        snprintf(buf, sizeof(buf),
                 "host=%s port=%s dbname=%s user=%s connect_timeout=3",
                 pg_host, port, dbname, user);
    } else {
        snprintf(buf, sizeof(buf),
                 "host=%s/pg_nix port=%s dbname=%s user=%s connect_timeout=3",
                 state, port, dbname, user);
    }
    return buf;
}

static int db_connect(void) {
    char *cs = build_connstr();
    if (!cs) return -1;
    fprintf(stderr, "[db-proxy] connecting: %s\n", cs);

    db_conn = PQconnectdb(cs);
    if (PQstatus(db_conn) == CONNECTION_OK) {
        fprintf(stderr, "[db-proxy] connected\n");
        return 0;
    }
    fprintf(stderr, "[db-proxy] connection failed: %s\n",
            PQerrorMessage(db_conn));
    PQfinish(db_conn);
    db_conn = NULL;
    return -1;
}

static void db_ensure(void) {
    if (db_conn && PQstatus(db_conn) == CONNECTION_OK) return;
    if (db_conn) { PQfinish(db_conn); db_conn = NULL; }
    db_connect();
}

static void send_err(int fd, const char *msg) {
    char esc[512];
    json_esc(msg, esc, sizeof(esc));
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
                     "{\"ok\":false,\"error\":\"%s\"}\n", esc);
    ssize_t wr = write(fd, buf, (size_t)n);
    if (wr < 0) fprintf(stderr, "[db-proxy] write error: %s\n", strerror(errno));
}

static void send_ok_exec(int fd, int nrows) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
                     "{\"ok\":true,\"nrows\":%d}\n", nrows);
    ssize_t wr = write(fd, buf, (size_t)n);
    if (wr < 0) fprintf(stderr, "[db-proxy] write error: %s\n", strerror(errno));
}

static void send_ok_query(int fd, PGresult *r) {
    int nrows = PQntuples(r);
    int ncols = PQnfields(r);

    char *buf = malloc(RESP_SZ);
    if (!buf) { send_err(fd, "oom"); return; }

    int off = snprintf(buf, RESP_SZ,
                       "{\"ok\":true,\"nrows\":%d,\"ncols\":%d,\"cols\":[",
                       nrows, ncols);
    for (int j = 0; j < ncols; j++) {
        if (j) buf[off++] = ',';
        char esc[128];
        json_esc(PQfname(r, j), esc, sizeof(esc));
        off += snprintf(buf + off, RESP_SZ - off, "\"%s\"", esc);
    }
    off += snprintf(buf + off, RESP_SZ - off, "],\"rows\":[");
    for (int i = 0; i < nrows && off < RESP_SZ - 4096; i++) {
        if (i) buf[off++] = ',';
        buf[off++] = '[';
        for (int j = 0; j < ncols; j++) {
            if (j) buf[off++] = ',';
            if (PQgetisnull(r, i, j)) {
                off += snprintf(buf + off, RESP_SZ - off, "null");
            } else {
                char esc[1024];
                json_esc(PQgetvalue(r, i, j), esc, sizeof(esc));
                off += snprintf(buf + off, RESP_SZ - off, "\"%s\"", esc);
            }
        }
        buf[off++] = ']';
    }
    off += snprintf(buf + off, RESP_SZ - off, "]}\n");
    ssize_t wr = write(fd, buf, (size_t)off);
    if (wr < 0) fprintf(stderr, "[db-proxy] write error: %s\n", strerror(errno));
    free(buf);
}

static int str_contains_nocase(const char *haystack, const char *needle) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return 0;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j;
        for (j = 0; j < nlen; j++) {
            if (toupper((unsigned char)haystack[i + j]) !=
                toupper((unsigned char)needle[j]))
                break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

static int sql_blocked(const char *sql, char *reason, size_t rsz) {
    char upper[256];
    size_t i = 0;
    while (sql[i] && i < sizeof(upper) - 1) {
        upper[i] = (char)toupper((unsigned char)sql[i]);
        i++;
    }
    upper[i] = '\0';

    if (strncmp(upper, "DROP ", 5) == 0 ||
        strncmp(upper, "TRUNCATE ", 9) == 0 ||
        strncmp(upper, "ALTER ", 6) == 0) {
        snprintf(reason, rsz,
                 "BLOCKED: %.*s not permitted",
                 (int)i > 20 ? 20 : (int)i, upper);
        return 1;
    }

    if (str_contains_nocase(sql, "information_schema") ||
        str_contains_nocase(sql, "pg_catalog")) {
        snprintf(reason, rsz,
                 "BLOCKED: schema introspection forbidden");
        return 1;
    }

    return 0;
}

static void handle_client(int fd) {
    char *buf = malloc(REQ_SZ);
    if (!buf) { send_err(fd, "oom"); return; }

    int total = 0;
    int finished = 0;
    while (total < REQ_SZ - 1 && !finished) {
        ssize_t n = read(fd, buf + total, (size_t)(REQ_SZ - 1 - total));
        if (n <= 0) break;
        char *nl = memchr(buf + total, '\n', (size_t)n);
        if (nl) { *nl = '\0'; finished = 1; }
        total += (int)n;
    }
    buf[total] = '\0';

    if (!buf[0]) { free(buf); return; }

    cJSON *req = cJSON_Parse(buf);
    if (!req) {
        send_err(fd, "invalid JSON request");
        free(buf);
        return;
    }

    cJSON *jsql = cJSON_GetObjectItemCaseSensitive(req, "sql");
    if (!jsql || !cJSON_IsString(jsql)) {
        send_err(fd, "missing sql field");
        cJSON_Delete(req);
        free(buf);
        return;
    }

    const char *sql = jsql->valuestring;
    char reason[256];
    if (sql_blocked(sql, reason, sizeof(reason))) {
        send_err(fd, reason);
        cJSON_Delete(req);
        free(buf);
        return;
    }

    cJSON *jparams = cJSON_GetObjectItemCaseSensitive(req, "params");
    const char *pvals[PARAM_MAX];
    int nparams = 0;
    if (jparams && cJSON_IsArray(jparams)) {
        nparams = cJSON_GetArraySize(jparams);
        if (nparams > PARAM_MAX) nparams = PARAM_MAX;
        for (int i = 0; i < nparams; i++) {
            cJSON *item = cJSON_GetArrayItem(jparams, i);
            if (item && cJSON_IsNull(item))
                pvals[i] = NULL;
            else if (item && cJSON_IsString(item))
                pvals[i] = item->valuestring;
            else if (item && cJSON_IsNumber(item))
                pvals[i] = item->valuestring;
            else
                pvals[i] = NULL;
        }
    }

    db_ensure();
    if (!db_conn) {
        send_err(fd, "DB unavailable");
        cJSON_Delete(req);
        free(buf);
        return;
    }

    PGresult *r = nparams > 0
        ? PQexecParams(db_conn, sql, nparams, NULL, pvals, NULL, NULL, 0)
        : PQexec(db_conn, sql);

    ExecStatusType st = PQresultStatus(r);
    if (st == PGRES_TUPLES_OK) {
        send_ok_query(fd, r);
    } else if (st == PGRES_COMMAND_OK) {
        const char *tag = PQcmdTuples(r);
        send_ok_exec(fd, tag ? atoi(tag) : 0);
    } else {
        send_err(fd, PQerrorMessage(db_conn));
    }

    PQclear(r);
    cJSON_Delete(req);
    free(buf);
}

static void handle_sig(int sig) { (void)sig; running = 0; }

int main(void) {
    set_sock_default();
    signal(SIGTERM, handle_sig);
    signal(SIGINT,  handle_sig);
    signal(SIGPIPE, SIG_IGN);

    if (db_connect() < 0) {
        fprintf(stderr, "[db-proxy] FATAL: DB unavailable at startup\n");
        return 1;
    }

    char *slash = strrchr(sock_path, '/');
    if (slash) { *slash = '\0'; ensure_dir(sock_path); *slash = '/'; }

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t slen = strlen(sock_path);
    if (slen >= sizeof(addr.sun_path)) slen = sizeof(addr.sun_path) - 1;
    memcpy(addr.sun_path, sock_path, slen);
    unlink(sock_path);
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    chmod(sock_path, 0600);
    listen(sfd, 16);

    fprintf(stderr, "[db-proxy] listening on %s\n", sock_path);

    struct pollfd pfd = { .fd = sfd, .events = POLLIN };
    while (running) {
        int pr = poll(&pfd, 1, 5000);
        if (pr < 0) { if (errno == EINTR) continue; break; }
        if (pr == 0) { db_ensure(); continue; }
        int cfd = accept(sfd, NULL, NULL);
        if (cfd >= 0) {
            handle_client(cfd);
            close(cfd);
        }
    }

    if (db_conn) PQfinish(db_conn);
    close(sfd);
    unlink(sock_path);
    fprintf(stderr, "[db-proxy] shutdown\n");
    return 0;
}
