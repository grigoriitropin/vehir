#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buf_t;

typedef struct {
    char *key;
    char *val;
} kv_t;

typedef struct {
    kv_t *entries;
    int count;
} cfg_t;

static _Noreturn void die_json(const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "forge: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", error);
    char *s = cJSON_PrintUnformatted(root);
    if (s) {
        printf("%s\n", s);
        free(s);
    } else {
        printf("{\"ok\":false,\"error\":\"%s\"}\n", error);
    }
    cJSON_Delete(root);
    fprintf(stderr, "forge: %s\n", error);
    exit(1);
}

static char *default_config_path(void) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) die_json("HOME is not set");
    char *path = malloc(strlen(home) + 32);
    if (!path) die_json("malloc failed");
    sprintf(path, "%s/.config/vehir/env", home);
    return path;
}

static cfg_t load_config(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "config not found: %s: %s", path, strerror(errno));
        die_json(msg);
    }
    if (st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "config %s is readable by group/other (mode %04o) — run: chmod 600 %s",
            path, st.st_mode & 0777, path);
        die_json(msg);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "cannot open config: %s: %s", path, strerror(errno));
        die_json(msg);
    }

    cfg_t cfg = { .entries = NULL, .count = 0 };
    int cap = 8;
    cfg.entries = malloc((size_t)cap * sizeof(kv_t));
    if (!cfg.entries) { fclose(f); die_json("malloc failed"); }

    char line[2048];
    while (fgets(line, (int)sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = p;
        char *val = eq + 1;

        size_t klen = strlen(key);
        while (klen > 0 && (key[klen-1] == ' ' || key[klen-1] == '\t')) key[--klen] = '\0';

        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r' ||
               val[vlen-1] == ' ' || val[vlen-1] == '\t')) val[--vlen] = '\0';

        if (cfg.count >= cap) {
            cap *= 2;
            kv_t *tmp = realloc(cfg.entries, (size_t)cap * sizeof(kv_t));
            if (!tmp) { fclose(f); die_json("realloc failed"); }
            cfg.entries = tmp;
        }
        cfg.entries[cfg.count].key = strdup(key);
        cfg.entries[cfg.count].val = strdup(val);
        if (!cfg.entries[cfg.count].key || !cfg.entries[cfg.count].val) {
            fclose(f);
            die_json("strdup failed");
        }
        cfg.count++;
    }
    if (ferror(f)) { fclose(f); die_json("read error on config file"); }
    fclose(f);
    return cfg;
}

static const char *cfg_require(const cfg_t *cfg, const char *key) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) return cfg->entries[i].val;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "key %s not found in config", key);
    die_json(msg);
}

static void cfg_free(cfg_t *cfg) {
    for (int i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].key);
        free(cfg->entries[i].val);
    }
    free(cfg->entries);
    cfg->entries = NULL;
    cfg->count = 0;
}

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
    if (!b.data) die_json("malloc failed");
    b.data[0] = '\0';
    return b;
}

static cJSON *api_request(const char *method, const char *path,
                           const char *json_body,
                           const char *base_url, const char *token) {
    char url[2048];
    snprintf(url, sizeof(url), "%s/api/v1%s", base_url, path);

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: token %s", token);

    CURL *curl = curl_easy_init();
    if (!curl) die_json("curl_easy_init failed");

    buf_t resp = buf_new();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "vehir-forge/0.1.0");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Accept: application/json");

    if (json_body) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (strcmp(method, "POST") == 0 && !json_body) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "HTTP request failed: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(resp.data);
        die_json(msg);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    cJSON *parsed = cJSON_Parse(resp.data);

    if (http_code < 200 || http_code >= 300) {
        char msg[1024];
        const char *detail = "";
        if (parsed) {
            cJSON *m = cJSON_GetObjectItemCaseSensitive(parsed, "message");
            if (cJSON_IsString(m)) detail = m->valuestring;
        }
        snprintf(msg, sizeof(msg), "API error %ld: %s (path: %s)", http_code, detail, path);
        if (parsed) cJSON_Delete(parsed);
        free(resp.data);
        die_json(msg);
    }

    free(resp.data);

    if (!parsed) {
        if (http_code == 204) {
            parsed = cJSON_CreateObject();
            if (!parsed) die_json("cJSON alloc failed");
        } else {
            die_json("failed to parse API response JSON");
        }
    }

    return parsed;
}

