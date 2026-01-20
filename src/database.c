#include "database.h"


sqlite3* db_open(const char *filename) 
{
    sqlite3 *db; // the database connection object.
    /*
        Open a connection to a new or existing SQLite database. 
        and returns a database connection object.
    */
    int rc = sqlite3_open(filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        // Destructor for sqlite3.
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

void db_close(sqlite3 *db) 
{
    if (db) {
        sqlite3_close(db);
    }
}

int db_exec(sqlite3 *db, const char *sql) 
{
    char *err_msg = NULL;
    /*
        A wrapper function that does :

        1- sqlite3_prepare()            // The constructor for sqlite3_stmt. Compile SQL text into byte-code
                                        // that will do the work of querying or updating the database.
        2- sqlite3_step()               // Advance an sqlite3_stmt to the next result row or to completion.
        3- sqlite3_column()             // a single column values in the current result row for an sqlite3_stmt.
        4- sqlite3_finalize()           // Destructor for sqlite3_stmt.

        for a string of one or more SQL statements.
     */
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    return 0;
}

int db_begin(sqlite3 *db) 
{
    return db_exec(db, "BEGIN TRANSACTION");
}

int db_commit(sqlite3 *db) 
{
    return db_exec(db, "COMMIT");
}

int db_rollback(sqlite3 *db) 
{
    return db_exec(db, "ROLLBACK");
}

int db_create_table(sqlite3 *db, const char *table_name, const char *schema) 
{
    char sql[1024];
    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS %s (%s)", table_name, schema);
    return db_exec(db, sql);
}

int db_drop_table(sqlite3 *db, const char *table_name) 
{
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS %s", table_name);
    return db_exec(db, sql);
}

int db_table_exists(sqlite3 *db, const char *table_name) 
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) 
    {
        return 0;
    }
    /*
        the application can invoke the sqlite3_bind() interfaces to attach values to the parameters.
        Each call to sqlite3_bind() overrides prior bindings on the same parameter. 
    */
    sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return exists;
}

