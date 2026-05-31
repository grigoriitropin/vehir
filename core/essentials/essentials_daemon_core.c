// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cjson/cJSON.h>
#include <libpq-fe.h>
#include <openssl/evp.h>
#include <db.h>
#include <pain.h>
#include "vehir_lib.h"

#define PG_PORT 5432
#define TOOLS_MARKER "<!-- TOOLS -->"
#define TOOLS_CLOSE  "<!-- /TOOLS -->"
#define BUF_SZ  131072
#define LINE_SZ 512

static volatile sig_atomic_t g_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static char *sha256_hex(const char *data, size_t len) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, (const unsigned char *)data, len);
    unsigned char md[32];
    EVP_DigestFinal_ex(ctx, md, NULL);
    EVP_MD_CTX_free(ctx);
    char *hex = malloc(65);
    if (!hex) return NULL;
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", md[i]);
    hex[64] = '\0';
    return hex;
}

static char *safe_exec_capture(const char *const argv[], int *out_len) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(pipefd[1]);

    char *buf = malloc(BUF_SZ);
    if (!buf) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    ssize_t total = 0;
    ssize_t n;
    while ((n = read(pipefd[0], buf + total,
                     (size_t)(BUF_SZ - 1 - total))) > 0) {
        total += n;
        if ((size_t)total >= BUF_SZ - 1) break;
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(buf);
        return NULL;
    }

    buf[total] = '\0';
    if (out_len) *out_len = (int)total;
    return buf;
}

static char *pg_socket_dir(void) {
    const char *xdg = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");
    const char *base = xdg ? xdg : home;
    if (!base) return NULL;
    char *path = malloc(strlen(base) + 32);
    if (!path) return NULL;
    if (xdg)
        sprintf(path, "%s/ipm/pgdata", base);
    else
        sprintf(path, "%s/.local/state/ipm/pgdata", base);
    return path;
}

static PGconn *pg_connect_listen(const char *sockdir) {
    char connstr[512];
    snprintf(connstr, sizeof(connstr),
             "host=%s port=%d dbname=ipm", sockdir, PG_PORT);
    PGconn *conn = PQconnectdb(connstr);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "essentials-daemon: PG connect failed: %s\n",
                PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    PGresult *res = PQexec(conn, "LISTEN idea_changed");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "essentials-daemon: LISTEN failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return NULL;
    }
    PQclear(res);
    return conn;
}

static char *build_tool_block(cJSON *nodes, size_t *out_len) {
    if (!nodes || !cJSON_IsArray(nodes)) return NULL;

    size_t cap = 4096;
    size_t len = 0;
    char *block = malloc(cap);
    if (!block) return NULL;
    block[0] = '\0';

    cJSON *node = NULL;
    cJSON_ArrayForEach(node, nodes) {
        cJSON *name_j = cJSON_GetObjectItemCaseSensitive(node, "name");
        cJSON *desc_j = cJSON_GetObjectItemCaseSensitive(node, "description");
        cJSON *art_j  = cJSON_GetObjectItemCaseSensitive(node, "artifact_name");
        cJSON *stat_j = cJSON_GetObjectItemCaseSensitive(node, "proof_status");
        cJSON *axiom_j = cJSON_GetObjectItemCaseSensitive(node, "is_axiom");
        cJSON *type_j = cJSON_GetObjectItemCaseSensitive(node, "artifact_type");

        if (!name_j || !stat_j) continue;
        const char *s = stat_j->valuestring;
        if (!s || (strcmp(s, "works") != 0 && strcmp(s, "attested") != 0))
            continue;
        if (axiom_j && cJSON_IsTrue(axiom_j)) continue;

        const char *name_str = name_j->valuestring;
        const char *desc_str = desc_j && desc_j->valuestring
                               ? desc_j->valuestring : "";
        const char *art_str  = art_j && art_j->valuestring
                               ? art_j->valuestring : name_str;
        const char *type_str = type_j && type_j->valuestring
                               ? type_j->valuestring : "";

        char line[LINE_SZ];
        snprintf(line, sizeof(line), "- `%s` (%s) [%s] — %s\n",
                 name_str, art_str, type_str, desc_str);

        size_t llen = strlen(line);
        if (len + llen + 1 > cap) {
            cap *= 2;
            char *nb = realloc(block, cap);
            if (!nb) { free(block); return NULL; }
            block = nb;
        }
        memcpy(block + len, line, llen + 1);
        len += llen;
    }
    if (out_len) *out_len = len;
    return block;
}

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

static int write_file_atomic(const char *path, const char *data, size_t len) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        unlink(tmp);
        return -1;
    }
    fclose(f);
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

