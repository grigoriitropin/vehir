// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include "safe-exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define BUF_SZ 262144

char *safe_exec(const char *const argv[], int *out_len)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        fprintf(stderr, "safe-exec: pipe failed: %s\n", strerror(errno));
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "safe-exec: fork failed: %s\n", strerror(errno));
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
    while ((n = read(pipefd[0], buf + total, (size_t)(BUF_SZ - 1 - total))) > 0) {
        total += n;
        if ((size_t)total >= BUF_SZ - 1) break;
    }
    close(pipefd[0]);

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        free(buf);
        return NULL;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        buf[total] = '\0';
        if (total > 0)
            fprintf(stderr, "safe-exec: %s\n", buf);
        free(buf);
        return NULL;
    }

    buf[total] = '\0';
    if (out_len) *out_len = (int)total;
    return buf;
}
