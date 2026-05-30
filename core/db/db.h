// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#pragma once
#include <stddef.h>

#define VEHIR_DB_ERROR_SZ 1024
#define VEHIR_DB_MAX_ROWS 256
#define VEHIR_DB_MAX_COLS  32
#define VEHIR_DB_COL_SZ     64
#define VEHIR_DB_VAL_SZ    512

typedef struct vehir_db vehir_db;

typedef struct vehir_db_result vehir_db_result;

vehir_db        *vehir_db_open(void);
void             vehir_db_close(vehir_db *db);
int              vehir_db_exec(vehir_db *db, const char *sql,
                               const char *const *params, int nparams);
vehir_db_result *vehir_db_query(vehir_db *db, const char *sql,
                                 const char *const *params, int nparams);
void             vehir_db_result_free(vehir_db_result *r);
const char      *vehir_db_error(const vehir_db *db);

int              vehir_db_result_nrows(const vehir_db_result *r);
int              vehir_db_result_ncols(const vehir_db_result *r);
const char      *vehir_db_result_colname(const vehir_db_result *r, int col);
const char      *vehir_db_result_value(const vehir_db_result *r, int row, int col);
int              vehir_db_result_isnull(const vehir_db_result *r, int row, int col);
const char      *vehir_db_result_error(const vehir_db_result *r);