static int soul_replace_marker(const char *tools_block) {
    size_t len;
    char *content = read_file("SOUL.md", &len);
    if (!content) {
        fprintf(stderr, "essentials-daemon: cannot read SOUL.md\n");
        return -1;
    }

    char *marker = strstr(content, TOOLS_MARKER);
    char *close  = marker ? strstr(marker, TOOLS_CLOSE) : NULL;
    if (!marker || !close) {
        fprintf(stderr, "essentials-daemon: TOOLS marker not found in SOUL.md\n");
        free(content);
        return -1;
    }

    size_t prefix_len = (size_t)(marker - content);
    const char *suffix = close + strlen(TOOLS_CLOSE);
    size_t suffix_len = strlen(suffix);
    size_t tools_len  = strlen(tools_block);
    size_t open_len    = strlen(TOOLS_MARKER);
    size_t close_len   = strlen(TOOLS_CLOSE);

    size_t total = prefix_len + open_len + 1 + tools_len + 1 + close_len + suffix_len + 1;
    char *new_c = malloc(total);
    if (!new_c) { free(content); return -1; }

    char *dst = new_c;
    memcpy(dst, content, prefix_len); dst += prefix_len;
    memcpy(dst, TOOLS_MARKER, open_len); dst += open_len;
    *dst++ = '\n';
    memcpy(dst, tools_block, tools_len); dst += tools_len;
    memcpy(dst, TOOLS_CLOSE, close_len); dst += close_len;
    *dst++ = '\n';
    memcpy(dst, suffix, suffix_len + 1);

    size_t written = (size_t)(dst - new_c) + suffix_len;
    int rc = write_file_atomic("SOUL.md", new_c, written);
    free(new_c);
    free(content);
    return rc;
}

static int store_hash(vehir_db *db, const char *section, const char *hash) {
    const char *params[2] = { section, hash };
    return vehir_db_exec(db,
        "INSERT INTO soul_hashes (section, sha256, links) "
        "VALUES ($1, $2, '[]'::jsonb) "
        "ON CONFLICT (section) DO UPDATE SET "
        "  sha256 = EXCLUDED.sha256, "
        "  updated_at = now()",
        params, 2);
}

static int check_drift(vehir_db *db, const char *section, const char *hash) {
    const char *params[1] = { section };
    vehir_db_result *r = vehir_db_query(db,
        "SELECT sha256 FROM soul_hashes WHERE section = $1",
        params, 1);
    int drift = 1;
    if (r && vehir_db_result_nrows(r) > 0) {
        const char *stored = vehir_db_result_value(r, 0, 0);
        if (stored && strcmp(stored, hash) == 0)
            drift = 0;
    }
    vehir_db_result_free(r);
    return drift;
}

static void rebuild_and_apply(vehir_db *db) {
    const char *argv[] = { "idea", "show", NULL };
    int json_len;
    char *json = safe_exec_capture(argv, &json_len);
    if (!json) {
        fprintf(stderr, "essentials-daemon: idea show failed\n");
        return;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) {
        fprintf(stderr, "essentials-daemon: JSON parse failed\n");
        return;
    }

    cJSON *nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
    if (!nodes) {
        cJSON_Delete(root);
        return;
    }

    size_t block_len;
    char *block = build_tool_block(nodes, &block_len);
    cJSON_Delete(root);

    if (!block || block_len == 0) {
        free(block);
        return;
    }

    char *hash = sha256_hex(block, block_len);
    if (!hash) {
        free(block);
        return;
    }

    if (check_drift(db, "tools_context", hash)) {
        store_hash(db, "tools_context", hash);
        vehir_pain_signal(db, "ipm", "tools_context", 0.5,
                          "essentials-daemon",
                          "tool context updated — IPM graph changed");

        if (soul_replace_marker(block) != 0)
            fprintf(stderr, "essentials-daemon: SOUL.md update failed\n");
    }

    free(hash);
    free(block);
}

int essentials_daemon_run(vehir_db *db_scaffold, int argc, char **argv) {
    (void)db_scaffold;
    (void)argc;
    (void)argv;

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    char *sockdir = pg_socket_dir();
    if (!sockdir)
        vl_die("essentials-daemon", "cannot resolve IPM PG socket directory");

    PGconn *pg = pg_connect_listen(sockdir);
    free(sockdir);
    if (!pg)
        vl_die("essentials-daemon",
               "cannot connect to IPM PostgreSQL for LISTEN");

    vehir_db *db = vehir_db_open();
    if (!db) {
        PQfinish(pg);
        vl_die("essentials-daemon", "db-proxy unreachable");
    }

    rebuild_and_apply(db);

    int sock = PQsocket(pg);
    if (sock < 0) {
        PQfinish(pg);
        vehir_db_close(db);
        vl_die("essentials-daemon", "cannot get PG socket fd");
    }

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        int ret = select(sock + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        PQconsumeInput(pg);
        if (!FD_ISSET(sock, &fds)) continue;

        PGnotify *notify = NULL;
        while ((notify = PQnotifies(pg)) != NULL) {
            PQfreemem(notify);
            rebuild_and_apply(db);
        }
    }

    PQfinish(pg);
    vehir_db_close(db);
    return 0;
}
