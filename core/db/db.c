#define _POSIX_C_SOURCE 200809L
#include "db.h"
#include "json_esc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>

#define PATH_SZ      1024
#define REQ_SZ      65536
#define RESP_SZ    2097152

static char db_proxy_sock[PATH_SZ];

struct vehir_db {
    char error[VEHIR_DB_ERROR_SZ];
};

struct vehir_db_result {
    int nrows;
    int ncols;
    char error[VEHIR_DB_ERROR_SZ];
    char (*cols)[VEHIR_DB_COL_SZ];
    char (*vals)[VEHIR_DB_MAX_COLS][VEHIR_DB_VAL_SZ];
    char (*nullmap)[VEHIR_DB_MAX_COLS];
};

static void set_error(vehir_db *db, const char *msg) {
    if (!db) return;
    snprintf(db->error, sizeof(db->error), "%s", msg);
}

static int resolve_sock_path(char *dst, size_t dstsz) {
    const char *env = getenv("VEHIR_DB_PROXY_SOCK");
    if (env && env[0]) {
        snprintf(dst, dstsz, "%s", env);
        return 0;
    }
    return -1;
}

static int build_request(char *buf, size_t bufsz,
                          const char *sql,
                          const char *const *params, int nparams) {
    int off = snprintf(buf, bufsz, "{\"sql\":\"");
    if (off <= 0 || (size_t)off >= bufsz) return -1;

    char esc[REQ_SZ];
    json_esc(sql, esc, sizeof(esc));
    off += snprintf(buf + off, bufsz - off, "%s\"", esc);
    if (off <= 0 || (size_t)off >= bufsz) return -1;

    off += snprintf(buf + off, bufsz - off, ",\"params\":[");
    if (off <= 0 || (size_t)off >= bufsz) return -1;

    for (int i = 0; i < nparams; i++) {
        if (i) {
            off += snprintf(buf + off, bufsz - off, ",");
            if (off <= 0 || (size_t)off >= bufsz) return -1;
        }
        if (!params[i]) {
            off += snprintf(buf + off, bufsz - off, "null");
        } else {
            json_esc(params[i], esc, sizeof(esc));
            off += snprintf(buf + off, bufsz - off, "\"%s\"", esc);
        }
        if (off <= 0 || (size_t)off >= bufsz) return -1;
    }

    off += snprintf(buf + off, bufsz - off, "]}\n");
    if (off <= 0 || (size_t)off >= bufsz) return -1;
    return off;
}

static int send_recv(const char *sock, const char *req, int reqlen,
                      char *resp, size_t respsz) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    size_t slen = strlen(sock);
    if (slen >= sizeof(addr.sun_path)) slen = sizeof(addr.sun_path) - 1;
    memcpy(addr.sun_path, sock, slen);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    ssize_t n = write(fd, req, (size_t)reqlen);
    if (n <= 0) { close(fd); return -1; }

    n = read(fd, resp, respsz - 1);
    close(fd);
    if (n <= 0) return -1;
    resp[n] = '\0';
    return 0;
}

vehir_db *vehir_db_open(void) {
    vehir_db *db = calloc(1, sizeof(*db));
    if (!db) return NULL;

    if (db_proxy_sock[0] == '\0') {
        if (resolve_sock_path(db_proxy_sock, sizeof(db_proxy_sock)) != 0) {
            set_error(db, "cannot resolve socket path");
            return db;
        }
    }
    return db;
}

void vehir_db_close(vehir_db *db) {
    free(db);
}

const char *vehir_db_error(const vehir_db *db) {
    if (!db) return "null db handle";
    return db->error;
}

static cJSON *do_roundtrip(vehir_db *db, const char *sql,
                             const char *const *params, int nparams) {
    char req[REQ_SZ];
    int rlen = build_request(req, sizeof(req), sql, params, nparams);
    if (rlen < 0) {
        set_error(db, "request too large");
        return NULL;
    }

    char resp[RESP_SZ];
    if (send_recv(db_proxy_sock, req, rlen, resp, sizeof(resp)) < 0) {
        set_error(db, "cannot connect to db-proxy (is it running?)");
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        set_error(db, "invalid JSON response from proxy");
        return NULL;
    }
    return root;
}

int vehir_db_exec(vehir_db *db, const char *sql,
                   const char *const *params, int nparams) {
    if (!db || !sql) {
        if (db) set_error(db, "invalid arguments");
        return -1;
    }

    cJSON *root = do_roundtrip(db, sql, params, nparams);
    if (!root) return -1;

    cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
    if (!ok || !cJSON_IsTrue(ok)) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
        if (err && cJSON_IsString(err))
            set_error(db, err->valuestring);
        else
            set_error(db, "unknown proxy error");
        cJSON_Delete(root);
        return -1;
    }

    cJSON *nrows = cJSON_GetObjectItemCaseSensitive(root, "nrows");
    int n = (nrows && cJSON_IsNumber(nrows)) ? nrows->valueint : 0;
    cJSON_Delete(root);
    return n;
}

