// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir
// graphd — event-driven HTML graph regenerator for IPM workspaces

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>
#include <libpq-fe.h>

static volatile sig_atomic_t running = 1;
static void handle_signal(int sig) { (void)sig; running = 0; }

static void regen(const char *ws, const char *out, const char *extra_args, bool full) {
    char cmd[1024];
    const char *mode = full ? "" : "--short";
    snprintf(cmd, sizeof(cmd),
        "IPM_WORKSPACE=%s ipm show --html %s %s --output %s",
        ws, mode, extra_args ? extra_args : "", out);
    int rc = system(cmd);
    if (rc != 0)
        fprintf(stderr, "graphd: regenerate %s failed (rc=%d)\n", out, rc);
}

int main(int argc, char **argv) {
    const char *ws = "ipm";
    char combined[4096] = {0}, ideas[4096] = {0}, progs[4096] = {0};
    bool full = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workspace") == 0 && i+1 < argc) ws = argv[++i];
        else if (strcmp(argv[i], "--combined") == 0 && i+1 < argc) snprintf(combined, sizeof(combined), "%s", argv[++i]);
        else if (strcmp(argv[i], "--ideas") == 0 && i+1 < argc) snprintf(ideas, sizeof(ideas), "%s", argv[++i]);
        else if (strcmp(argv[i], "--programs") == 0 && i+1 < argc) snprintf(progs, sizeof(progs), "%s", argv[++i]);
        else if (strcmp(argv[i], "--all") == 0 && i+1 < argc) {
            snprintf(combined, sizeof(combined), "%s/combined.html", argv[++i]);
            snprintf(ideas, sizeof(ideas), "%s/ideas.html", argv[i]);
            snprintf(progs, sizeof(progs), "%s/programs.html", argv[i]);
        }
        else if (strcmp(argv[i], "--full") == 0) full = true;
        else {
            fprintf(stderr, "usage: graphd --workspace W --all DIR [--full]\n"
                            "       graphd --workspace W --combined F --ideas F --programs F [--full]\n");
            return 1;
        }
    }
    if (!combined[0] && !ideas[0] && !progs[0]) {
        fprintf(stderr, "graphd: no outputs. Use --combined, --ideas, --programs, or --all DIR\n");
        return 1;
    }

    const char *db_url = getenv("IPM_DATABASE_URL");
    char auto_url[4096] = {0};
    if (!db_url) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(auto_url, sizeof(auto_url), "postgresql:///ipm?host=%s/.local/state/ipm/pgdata", home);
            db_url = auto_url;
        }
    }
    if (!db_url) { fprintf(stderr, "graphd: no DB URL\n"); return 1; }

    PGconn *conn = PQconnectdb(db_url);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "graphd: DB connect failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn); return 1;
    }

    PGresult *r = PQexec(conn, "LISTEN idea_changed");
    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        fprintf(stderr, "graphd: LISTEN failed\n");
        PQclear(r); PQfinish(conn); return 1;
    }
    PQclear(r);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Initial generation */
    if (combined[0]) regen(ws, combined, "", full);
    if (ideas[0]) regen(ws, ideas, "--ideas", full);
    if (progs[0]) regen(ws, progs, "--programs", full);
    fprintf(stderr, "graphd: listening on idea_changed, workspace=%s\n", ws);

    int sock = PQsocket(conn);
    struct timeval tv;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 30; tv.tv_usec = 0;

        int rc = select(sock + 1, &fds, NULL, NULL, &tv);
        if (rc < 0 && errno != EINTR) break;
        if (!running) break;

        PQconsumeInput(conn);
        PGnotify *notify;
        while ((notify = PQnotifies(conn)) != NULL) {
            fprintf(stderr, "graphd: change detected\n");
            if (combined[0]) regen(ws, combined, "", full);
            if (ideas[0]) regen(ws, ideas, "--ideas", full);
            if (progs[0]) regen(ws, progs, "--programs", full);
            PQfreemem(notify);
        }

        if (rc == 0) {
            PGresult *ping = PQexec(conn, "SELECT 1");
            if (PQresultStatus(ping) != PGRES_TUPLES_OK) {
                fprintf(stderr, "graphd: connection lost\n");
                PQclear(ping); break;
            }
            PQclear(ping);
        }
    }

    PQfinish(conn);
    fprintf(stderr, "graphd: stopped\n");
    return 0;
}