static char *get_authenticated_user(const char *base_url, const char *token) {
    cJSON *user = api_request("GET", "/user", NULL, base_url, token);
    cJSON *login = cJSON_GetObjectItemCaseSensitive(user, "login");
    if (!cJSON_IsString(login) || !login->valuestring[0]) {
        cJSON_Delete(user);
        die_json("could not determine authenticated user login");
    }
    char *result = strdup(login->valuestring);
    if (!result) die_json("strdup failed");
    cJSON_Delete(user);
    return result;
}

static void cmd_repo_create(const char *base_url, const char *token,
                             const char *name, int private, const char *desc) {
    cJSON *body = cJSON_CreateObject();
    if (!body) die_json("cJSON alloc failed");
    cJSON_AddStringToObject(body, "name", name);
    cJSON_AddBoolToObject(body, "private", private);
    if (desc) cJSON_AddStringToObject(body, "description", desc);

    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!body_str) die_json("cJSON print failed");

    cJSON *resp = api_request("POST", "/user/repos", body_str, base_url, token);
    free(body_str);

    cJSON *out = cJSON_CreateObject();
    if (!out) { cJSON_Delete(resp); die_json("cJSON alloc failed"); }

    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "repo_create");

    cJSON *full_name = cJSON_GetObjectItemCaseSensitive(resp, "full_name");
    if (cJSON_IsString(full_name))
        cJSON_AddStringToObject(out, "full_name", full_name->valuestring);

    cJSON *html_url = cJSON_GetObjectItemCaseSensitive(resp, "html_url");
    if (cJSON_IsString(html_url))
        cJSON_AddStringToObject(out, "html_url", html_url->valuestring);

    cJSON *clone_url = cJSON_GetObjectItemCaseSensitive(resp, "clone_url");
    if (cJSON_IsString(clone_url))
        cJSON_AddStringToObject(out, "clone_url", clone_url->valuestring);

    cJSON_AddBoolToObject(out, "private", private);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    cJSON_Delete(resp);
    if (!s) die_json("cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static void cmd_repo_list(const char *base_url, const char *token) {
    int page = 1;
    int limit = 50;
    cJSON *all = cJSON_CreateArray();
    if (!all) die_json("cJSON alloc failed");

    for (;;) {
        char path[256];
        snprintf(path, sizeof(path), "/user/repos?page=%d&limit=%d", page, limit);
        cJSON *resp = api_request("GET", path, NULL, base_url, token);

        if (!cJSON_IsArray(resp)) {
            cJSON_Delete(resp);
            break;
        }

        int count = cJSON_GetArraySize(resp);
        if (count == 0) {
            cJSON_Delete(resp);
            break;
        }

        for (int i = 0; i < count; i++) {
            cJSON *repo = cJSON_GetArrayItem(resp, i);
            cJSON *entry = cJSON_CreateObject();
            if (!entry) { cJSON_Delete(resp); cJSON_Delete(all); die_json("cJSON alloc failed"); }

            cJSON *fn = cJSON_GetObjectItemCaseSensitive(repo, "full_name");
            if (cJSON_IsString(fn)) cJSON_AddStringToObject(entry, "full_name", fn->valuestring);

            cJSON *hu = cJSON_GetObjectItemCaseSensitive(repo, "html_url");
            if (cJSON_IsString(hu)) cJSON_AddStringToObject(entry, "html_url", hu->valuestring);

            cJSON *priv = cJSON_GetObjectItemCaseSensitive(repo, "private");
            if (cJSON_IsBool(priv)) cJSON_AddBoolToObject(entry, "private", cJSON_IsTrue(priv));

            cJSON *desc = cJSON_GetObjectItemCaseSensitive(repo, "description");
            if (cJSON_IsString(desc)) cJSON_AddStringToObject(entry, "description", desc->valuestring);

            cJSON_AddItemToArray(all, entry);
        }
        cJSON_Delete(resp);

        if (count < limit) break;
        page++;
    }

    cJSON *out = cJSON_CreateObject();
    if (!out) { cJSON_Delete(all); die_json("cJSON alloc failed"); }
    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "repo_list");
    cJSON_AddNumberToObject(out, "count", cJSON_GetArraySize(all));
    cJSON_AddItemToObject(out, "repos", all);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!s) die_json("cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static void cmd_release_create(const char *base_url, const char *token,
                                const char *repo, const char *tag,
                                const char *notes) {
    char *owner = get_authenticated_user(base_url, token);

    char path[512];
    snprintf(path, sizeof(path), "/repos/%s/%s/releases", owner, repo);
    free(owner);

    cJSON *body = cJSON_CreateObject();
    if (!body) die_json("cJSON alloc failed");
    cJSON_AddStringToObject(body, "tag_name", tag);
    if (notes) cJSON_AddStringToObject(body, "body", notes);

    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!body_str) die_json("cJSON print failed");

    cJSON *resp = api_request("POST", path, body_str, base_url, token);
    free(body_str);

    cJSON *out = cJSON_CreateObject();
    if (!out) { cJSON_Delete(resp); die_json("cJSON alloc failed"); }
    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "release_create");
    cJSON_AddStringToObject(out, "tag", tag);
    cJSON_AddStringToObject(out, "repo", repo);

    cJSON *rid = cJSON_GetObjectItemCaseSensitive(resp, "id");
    if (cJSON_IsNumber(rid)) cJSON_AddNumberToObject(out, "id", rid->valuedouble);

    cJSON *hu = cJSON_GetObjectItemCaseSensitive(resp, "html_url");
    if (cJSON_IsString(hu)) cJSON_AddStringToObject(out, "html_url", hu->valuestring);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    cJSON_Delete(resp);
    if (!s) die_json("cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static void cmd_issue_create(const char *base_url, const char *token,
                              const char *repo, const char *title,
                              const char *body_text) {
    char *owner = get_authenticated_user(base_url, token);

    char path[512];
    snprintf(path, sizeof(path), "/repos/%s/%s/issues", owner, repo);
    free(owner);

    cJSON *body = cJSON_CreateObject();
    if (!body) die_json("cJSON alloc failed");
    cJSON_AddStringToObject(body, "title", title);
    if (body_text) cJSON_AddStringToObject(body, "body", body_text);

    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!body_str) die_json("cJSON print failed");

    cJSON *resp = api_request("POST", path, body_str, base_url, token);
    free(body_str);

    cJSON *out = cJSON_CreateObject();
    if (!out) { cJSON_Delete(resp); die_json("cJSON alloc failed"); }
    cJSON_AddBoolToObject(out, "ok", 1);
    cJSON_AddStringToObject(out, "action", "issue_create");
    cJSON_AddStringToObject(out, "repo", repo);

    cJSON *num = cJSON_GetObjectItemCaseSensitive(resp, "number");
    if (cJSON_IsNumber(num)) cJSON_AddNumberToObject(out, "number", num->valuedouble);

    cJSON *hu = cJSON_GetObjectItemCaseSensitive(resp, "html_url");
    if (cJSON_IsString(hu)) cJSON_AddStringToObject(out, "html_url", hu->valuestring);

    cJSON *ti = cJSON_GetObjectItemCaseSensitive(resp, "title");
    if (cJSON_IsString(ti)) cJSON_AddStringToObject(out, "title", ti->valuestring);

    char *s = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    cJSON_Delete(resp);
    if (!s) die_json("cJSON print failed");
    printf("%s\n", s);
    free(s);
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--config <path>] <command> [options]\n"
        "\n"
        "Forgejo REST API client.\n"
        "\n"
        "Commands:\n"
        "  repo create <name> [--private] [--desc \"...\"]    Create a repository\n"
        "  repo list                                         List your repositories\n"
        "  release create <repo> <tag> [--notes \"...\"]       Create a release\n"
        "  issue create <repo> <title> [--body \"...\"]        Create an issue\n"
        "\n"
        "Output: JSON {\"ok\":true/false, ...}\n"
        "Exit:   0 = success, 1 = error, 2 = usage error\n"
        "\n"
        "Config: ~/.config/vehir/env (override with --config <path>)\n"
        "  FORGE_BASE_URL=<url>     Forge API base (e.g. https://codeberg.org)\n"
        "  FORGE_TOKEN=<token>      API token (scopes: repository, issue, release)\n"
        "  File must be chmod 600 (owner-only). Tokens never touch process env.\n"
        "\n"
        "Note: git push is standard git (SSH key). This tool is the API surface.\n",
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
        if (!cfg_path) die_json("strdup failed");
        arg_off = 3;
    }

    if (arg_off >= argc) usage(argv[0]);

    if (!cfg_path) cfg_path = default_config_path();

    cfg_t cfg = load_config(cfg_path);
    free(cfg_path);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    const char *base_url = cfg_require(&cfg, "FORGE_BASE_URL");
    const char *token = cfg_require(&cfg, "FORGE_TOKEN");

    if (strcmp(argv[arg_off], "repo") == 0) {
        if (arg_off + 1 >= argc) usage(argv[0]);

        if (strcmp(argv[arg_off + 1], "create") == 0) {
            if (arg_off + 2 >= argc) usage(argv[0]);
            const char *name = argv[arg_off + 2];
            int priv = has_flag(argc, argv, arg_off + 3, "--private");
            const char *desc = find_opt(argc, argv, arg_off + 3, "--desc");
            cmd_repo_create(base_url, token, name, priv, desc);
        } else if (strcmp(argv[arg_off + 1], "list") == 0) {
            cmd_repo_list(base_url, token);
        } else {
            fprintf(stderr, "forge: unknown repo command '%s'\n", argv[arg_off + 1]);
            cfg_free(&cfg);
            curl_global_cleanup();
            return 2;
        }
    } else if (strcmp(argv[arg_off], "release") == 0) {
        if (arg_off + 1 >= argc) usage(argv[0]);

        if (strcmp(argv[arg_off + 1], "create") == 0) {
            if (arg_off + 3 >= argc) usage(argv[0]);
            const char *repo = argv[arg_off + 2];
            const char *tag = argv[arg_off + 3];
            const char *notes = find_opt(argc, argv, arg_off + 4, "--notes");
            cmd_release_create(base_url, token, repo, tag, notes);
        } else {
            fprintf(stderr, "forge: unknown release command '%s'\n", argv[arg_off + 1]);
            cfg_free(&cfg);
            curl_global_cleanup();
            return 2;
        }
    } else if (strcmp(argv[arg_off], "issue") == 0) {
        if (arg_off + 1 >= argc) usage(argv[0]);

        if (strcmp(argv[arg_off + 1], "create") == 0) {
            if (arg_off + 3 >= argc) usage(argv[0]);
            const char *repo = argv[arg_off + 2];
            const char *title = argv[arg_off + 3];
            const char *body_text = find_opt(argc, argv, arg_off + 4, "--body");
            cmd_issue_create(base_url, token, repo, title, body_text);
        } else {
            fprintf(stderr, "forge: unknown issue command '%s'\n", argv[arg_off + 1]);
            cfg_free(&cfg);
            curl_global_cleanup();
            return 2;
        }
    } else {
        fprintf(stderr, "forge: unknown command '%s'\n", argv[arg_off]);
        cfg_free(&cfg);
        curl_global_cleanup();
        return 2;
    }

    cfg_free(&cfg);
    curl_global_cleanup();
    return 0;
}
