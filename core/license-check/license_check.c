// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <cjson/cJSON.h>
#include <libpq-fe.h>

#define PATH_SZ 4096
#define LINE_SZ 1024
#define SHA256_HEX_SZ 65

static int sha256_file(const char *path, char hex[SHA256_HEX_SZ]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return -1; }
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        EVP_DigestUpdate(ctx, buf, n);
    fclose(f);
    unsigned char md[32];
    EVP_DigestFinal_ex(ctx, md, NULL);
    EVP_MD_CTX_free(ctx);
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", md[i]);
    hex[64] = '\0';
    return 0;
}

static int is_hidden(const char *name) {
    return name[0] == '.';
}

static int has_suffix(const char *name, const char *suf) {
    size_t nl = strlen(name), sl = strlen(suf);
    return nl > sl && strcmp(name + nl - sl, suf) == 0;
}

static void scan_sources(const char *dir, cJSON *array) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (is_hidden(e->d_name)) continue;
        if (strcmp(e->d_name, "result") == 0 || strcmp(e->d_name, "nix") == 0) continue;
        char path[PATH_SZ];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_sources(path, array);
        } else if (S_ISREG(st.st_mode) && (has_suffix(e->d_name, ".c") || has_suffix(e->d_name, ".h"))) {
            char *rel = strdup(path);
            if (rel) {
                cJSON_AddItemToArray(array, cJSON_CreateString(rel));
            }
        }
    }
    closedir(d);
}

static int check_spdx_header(const char *path, char *line1, size_t l1sz,
                              char *line2, size_t l2sz) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[LINE_SZ];
    int has_spdx = 0, has_copyright = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '\n' || buf[0] == '\r') continue;
        if (strncmp(buf, "// SPDX-License-Identifier:", 27) == 0) {
            has_spdx = 1;
            snprintf(line1, l1sz, "%s", buf);
            char *nl = strchr(line1, '\n');
            if (nl) *nl = '\0';
        }
        if (strncmp(buf, "// Copyright", 12) == 0) {
            has_copyright = 1;
            snprintf(line2, l2sz, "%s", buf);
            char *nl = strchr(line2, '\n');
            if (nl) *nl = '\0';
        }
        if (has_spdx && has_copyright) break;
    }
    fclose(f);
    return (has_spdx && has_copyright) ? 0 : -1;
}

static int third_party_contains(const char *tp_path, const char *keyword) {
    FILE *f = fopen(tp_path, "r");
    if (!f) return 0;
    char buf[LINE_SZ];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, keyword)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

