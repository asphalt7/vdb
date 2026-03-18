#ifndef BTREE_H
#define BTREE_H

#include "vdb.h"
#include "pager.h"

/* ── Node accessor helpers ───────────────────────────────────────────── */

/* Common header */
NodeType  node_get_type(void *node);
void      node_set_type(void *node, NodeType type);
uint8_t   node_is_root(void *node);
void      node_set_root(void *node, uint8_t is_root);
uint32_t  node_get_parent(void *node);
void      node_set_parent(void *node, uint32_t parent);

/* Leaf node */
uint32_t *leaf_node_num_cells(void *node);
uint32_t *leaf_node_next_leaf(void *node);
void     *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t *leaf_node_key(void *node, uint32_t cell_num);
void     *leaf_node_value(void *node, uint32_t cell_num);
uint32_t  leaf_node_max_key(void *node);

/* Internal node */
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
uint32_t  internal_node_max_key(Pager *pager, void *node);

uint32_t  node_get_max_key(Pager *pager, void *node);

/* ── Initialization ──────────────────────────────────────────────────── */

void initialize_leaf_node(void *node);
void initialize_internal_node(void *node);

/* ── B+Tree cursor ───────────────────────────────────────────────────── */

typedef struct {
    Pager    *pager;
    uint32_t  page_num;     /* current leaf page */
    uint32_t  cell_num;     /* current cell within the leaf */
    uint8_t   end_of_table; /* 1 if past last row */
} Cursor;

/* Find the leaf node + cell position for a given key. */
Cursor *btree_find(Pager *pager, uint32_t root_page, uint32_t key);

/* Insert a row into the tree. May trigger splits. */
void    btree_insert(Cursor *cursor, uint32_t key, Row *value,
                     uint32_t *root_page_num);

/* Delete a key from the tree (leaf-only simplified delete). */
void    btree_delete(Cursor *cursor);

/* Advance cursor to the next row. */
void    cursor_advance(Cursor *cursor);

/* Get pointer to the value (row) at cursor position. */
void   *cursor_value(Cursor *cursor);

/* Create a cursor pointing to the start of the table. */
Cursor *btree_start(Pager *pager, uint32_t root_page);

/* ── Debug ───────────────────────────────────────────────────────────── */

void btree_print(Pager *pager, uint32_t page_num, int indent);

#endif /* BTREE_H */
