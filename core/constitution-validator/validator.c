#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <openssl/evp.h>
#include <cjson/cJSON.h>
#include <db.h>
#include <pain.h>

#define PATH_SZ 1024

static int sha256_hex(const char *data, size_t len, char hex[65]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, (const unsigned char *)data, len);
    unsigned char md[32];
    EVP_DigestFinal_ex(ctx, md, NULL);
    EVP_MD_CTX_free(ctx);
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", md[i]);
    hex[64] = '\0';
    return 0;
}

static char *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    *out_len = fread(buf, 1, (size_t)sz, f);
    buf[*out_len] = '\0';
    fclose(f);
    return buf;
}

static void process_sections(const char *buf, vehir_db *db, int verify,
                              cJSON *results) {
    const char *p = buf;
    while (*p) {
        const char *tag_start = strchr(p, '<');
        if (!tag_start) break;
        const char *tag_end = strchr(tag_start, '>');
        if (!tag_end) { p = tag_start + 1; continue; }
        const char *tns = tag_start + 1;
        if (*tns == '/') { p = tag_end + 1; continue; }
        const char *tne = tns;
        while (tne < tag_end && (isalnum((unsigned char)*tne) || *tne == '_'))
            tne++;
        size_t tlen = (size_t)(tne - tns);
        if (tlen == 0 || tlen > 63) { p = tag_end + 1; continue; }
        char tag[64];
        memcpy(tag, tns, tlen);
        tag[tlen] = '\0';
        char close_tag[128];
        snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
        const char *content_start = tag_end + 1;
        const char *close = strstr(content_start, close_tag);
        if (!close) { p = tag_end + 1; continue; }
        size_t clen = (size_t)(close - content_start);
        char hex[65];
        sha256_hex(content_start, clen, hex);
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "section", tag);
        cJSON_AddStringToObject(entry, "hash", hex);
        if (verify) {
            const char *params[1] = { tag };
            vehir_db_result *r = vehir_db_query(db,
                "SELECT sha256 FROM soul_hashes WHERE section = $1",
                params, 1);
            if (r && vehir_db_result_nrows(r) > 0) {
                const char *stored = vehir_db_result_value(r, 0, 0);
                int ok = (strcmp(stored, hex) == 0);
                cJSON_AddBoolToObject(entry, "ok", ok);
                if (!ok) {
                    cJSON_AddStringToObject(entry, "stored", stored);
                    vehir_pain_signal(db, "soul",
                        tag, 0.7, "constitution-validator",
                        "SOUL.md section hash drift detected");
                }
            } else {
                cJSON_AddBoolToObject(entry, "ok", 0);
                cJSON_AddStringToObject(entry, "error", "no baseline");
            }
            vehir_db_result_free(r);
        } else {
            const char *params[3] = { tag, hex, "[]" };
            vehir_db_exec(db,
                "INSERT INTO soul_hashes (section, sha256, links) "
                "VALUES ($1, $2, $3::jsonb) "
                "ON CONFLICT (section) DO UPDATE SET "
                "  sha256 = EXCLUDED.sha256, "
                "  updated_at = now()",
                params, 3);
            cJSON_AddBoolToObject(entry, "stored", 1);
        }
        cJSON_AddItemToArray(results, entry);
        process_sections(content_start, db, verify, results);
        p = close + strlen(close_tag);
    }
}

int main(int argc, char *argv[]) {
    int verify = 0;
    if (argc > 1 && strcmp(argv[1], "--verify") == 0) verify = 1;
    else if (argc > 1 && strcmp(argv[1], "--baseline") == 0) verify = 0;
    else if (argc > 1) {
        fprintf(stderr, "Usage: %s [--verify | --baseline]\n", argv[0]);
        return 2;
    }
    const char *soul_path = "SOUL.md";
    size_t len;
    char *content = load_file(soul_path, &len);
    if (!content) {
        fprintf(stderr, "constitution-validator: cannot read %s\n", soul_path);
        return 2;
    }
    vehir_db *db = vehir_db_open();
    if (!db) {
        fprintf(stderr, "constitution-validator: db-proxy unreachable\n");
        free(content);
        return 1;
    }
    char file_hex[65];
    sha256_hex(content, len, file_hex);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "file", soul_path);
    cJSON_AddStringToObject(root, "file_hash", file_hex);
    cJSON *sections = cJSON_AddArrayToObject(root, "sections");
    process_sections(content, db, verify, sections);
    if (verify) {
        const char *params[1] = { "soul_md" };
        vehir_db_result *r = vehir_db_query(db,
            "SELECT sha256 FROM soul_hashes WHERE section = $1",
            params, 1);
        int ok = 0;
        if (r && vehir_db_result_nrows(r) > 0)
            ok = (strcmp(vehir_db_result_value(r, 0, 0), file_hex) == 0);
        cJSON_AddBoolToObject(root, "ok", ok);
        vehir_db_result_free(r);
    } else {
        const char *params[3] = { "soul_md", file_hex, "[]" };
        vehir_db_exec(db,
            "INSERT INTO soul_hashes (section, sha256, links) "
            "VALUES ($1, $2, $3::jsonb) "
            "ON CONFLICT (section) DO UPDATE SET "
            "  sha256 = EXCLUDED.sha256, "
            "  updated_at = now()",
            params, 3);
        cJSON_AddBoolToObject(root, "stored", 1);
    }
    char *out = cJSON_Print(root);
    if (out) { printf("%s\n", out); free(out); }
    vehir_db_close(db);
    free(content);
    return verify && !cJSON_GetObjectItemCaseSensitive(root, "ok")->valueint ? 1 : 0;
}
