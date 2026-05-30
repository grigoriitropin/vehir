# IPM --help-json patch instructions

## File: `/home/vehir/ipm/src/idea.c`

### Change 1: Replace `usage()` function (lines 39-57)

Replace the entire `usage` function with the one in IPM_HELP_JSON_PATCH.c (already written).

### Change 2: Add `cmd_help_json()` BEFORE `usage()`

Insert this function before the existing `usage()` function (before line 39):

```c
static void cmd_help_json(PGconn *conn) {
    PGresult *res = db_exec_params(conn,
        "SELECT table_name, column_name, data_type, is_nullable "
        "FROM information_schema.columns "
        "WHERE table_schema = 'public' "
        "  AND table_name IN ('licenses', 'idea_packages', 'idea_edges') "
        "ORDER BY table_name, ordinal_position",
        0, NULL);

    int n = PQntuples(res);
    cJSON *schema = cJSON_CreateObject();

    cJSON *licenses = cJSON_CreateArray();
    cJSON *packages = cJSON_CreateArray();
    cJSON *edges    = cJSON_CreateArray();

    for (int i = 0; i < n; i++) {
        const char *table = PQgetvalue(res, i, 0);
        const char *col   = PQgetvalue(res, i, 1);
        const char *type  = PQgetvalue(res, i, 2);
        int required      = (strcmp(PQgetvalue(res, i, 3), "NO") == 0);

        cJSON *f = cJSON_CreateObject();
        cJSON_AddStringToObject(f, "name", col);
        cJSON_AddStringToObject(f, "type", type);
        cJSON_AddBoolToObject(f, "required", required ? 1 : 0);

        if (strcmp(table, "licenses") == 0) cJSON_AddItemToArray(licenses, f);
        else if (strcmp(table, "idea_packages") == 0) cJSON_AddItemToArray(packages, f);
        else if (strcmp(table, "idea_edges") == 0) cJSON_AddItemToArray(edges, f);
        else cJSON_Delete(f);
    }
    PQclear(res);

    cJSON *manifest = cJSON_CreateObject();
    cJSON_AddStringToObject(manifest, "licenses",
        "array of {name (TEXT, required), spdx_id (TEXT), kind (TEXT, required: owned|upstream_reference|original_patent), patent_grant (BOOLEAN)}");
    cJSON_AddStringToObject(manifest, "packages",
        "array of {name (TEXT, required), description (TEXT), artifact_type (TEXT: tool|service|binary|none), artifact_name (TEXT), license (TEXT), license_version (TEXT), is_axiom (BOOLEAN)}");
    cJSON_AddStringToObject(manifest, "edges",
        "array of {parent (TEXT, required), child (TEXT, required), relation (TEXT, required), link_type (TEXT, required: static|dynamic), edge_license (TEXT, required)}");

    cJSON_AddItemToObject(schema, "manifest_structure", manifest);
    cJSON_AddItemToObject(schema, "licenses_schema", licenses);
    cJSON_AddItemToObject(schema, "packages_schema", packages);
    cJSON_AddItemToObject(schema, "edges_schema", edges);

    json_print_ok(schema);
}
```

### Change 3: Replace `main()` function (lines 59-94)

Replace from `int main` to the closing `}` before `/* ---- helpers ---- */`:

```c
int main(int argc, char **argv) {
    db_set_json_errors(true);

    if (argc < 2) { usage(argv[0]); return 1; }

    /* --help-json before DB connection: still needs schema query so we connect first */
    if (strcmp(argv[1], "--help-json") == 0) {
        if (!getenv("IPM_DATABASE_URL")) {
            char dd[2048];
            resolve_datadir(dd, sizeof(dd));
            if (dir_has_pgdata(dd)) {
                char url[4096];
                snprintf(url, sizeof(url), "postgresql:///ipm?host=%s", dd);
                setenv("IPM_DATABASE_URL", url, 1);
            }
        }
        PGconn *conn = db_connect();
        cmd_help_json(conn);
        db_disconnect(conn);
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        cmd_init(argc - 2, argv + 2);
        return 0;
    }

    if (!getenv("IPM_DATABASE_URL")) {
        char dd[2048];
        resolve_datadir(dd, sizeof(dd));
        if (dir_has_pgdata(dd)) {
            char url[4096];
            snprintf(url, sizeof(url), "postgresql:///ipm?host=%s", dd);
            setenv("IPM_DATABASE_URL", url, 1);
        }
    }

    PGconn *conn = db_connect();

    if (strcmp(argv[1], "apply") == 0) {
        /* check for --help-json as subcommand */
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--help-json") == 0) {
                cmd_help_json(conn);
                db_disconnect(conn);
                return 0;
            }
        }
        cmd_apply(conn, argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "add") == 0)        cmd_add(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "link") == 0)       cmd_link(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "license") == 0)    cmd_license(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "show") == 0)       cmd_show(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "status") == 0)     cmd_status(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "check") == 0)      cmd_check(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "build") == 0)      cmd_build(conn, argc - 2, argv + 2);
    else if (strcmp(argv[1], "legal") == 0)      cmd_legal(conn, argc - 2, argv + 2);
    else {
        usage(argv[0]);
        fprintf(stderr, "\nFor the expected manifest JSON format, run: %s --help-json\n", argv[0]);
        db_disconnect(conn);
        return 1;
    }

    db_disconnect(conn);
    return 0;
}
```

### Change 4: Update `cmd_apply()` — edge validation error (lines 824-826)

Replace:
```c
            if (!parent || !child || !relation || !link_type || !edge_lic)
                json_die("edge missing required fields",
                         "need: parent, child, relation, link_type, edge_license");
```

With:
```c
            if (!parent || !child || !relation || !link_type || !edge_lic) {
                fprintf(stderr, "ERROR: edge missing required fields. Expected format:\n");
                cmd_help_json(conn);
                exit(1);
            }
```

### Verify

After applying changes, rebuild IPM and test:
```
make -C ~/ipm
idea --help          # should show --help-json mention
idea --help-json     # dynamic schema from DB
idea                 # no args → help
idea apply bad.json  # invalid → help-json
```
