#ifndef VDB_H
#define VDB_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Page constants ──────────────────────────────────────────────────── */

#define PAGE_SIZE       4096
#define MAX_PAGES       100

/* ── Row schema: fixed layout ────────────────────────────────────────── */
/*
 *  id      uint32_t    4  bytes
 *  name    char[33]    33 bytes  (32 + NUL)
 *  email   char[65]    65 bytes  (64 + NUL)
 *  ─────────────────────────────
 *  total               102 bytes
 */

#define COL_NAME_SIZE   32
#define COL_EMAIL_SIZE  64

typedef struct {
    uint32_t id;
    char     name[COL_NAME_SIZE + 1];
    char     email[COL_EMAIL_SIZE + 1];
} Row;

/* Serialized sizes (packed, no padding) */
#define ROW_ID_SIZE     sizeof(uint32_t)
#define ROW_ID_OFFSET   0
#define ROW_NAME_SIZE   (COL_NAME_SIZE + 1)
#define ROW_NAME_OFFSET (ROW_ID_OFFSET + ROW_ID_SIZE)
#define ROW_EMAIL_SIZE  (COL_EMAIL_SIZE + 1)
#define ROW_EMAIL_OFFSET (ROW_NAME_OFFSET + ROW_NAME_SIZE)
#define ROW_SIZE        (ROW_ID_SIZE + ROW_NAME_SIZE + ROW_EMAIL_SIZE)

/* ── B+Tree node types ───────────────────────────────────────────────── */

typedef enum {
    NODE_INTERNAL,
    NODE_LEAF
} NodeType;

/*
 * Common node header layout (at the start of every page):
 *
 *  Offset  Size  Field
 *  ──────  ────  ─────────────────
 *  0       1     node_type
 *  1       1     is_root
 *  2       4     parent_page_num
 *  ──────────────────────────────
 *  total   6 bytes
 */
#define NODE_TYPE_SIZE          1
#define NODE_TYPE_OFFSET        0
#define IS_ROOT_SIZE            1
#define IS_ROOT_OFFSET          (NODE_TYPE_OFFSET + NODE_TYPE_SIZE)
#define PARENT_PTR_SIZE         4
#define PARENT_PTR_OFFSET       (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_PTR_SIZE)

/*
 * Leaf node layout:
 *
 *  [common header][num_cells (4)][next_leaf (4)][cell 0][cell 1]...
 *
 *  Each cell = [key (4 bytes)] + [value (ROW_SIZE bytes)]
 */
#define LEAF_NODE_NUM_CELLS_SIZE    4
#define LEAF_NODE_NUM_CELLS_OFFSET  COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_SIZE    4
#define LEAF_NODE_NEXT_LEAF_OFFSET  (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE       (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

#define LEAF_NODE_KEY_SIZE          4
#define LEAF_NODE_VALUE_SIZE        ROW_SIZE
#define LEAF_NODE_CELL_SIZE         (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS   (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS         (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

/* Split: left gets ceil(n/2), right gets floor(n/2) */
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)
#define LEAF_NODE_LEFT_SPLIT_COUNT  ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT)

/*
 * Internal node layout:
 *
 *  [common header][num_keys (4)][right_child (4)][child0 (4)][key0 (4)][child1 (4)][key1 (4)]...
 *
 *  right_child is the page number of the rightmost child (no key for it).
 */
#define INTERNAL_NODE_NUM_KEYS_SIZE     4
#define INTERNAL_NODE_NUM_KEYS_OFFSET   COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_SIZE  4
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE       (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

#define INTERNAL_NODE_KEY_SIZE          4
#define INTERNAL_NODE_CHILD_SIZE        4
#define INTERNAL_NODE_CELL_SIZE         (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)
#define INTERNAL_NODE_SPACE_FOR_CELLS   (PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE)
#define INTERNAL_NODE_MAX_KEYS          (INTERNAL_NODE_SPACE_FOR_CELLS / INTERNAL_NODE_CELL_SIZE)

#endif /* VDB_H */
