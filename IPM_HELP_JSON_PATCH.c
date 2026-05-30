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

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <command> [args...]\n"
        "       %s --help-json           show manifest JSON format (dynamic from schema)\n"
        "\n"
        "Commands:\n"
        "  init [--stop]\n"
        "  apply <file.json | -> [--dry-run] [--force] [--prune]\n"
        "  add <name> --desc \"...\" [--artifact-type t] [--artifact-name n] [--axiom]\n"
        "  link <parent> <child> --relation R --link-type t --edge-license L\n"
        "  license add <name> [--spdx ID] --kind K [--patent-grant]\n"
        "  license set <package> <license> [--version V]\n"
        "  license show\n"
        "  show [<name>]\n"
        "  status\n"
        "  check [<name>]\n"
        "  build [<name>]\n"
        "  legal set <subject> <ref> <jurisdiction> <question> <status>\n"
        "  legal show <subject> <ref>\n"
        "\n"
        "  --help-json   Display the expected manifest JSON format.\n"
        "                Reads live schema from the IPM database.\n"
        "                Use: idea apply --help-json or idea --help-json\n",
        prog, prog);
}
