#include "table.h"

Table *table_open(const char *filename)
{
    Pager *pager = pager_open(filename);
    Table *table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if (pager->num_pages == 0) {
        /* New database: initialize root as empty leaf */
        void *root = pager_get_page(pager, 0);
        initialize_leaf_node(root);
        node_set_root(root, 1);
    }

    return table;
}

void table_close(Table *table)
{
    pager_close(table->pager);
    free(table);
}

int table_insert(Table *table, Row *row)
{
    Cursor *cursor = btree_find(table->pager, table->root_page_num, row->id);
    void *node = pager_get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    /* Check for duplicate */
    if (cursor->cell_num < num_cells) {
        uint32_t existing = *leaf_node_key(node, cursor->cell_num);
        if (existing == row->id) {
            fprintf(stderr, "error: duplicate key %u\n", row->id);
            free(cursor);
            return -1;
        }
    }

    btree_insert(cursor, row->id, row, &table->root_page_num);
    free(cursor);
    return 0;
}

int table_select(Table *table, uint32_t id, Row *out)
{
    Cursor *cursor = btree_find(table->pager, table->root_page_num, id);
    void *node = pager_get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor->cell_num < num_cells &&
        *leaf_node_key(node, cursor->cell_num) == id) {
        void *val = cursor_value(cursor);
        memcpy(&out->id,    (uint8_t *)val + ROW_ID_OFFSET,   ROW_ID_SIZE);
        memcpy(out->name,   (uint8_t *)val + ROW_NAME_OFFSET, ROW_NAME_SIZE);
        memcpy(out->email,  (uint8_t *)val + ROW_EMAIL_OFFSET,ROW_EMAIL_SIZE);
        out->name[COL_NAME_SIZE] = '\0';
        out->email[COL_EMAIL_SIZE] = '\0';
        free(cursor);
        return 0;
    }

    free(cursor);
    return -1;
}

int table_delete(Table *table, uint32_t id)
{
    Cursor *cursor = btree_find(table->pager, table->root_page_num, id);
    void *node = pager_get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor->cell_num < num_cells &&
        *leaf_node_key(node, cursor->cell_num) == id) {
        btree_delete(cursor);
        free(cursor);
        return 0;
    }

    free(cursor);
    return -1;
}

static void print_row_cb(Row *row, void *ctx)
{
    (void)ctx;
    printf("(%u, %s, %s)\n", row->id, row->name, row->email);
}

void table_select_all(Table *table, void (*cb)(Row *row, void *ctx), void *ctx)
{
    if (!cb) cb = print_row_cb;

    Cursor *cursor = btree_start(table->pager, table->root_page_num);

    while (!cursor->end_of_table) {
        void *val = cursor_value(cursor);
        Row row;
        memcpy(&row.id,    (uint8_t *)val + ROW_ID_OFFSET,   ROW_ID_SIZE);
        memcpy(row.name,   (uint8_t *)val + ROW_NAME_OFFSET, ROW_NAME_SIZE);
        memcpy(row.email,  (uint8_t *)val + ROW_EMAIL_OFFSET,ROW_EMAIL_SIZE);
        row.name[COL_NAME_SIZE] = '\0';
        row.email[COL_EMAIL_SIZE] = '\0';
        cb(&row, ctx);
        cursor_advance(cursor);
    }

    free(cursor);
}

void table_print_tree(Table *table)
{
    btree_print(table->pager, table->root_page_num, 0);
}