vehir_db_result *vehir_db_query(vehir_db *db, const char *sql,
                                 const char *const *params, int nparams) {
    vehir_db_result *res = calloc(1, sizeof(*res));
    if (!res) return NULL;

    if (!db || !sql) {
        snprintf(res->error, sizeof(res->error), "invalid arguments");
        return res;
    }

    cJSON *root = do_roundtrip(db, sql, params, nparams);
    if (!root) {
        snprintf(res->error, sizeof(res->error), "%s", db->error);
        return res;
    }

    cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
    if (!ok || !cJSON_IsTrue(ok)) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
        if (err && cJSON_IsString(err))
            snprintf(res->error, sizeof(res->error), "%s", err->valuestring);
        else
            snprintf(res->error, sizeof(res->error), "unknown proxy error");
        cJSON_Delete(root);
        return res;
    }

    cJSON *jncols = cJSON_GetObjectItemCaseSensitive(root, "ncols");
    cJSON *jnrows = cJSON_GetObjectItemCaseSensitive(root, "nrows");
    int ncols = (jncols && cJSON_IsNumber(jncols)) ? jncols->valueint : 0;
    int nrows = (jnrows && cJSON_IsNumber(jnrows)) ? jnrows->valueint : 0;
    if (ncols > VEHIR_DB_MAX_COLS) ncols = VEHIR_DB_MAX_COLS;
    if (nrows > VEHIR_DB_MAX_ROWS) nrows = VEHIR_DB_MAX_ROWS;

    size_t cols_sz = VEHIR_DB_MAX_COLS * VEHIR_DB_COL_SZ;
    size_t vals_sz = VEHIR_DB_MAX_ROWS * VEHIR_DB_MAX_COLS * VEHIR_DB_VAL_SZ;
    size_t null_sz = VEHIR_DB_MAX_ROWS * VEHIR_DB_MAX_COLS;

    res->cols    = malloc(cols_sz);
    res->vals    = malloc(vals_sz);
    res->nullmap = malloc(null_sz);
    if (!res->cols || !res->vals || !res->nullmap) {
        free(res->cols); free(res->vals); free(res->nullmap);
        snprintf(res->error, sizeof(res->error), "oom");
        cJSON_Delete(root);
        return res;
    }
    memset(res->cols, 0, cols_sz);
    memset(res->vals, 0, vals_sz);
    memset(res->nullmap, 0, null_sz);

    cJSON *jcols = cJSON_GetObjectItemCaseSensitive(root, "cols");
    if (jcols && cJSON_IsArray(jcols)) {
        for (int c = 0; c < ncols && c < cJSON_GetArraySize(jcols); c++) {
            cJSON *item = cJSON_GetArrayItem(jcols, c);
            if (item && cJSON_IsString(item))
                snprintf(res->cols[c], VEHIR_DB_COL_SZ, "%s", item->valuestring);
        }
    }

    cJSON *jrows = cJSON_GetObjectItemCaseSensitive(root, "rows");
    if (jrows && cJSON_IsArray(jrows)) {
        int ri = 0;
        for (int i = 0; i < cJSON_GetArraySize(jrows) && ri < nrows; i++) {
            cJSON *row = cJSON_GetArrayItem(jrows, i);
            if (!row || !cJSON_IsArray(row)) continue;
            for (int j = 0; j < cJSON_GetArraySize(row) && j < ncols; j++) {
                cJSON *val = cJSON_GetArrayItem(row, j);
                if (!val || cJSON_IsNull(val)) {
                    res->nullmap[ri][j] = 1;
                } else if (cJSON_IsString(val)) {
                    res->nullmap[ri][j] = 0;
                    snprintf(res->vals[ri][j], VEHIR_DB_VAL_SZ,
                             "%s", val->valuestring);
                } else if (cJSON_IsNumber(val)) {
                    res->nullmap[ri][j] = 0;
                    snprintf(res->vals[ri][j], VEHIR_DB_VAL_SZ,
                             "%g", val->valuedouble);
                } else if (cJSON_IsBool(val)) {
                    res->nullmap[ri][j] = 0;
                    snprintf(res->vals[ri][j], VEHIR_DB_VAL_SZ,
                             "%s", cJSON_IsTrue(val) ? "true" : "false");
                }
            }
            ri++;
        }
    }

    res->nrows = nrows;
    res->ncols = ncols;
    cJSON_Delete(root);
    return res;
}

void vehir_db_result_free(vehir_db_result *r) {
    if (!r) return;
    free(r->cols);
    free(r->vals);
    free(r->nullmap);
    free(r);
}

int vehir_db_result_nrows(const vehir_db_result *r) {
    return r ? r->nrows : 0;
}

int vehir_db_result_ncols(const vehir_db_result *r) {
    return r ? r->ncols : 0;
}

const char *vehir_db_result_colname(const vehir_db_result *r, int col) {
    if (!r || col < 0 || col >= r->ncols) return "";
    return r->cols[col];
}

const char *vehir_db_result_value(const vehir_db_result *r, int row, int col) {
    if (!r || row < 0 || row >= r->nrows || col < 0 || col >= r->ncols)
        return "";
    if (r->nullmap[row][col]) return "";
    return r->vals[row][col];
}

int vehir_db_result_isnull(const vehir_db_result *r, int row, int col) {
    if (!r || row < 0 || row >= r->nrows || col < 0 || col >= r->ncols)
        return 1;
    return r->nullmap[row][col];
}

const char *vehir_db_result_error(const vehir_db_result *r) {
    if (!r) return "null result";
    return r->error;
}
