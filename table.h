#ifndef TABLE_H
#define TABLE_H

#include "vdb.h"
#include "pager.h"
#include "btree.h"

typedef struct {
    Pager    *pager;
    uint32_t  root_page_num;
} Table;

/* Open a table backed by the given database file. */
Table *table_open(const char *filename);

/* Close the table (flushes all pages to disk). */
void   table_close(Table *table);

/* Insert a row. Returns 0 on success, -1 on error. */
int    table_insert(Table *table, Row *row);

/* Select a row by id. Returns 0 on success, -1 if not found. */
int    table_select(Table *table, uint32_t id, Row *out);

/* Delete a row by id. Returns 0 on success, -1 if not found. */
int    table_delete(Table *table, uint32_t id);

/* Iterate all rows, calling callback for each. */
void   table_select_all(Table *table, void (*cb)(Row *row, void *ctx), void *ctx);

/* Print the B+Tree structure (for .btree debug command). */
void   table_print_tree(Table *table);

#endif /* TABLE_H */
