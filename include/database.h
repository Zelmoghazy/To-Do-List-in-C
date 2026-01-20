#ifndef DB_H
#define DB_H

#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef struct {
    const char *key;
    enum { DB_INT, DB_TEXT, DB_DOUBLE, DB_BLOB, DB_NULL } type;
    union {
        int         int_val;
        const char *text_val;
        double      double_val;
        struct {
            const void *data;
            size_t size;
        } blob_val;
    } value;
} db_value;

typedef struct {
    sqlite3_stmt *stmt;
    int done;
    int column_count;
} db_result;

sqlite3* db_open(const char *filename);
void db_close(sqlite3 *db);

int db_exec(sqlite3 *db, const char *sql);

int db_begin(sqlite3 *db);
int db_commit(sqlite3 *db);
int db_rollback(sqlite3 *db);

int db_create_table(sqlite3 *db, const char *table_name, const char *schema);
int db_drop_table(sqlite3 *db, const char *table_name);
int db_table_exists(sqlite3 *db, const char *table_name);

int db_insert(sqlite3 *db, const char *table, db_value *values, size_t count);
long long db_insert_id(sqlite3 *db, const char *table, db_value *values, size_t count);
int db_update(sqlite3 *db, const char *table, db_value *values, size_t count, 
              const char *where_clause);
int db_delete(sqlite3 *db, const char *table, const char *where_clause);
int db_delete_all(sqlite3 *db, const char *table);


db_result* db_select(sqlite3 *db, const char *query);

int db_next(db_result *result);

int db_get_int(db_result *result, int col);
double db_get_double(db_result *result, int col);
const char* db_get_text(db_result *result, int col);
const void* db_get_blob(db_result *result, int col, size_t *size);
int db_is_null(db_result *result, int col);

int db_column_count(db_result *result);
const char* db_column_name(db_result *result, int col);

void db_result_free(db_result *result);

typedef struct {
    char table[128];
    char columns[512];
    char where[512];
    char order[256];
    char group[256];
    int limit;
    int offset;
} db_query;

db_query* db_query_new(const char *table);
void db_query_select(db_query *q, const char *columns);
void db_query_where(db_query *q, const char *condition);
void db_query_order(db_query *q, const char *order);
void db_query_group(db_query *q, const char *group);
void db_query_limit(db_query *q, int limit);
void db_query_offset(db_query *q, int offset);
db_result* db_query_exec(sqlite3 *db, db_query *q);
void db_query_free(db_query *q);

int db_changes(sqlite3 *db);
const char* db_error(sqlite3 *db);
long long db_count(sqlite3 *db, const char *table, const char *where_clause);

#endif // DB_H