int db_insert(sqlite3 *db, const char *table, db_value *values, size_t count) 
{
    if (count == 0) return -1;
    
    char sql[2048] = {0};
    char cols[1024] = {0};
    char placeholders[1024] = {0};
    
    for (size_t i = 0; i < count; i++) 
    {
        if (i > 0) {
            strcat_s(cols, 1024, ", ");
            strcat_s(placeholders, 1024, ", ");
        }
        strcat_s(cols, 1024, values[i].key);
        strcat_s(placeholders, 1024, "?");
    }
    
    snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (%s)", 
             table, cols, placeholders);
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) 
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) 
    {
        int idx = i + 1;
        switch (values[i].type) 
        {
            case DB_INT:
                sqlite3_bind_int(stmt, idx, values[i].value.int_val);
                break;
            case DB_TEXT:
                sqlite3_bind_text(stmt, idx, values[i].value.text_val, -1, SQLITE_TRANSIENT);
                break;
            case DB_DOUBLE:
                sqlite3_bind_double(stmt, idx, values[i].value.double_val);
                break;
            case DB_BLOB:
                sqlite3_bind_blob(stmt, idx, values[i].value.blob_val.data, 
                                values[i].value.blob_val.size, SQLITE_TRANSIENT);
                break;
            case DB_NULL:
                sqlite3_bind_null(stmt, idx);
                break;
        }
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

long long db_insert_id(sqlite3 *db, const char *table, db_value *values, size_t count) 
{
    if (db_insert(db, table, values, count) == 0) {
        return sqlite3_last_insert_rowid(db);
    }
    return -1;
}

int db_update(sqlite3 *db, const char *table, db_value *values, size_t count, 
              const char *where_clause) 
{
    if (count == 0) return -1;
    
    char sql[2048] = {0};
    char set_clause[1024] = {0};
    
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat_s(set_clause, 1024, ", ");
        strcat_s(set_clause,1024, values[i].key);
        strcat_s(set_clause,1024, " = ?");
    }
    
    snprintf(sql, sizeof(sql), "UPDATE %s SET %s", table, set_clause);
    if (where_clause && where_clause[0]) {
        strcat_s(sql,2048, " WHERE ");
        strcat_s(sql,2048, where_clause);
    }
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) 
    {
        int idx = i + 1;
        switch (values[i].type) 
        {
            case DB_INT:
                sqlite3_bind_int(stmt, idx, values[i].value.int_val);
                break;
            case DB_TEXT:
                sqlite3_bind_text(stmt, idx, values[i].value.text_val, -1, SQLITE_TRANSIENT);
                break;
            case DB_DOUBLE:
                sqlite3_bind_double(stmt, idx, values[i].value.double_val);
                break;
            case DB_BLOB:
                sqlite3_bind_blob(stmt, idx, values[i].value.blob_val.data, 
                                values[i].value.blob_val.size, SQLITE_TRANSIENT);
                break;
            case DB_NULL:
                sqlite3_bind_null(stmt, idx);
                break;
        }
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete(sqlite3 *db, const char *table, const char *where_clause) 
{
    char sql[512];
    snprintf(sql, sizeof(sql), "DELETE FROM %s", table);
    if (where_clause && where_clause[0]) {
        strcat_s(sql, 512, " WHERE ");
        strcat_s(sql, 512, where_clause);
    }
    return db_exec(db, sql);
}

int db_delete_all(sqlite3 *db, const char *table) 
{
    return db_delete(db, table, NULL);
}

db_result* db_select(sqlite3 *db, const char *query) 
{
    db_result *result = malloc(sizeof(db_result));
    if (!result) return NULL;
    
    if (sqlite3_prepare_v2(db, query, -1, &result->stmt, NULL) != SQLITE_OK) 
    {
        fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        free(result);
        return NULL;
    }
    
    result->done = 0;
    result->column_count = sqlite3_column_count(result->stmt);
    return result;
}

int db_next(db_result *result) 
{
    if (result->done) return 0;
    
    int rc = sqlite3_step(result->stmt);
    if (rc == SQLITE_ROW) {
        return 1;
    }
    result->done = 1;
    return 0;
}

int db_get_int(db_result *result, int col) 
{
    return sqlite3_column_int(result->stmt, col);
}

double db_get_double(db_result *result, int col) 
{
    return sqlite3_column_double(result->stmt, col);
}

const char* db_get_text(db_result *result, int col) 
{
    return (const char*)sqlite3_column_text(result->stmt, col);
}

const void* db_get_blob(db_result *result, int col, size_t *size) 
{
    if (size) {
        *size = sqlite3_column_bytes(result->stmt, col);
    }
    return sqlite3_column_blob(result->stmt, col);
}

void db_dump(db_result *r)
{
    if (!r) return;

    for (int i = 0; i < r->column_count; i++) {
        const char *name = sqlite3_column_name(r->stmt, i);
        printf("%s", name ? name : "?");
        if (i + 1 < r->column_count)
            printf(" | ");
    }
    printf("\n");

    for (int i = 0; i < r->column_count; i++) {
        printf("----------");
        if (i + 1 < r->column_count)
            printf("+");
    }
    printf("\n");

    while (db_next(r)) {
        for (int i = 0; i < r->column_count; i++) {
            int type = sqlite3_column_type(r->stmt, i);

            switch (type) {
                case SQLITE_INTEGER:
                    printf("%lld", sqlite3_column_int64(r->stmt, i));
                    break;

                case SQLITE_FLOAT:
                    printf("%f", sqlite3_column_double(r->stmt, i));
                    break;

                case SQLITE_TEXT:
                    printf("%s", sqlite3_column_text(r->stmt, i));
                    break;

                case SQLITE_BLOB: {
                    int n = sqlite3_column_bytes(r->stmt, i);
                    printf("<blob %d bytes>", n);
                } break;

                case SQLITE_NULL:
                    printf("NULL");
                    break;
            }

            if (i + 1 < r->column_count)
                printf(" | ");
        }
        printf("\n");
    }
}

void db_log_table(sqlite3 *db, const char *table)
{
    char sql[512] = {0};
    snprintf(sql,512,"SELECT * FROM %s", table);

    db_result *r = db_select(db, sql);
    db_dump(r);
    db_result_free(r);
}

int db_is_null(db_result *result, int col) 
{
    return sqlite3_column_type(result->stmt, col) == SQLITE_NULL;
}

int db_column_count(db_result *result) 
{
    return result->column_count;
}

const char* db_column_name(db_result *result, int col) 
{
    return sqlite3_column_name(result->stmt, col);
}

void db_result_free(db_result *result) 
{
    if (result) {
        sqlite3_finalize(result->stmt);
        free(result);
    }
}

db_query* db_query_new(const char *table) 
{
    db_query *q = calloc(1, sizeof(db_query));
    if (!q) return NULL;
    
    strncpy(q->table, table, sizeof(q->table) - 1);
    strcpy(q->columns, "*");
    q->limit = -1;
    q->offset = -1;
    return q;
}

void db_query_select(db_query *q, const char *columns) 
{
    strncpy(q->columns, columns, sizeof(q->columns) - 1);
}

void db_query_where(db_query *q, const char *condition) 
{
    if (q->where[0]) {
        strcat(q->where, " AND ");
    }
    strncat(q->where, condition, sizeof(q->where) - strlen(q->where) - 1);
}

void db_query_order(db_query *q, const char *order) 
{
    strncpy(q->order, order, sizeof(q->order) - 1);
}

void db_query_group(db_query *q, const char *group) 
{
    strncpy(q->group, group, sizeof(q->group) - 1);
}

void db_query_limit(db_query *q, int limit) 
{
    q->limit = limit;
}

void db_query_offset(db_query *q, int offset) 
{
    q->offset = offset;
}

db_result* db_query_exec(sqlite3 *db, db_query *q) 
{
    char sql[2048] = {0};
    
    snprintf(sql, sizeof(sql), "SELECT %s FROM %s", q->columns, q->table);
    
    if (q->where[0]) {
        strcat(sql, " WHERE ");
        strcat(sql, q->where);
    }
    
    if (q->group[0]) {
        strcat(sql, " GROUP BY ");
        strcat(sql, q->group);
    }
    
    if (q->order[0]) {
        strcat(sql, " ORDER BY ");
        strcat(sql, q->order);
    }
    
    if (q->limit >= 0) {
        char limit_str[32];
        snprintf(limit_str, sizeof(limit_str), " LIMIT %d", q->limit);
        strcat(sql, limit_str);
    }
    
    if (q->offset >= 0) {
        char offset_str[32];
        snprintf(offset_str, sizeof(offset_str), " OFFSET %d", q->offset);
        strcat(sql, offset_str);
    }
    
    return db_select(db, sql);
}

void db_query_free(db_query *q) 
{
    free(q);
}

int db_changes(sqlite3 *db) 
{
    return sqlite3_changes(db);
}

const char* db_error(sqlite3 *db) 
{
    return sqlite3_errmsg(db);
}

long long db_count(sqlite3 *db, const char *table, const char *where_clause) 
{
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
    if (where_clause && where_clause[0]) {
        strcat(sql, " WHERE ");
        strcat(sql, where_clause);
    }
    
    db_result *result = db_select(db, sql);
    if (!result) return -1;
    
    long long count = 0;
    if (db_next(result)) {
        count = db_get_int(result, 0);
    }
    
    db_result_free(result);
    return count;
}