int main(void) {
    const char *expected_license_sha256 = "cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30";

    cJSON *root = cJSON_CreateObject();
    cJSON *licenses = cJSON_AddObjectToObject(root, "licenses");
    cJSON *copyrights = cJSON_AddObjectToObject(root, "copyrights");
    cJSON *deps_obj = cJSON_AddObjectToObject(root, "deps");

    cJSON_AddStringToObject(root, "repo", ".");

    /* LICENSE check */
    char license_sha[SHA256_HEX_SZ];
    int license_ok = (sha256_file("LICENSE", license_sha) == 0 &&
                      strcmp(license_sha, expected_license_sha256) == 0);
    cJSON *lic = cJSON_AddObjectToObject(licenses, "LICENSE");
    cJSON_AddBoolToObject(lic, "ok", license_ok);
    cJSON_AddStringToObject(lic, "expected", expected_license_sha256);
    cJSON_AddStringToObject(lic, "actual", license_ok ? license_sha : "");

    /* NOTICE check */
    struct stat notice_st;
    int notice_ok = (stat("NOTICE", &notice_st) == 0 && S_ISREG(notice_st.st_mode));
    cJSON *ntc = cJSON_AddObjectToObject(licenses, "NOTICE");
    cJSON_AddBoolToObject(ntc, "ok", notice_ok);

    /* THIRD_PARTY_NOTICES check */
    struct stat tp_st;
    int tp_exists = (stat("THIRD_PARTY_NOTICES", &tp_st) == 0 && S_ISREG(tp_st.st_mode));
    cJSON *tp = cJSON_AddObjectToObject(licenses, "THIRD_PARTY_NOTICES");
    cJSON_AddBoolToObject(tp, "exists", tp_exists);
    if (tp_exists) {
        const char *expected_deps[] = {"cJSON", "libpq", "libcurl", "OpenSSL", "glibc", NULL};
        cJSON *found = cJSON_AddArrayToObject(tp, "found");
        cJSON *missing = cJSON_AddArrayToObject(tp, "missing");
        for (int i = 0; expected_deps[i]; i++) {
            if (third_party_contains("THIRD_PARTY_NOTICES", expected_deps[i]))
                cJSON_AddItemToArray(found, cJSON_CreateString(expected_deps[i]));
            else
                cJSON_AddItemToArray(missing, cJSON_CreateString(expected_deps[i]));
        }
    }

    /* SPDX header check */
    cJSON *src_files = cJSON_AddArrayToObject(root, "source_files");
    scan_sources(".", src_files);

    int total_src = cJSON_GetArraySize(src_files);
    int ok_src = 0;
    cJSON *bad_files = cJSON_AddArrayToObject(copyrights, "errors");

    for (int i = 0; i < total_src; i++) {
        const char *path = cJSON_GetArrayItem(src_files, i)->valuestring;
        char line1[LINE_SZ], line2[LINE_SZ];
        int r = check_spdx_header(path, line1, sizeof(line1), line2, sizeof(line2));
        cJSON *fe = cJSON_CreateObject();
        cJSON_AddStringToObject(fe, "file", path);
        if (r == 0) {
            cJSON_AddBoolToObject(fe, "ok", 1);
            ok_src++;
        } else {
            cJSON_AddBoolToObject(fe, "ok", 0);
            char reason[256] = "";
            if (!line1[0]) snprintf(reason, sizeof(reason), "missing SPDX header");
            else if (!line2[0]) snprintf(reason, sizeof(reason), "missing copyright");
            else snprintf(reason, sizeof(reason), "header incomplete");
            cJSON_AddStringToObject(fe, "error", reason);
        }
        cJSON_AddItemToArray(bad_files, fe);
    }
    cJSON_AddNumberToObject(copyrights, "total", total_src);
    cJSON_AddNumberToObject(copyrights, "ok", ok_src);

    /* IPM dependency check */
    const char *ipm_url = getenv("IPM_DATABASE_URL");
    if (ipm_url && ipm_url[0]) {
        PGconn *ipm = PQconnectdb(ipm_url);
        if (PQstatus(ipm) == CONNECTION_OK) {
            cJSON *declared = cJSON_AddArrayToObject(deps_obj, "declared");
            PGresult *r = PQexecParams(ipm,
                "SELECT p1.name AS consumer, p2.name AS dep, l.name "
                "FROM idea_edges e "
                "JOIN idea_packages p1 ON p1.id = e.parent_id "
                "JOIN idea_packages p2 ON p2.id = e.child_id "
                "JOIN licenses l ON l.id = e.edge_license_id "
                "WHERE e.link_type = 'library' "
                "ORDER BY p1.name",
                0, NULL, NULL, NULL, NULL, 0);
            if (PQresultStatus(r) == PGRES_TUPLES_OK) {
                for (int i = 0; i < PQntuples(r); i++) {
                    cJSON *dep = cJSON_CreateObject();
                    cJSON_AddStringToObject(dep, "consumer", PQgetvalue(r, i, 0));
                    cJSON_AddStringToObject(dep, "dep", PQgetvalue(r, i, 1));
                    cJSON_AddStringToObject(dep, "license", PQgetvalue(r, i, 2));
                    cJSON_AddItemToArray(declared, dep);
                }
            }
            PQclear(r);
            PQfinish(ipm);
        } else {
            cJSON_AddStringToObject(deps_obj, "error", PQerrorMessage(ipm));
            PQfinish(ipm);
        }
    } else {
        cJSON_AddStringToObject(deps_obj, "note", "IPM_DATABASE_URL not set — IPM check skipped");
    }

    /* Overall verdict */
    int all_ok = license_ok && (total_src == ok_src);
    cJSON_AddBoolToObject(root, "ok", all_ok);

    char *out = cJSON_Print(root);
    if (out) { printf("%s\n", out); free(out); }
    cJSON_Delete(root);
    return all_ok ? 0 : 1;
}
