#include "btree.h"

/* ── Row serialization ───────────────────────────────────────────────── */

static void serialize_row(Row *src, void *dst)
{
    memcpy((uint8_t *)dst + ROW_ID_OFFSET,    &src->id,    ROW_ID_SIZE);
    memcpy((uint8_t *)dst + ROW_NAME_OFFSET,   src->name,  ROW_NAME_SIZE);
    memcpy((uint8_t *)dst + ROW_EMAIL_OFFSET,  src->email,  ROW_EMAIL_SIZE);
}

static void deserialize_row(void *src, Row *dst)
{
    memcpy(&dst->id,    (uint8_t *)src + ROW_ID_OFFSET,    ROW_ID_SIZE);
    memcpy(dst->name,   (uint8_t *)src + ROW_NAME_OFFSET,  ROW_NAME_SIZE);
    memcpy(dst->email,  (uint8_t *)src + ROW_EMAIL_OFFSET, ROW_EMAIL_SIZE);
    dst->name[COL_NAME_SIZE] = '\0';
    dst->email[COL_EMAIL_SIZE] = '\0';
}

/* ── Common node header accessors ────────────────────────────────────── */

NodeType node_get_type(void *node)
{
    uint8_t val = *((uint8_t *)node + NODE_TYPE_OFFSET);
    return (NodeType)val;
}

void node_set_type(void *node, NodeType type)
{
    uint8_t val = (uint8_t)type;
    *((uint8_t *)node + NODE_TYPE_OFFSET) = val;
}

uint8_t node_is_root(void *node)
{
    return *((uint8_t *)node + IS_ROOT_OFFSET);
}

void node_set_root(void *node, uint8_t is_root)
{
    *((uint8_t *)node + IS_ROOT_OFFSET) = is_root;
}

uint32_t node_get_parent(void *node)
{
    uint32_t val;
    memcpy(&val, (uint8_t *)node + PARENT_PTR_OFFSET, sizeof(val));
    return val;
}

void node_set_parent(void *node, uint32_t parent)
{
    memcpy((uint8_t *)node + PARENT_PTR_OFFSET, &parent, sizeof(parent));
}

/* ── Leaf node accessors ─────────────────────────────────────────────── */

uint32_t *leaf_node_num_cells(void *node)
{
    return (uint32_t *)((uint8_t *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

uint32_t *leaf_node_next_leaf(void *node)
{
    return (uint32_t *)((uint8_t *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

void *leaf_node_cell(void *node, uint32_t cell_num)
{
    return (uint8_t *)node + LEAF_NODE_HEADER_SIZE +
           cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
    return (uint32_t *)leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num)
{
    return (uint8_t *)leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

uint32_t leaf_node_max_key(void *node)
{
    uint32_t n = *leaf_node_num_cells(node);
    return *leaf_node_key(node, n - 1);
}

/* ── Internal node accessors ─────────────────────────────────────────── */

uint32_t *internal_node_num_keys(void *node)
{
    return (uint32_t *)((uint8_t *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint32_t *internal_node_right_child(void *node)
{
    return (uint32_t *)((uint8_t *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return (uint32_t *)((uint8_t *)node + INTERNAL_NODE_HEADER_SIZE +
                        cell_num * INTERNAL_NODE_CELL_SIZE);
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_keys(node);
    if (child_num > num_keys) {
        fprintf(stderr, "error: child_num %u > num_keys %u\n",
                child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    if (child_num == num_keys) {
        return internal_node_right_child(node);
    }
    return internal_node_cell(node, child_num);
}

uint32_t *internal_node_key(void *node, uint32_t key_num)
{
    return (uint32_t *)((uint8_t *)internal_node_cell(node, key_num) +
                        INTERNAL_NODE_CHILD_SIZE);
}

uint32_t internal_node_max_key(Pager *pager, void *node)
{
    uint32_t num_keys = *internal_node_num_keys(node);
    if (num_keys > 0)
        return *internal_node_key(node, num_keys - 1);
    /* Recurse into rightmost child */
    uint32_t rc = *internal_node_right_child(node);
    void *child = pager_get_page(pager, rc);
    return node_get_max_key(pager, child);
}

uint32_t node_get_max_key(Pager *pager, void *node)
{
    if (node_get_type(node) == NODE_LEAF)
        return leaf_node_max_key(node);
    return internal_node_max_key(pager, node);
}

/* ── Node initialization ─────────────────────────────────────────────── */

void initialize_leaf_node(void *node)
{
    node_set_type(node, NODE_LEAF);
    node_set_root(node, 0);
    *leaf_node_num_cells(node) = 0;
    *leaf_node_next_leaf(node) = 0; /* 0 = no sibling */
}

void initialize_internal_node(void *node)
{
    node_set_type(node, NODE_INTERNAL);
    node_set_root(node, 0);
    *internal_node_num_keys(node) = 0;
    *internal_node_right_child(node) = 0;
}

/* ── Search: find leaf + cell for a key ──────────────────────────────── */

static uint32_t leaf_node_find(void *node, uint32_t key)
{
    uint32_t num_cells = *leaf_node_num_cells(node);
    uint32_t lo = 0, hi = num_cells;

    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t mid_key = *leaf_node_key(node, mid);
        if (key <= mid_key)
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

static uint32_t internal_node_find_child(void *node, uint32_t key)
{
    uint32_t num_keys = *internal_node_num_keys(node);
    uint32_t lo = 0, hi = num_keys;

    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t mid_key = *internal_node_key(node, mid);
        if (key <= mid_key)
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

Cursor *btree_find(Pager *pager, uint32_t root_page, uint32_t key)
{
    void *node = pager_get_page(pager, root_page);

    if (node_get_type(node) == NODE_LEAF) {
        Cursor *cursor = malloc(sizeof(Cursor));
        cursor->pager = pager;
        cursor->page_num = root_page;
        cursor->cell_num = leaf_node_find(node, key);
        cursor->end_of_table = 0;
        return cursor;
    }

    /* Internal: descend to the correct child */
    uint32_t child_idx = internal_node_find_child(node, key);
    uint32_t child_page = *internal_node_child(node, child_idx);
    return btree_find(pager, child_page, key);
}

/* ── Cursor helpers ──────────────────────────────────────────────────── */

Cursor *btree_start(Pager *pager, uint32_t root_page)
{
    /* Find the leftmost leaf */
    void *node = pager_get_page(pager, root_page);
    uint32_t page = root_page;

    while (node_get_type(node) == NODE_INTERNAL) {
        uint32_t child = *internal_node_child(node, 0);
        page = child;
        node = pager_get_page(pager, page);
    }

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->pager = pager;
    cursor->page_num = page;
    cursor->cell_num = 0;
    cursor->end_of_table = (*leaf_node_num_cells(node) == 0);
    return cursor;
}

void cursor_advance(Cursor *cursor)
{
    void *node = pager_get_page(cursor->pager, cursor->page_num);
    cursor->cell_num++;

    if (cursor->cell_num >= *leaf_node_num_cells(node)) {
        /* Move to next leaf */
        uint32_t next = *leaf_node_next_leaf(node);
        if (next == 0) {
            cursor->end_of_table = 1;
        } else {
            cursor->page_num = next;
            cursor->cell_num = 0;
        }
    }
}

void *cursor_value(Cursor *cursor)
{
    void *node = pager_get_page(cursor->pager, cursor->page_num);
    return leaf_node_value(node, cursor->cell_num);
}

/* ── Leaf split ──────────────────────────────────────────────────────── */

static void create_new_root(Pager *pager, uint32_t *root_page_num,
                            uint32_t right_page);

static void leaf_node_split_and_insert(Cursor *cursor, uint32_t key,
                                       Row *value, uint32_t *root_page_num)
{
    void *old_node = pager_get_page(cursor->pager, cursor->page_num);
    uint32_t old_max = leaf_node_max_key(old_node);
    uint32_t new_page_num = pager_get_unused_page(cursor->pager);
    void *new_node = pager_get_page(cursor->pager, new_page_num);
    initialize_leaf_node(new_node);
    node_set_parent(new_node, node_get_parent(old_node));

    /* Chain next_leaf pointers: old -> new -> old's original next */
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    /*
     * Redistribute cells: iterate all existing cells + the new one.
     * Cells 0..LEFT_SPLIT_COUNT-1 stay in old_node,
     * cells LEFT_SPLIT_COUNT..MAX_CELLS go to new_node.
     */
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
        void *dest_node;
        if ((uint32_t)i >= LEAF_NODE_LEFT_SPLIT_COUNT)
            dest_node = new_node;
        else
            dest_node = old_node;

        uint32_t index_within_node = (uint32_t)i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void *dest = leaf_node_cell(dest_node, index_within_node);

        if ((uint32_t)i == cursor->cell_num) {
            /* Insert the new cell */
            *leaf_node_key(dest_node, index_within_node) = key;
            serialize_row(value, leaf_node_value(dest_node, index_within_node));
        } else if ((uint32_t)i > cursor->cell_num) {
            memcpy(dest, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        } else {
            memcpy(dest, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }

    *leaf_node_num_cells(old_node) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *leaf_node_num_cells(new_node) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    if (node_is_root(old_node)) {
        create_new_root(cursor->pager, root_page_num, new_page_num);
    } else {
        /* Update parent */
        uint32_t parent_page = node_get_parent(old_node);
        void *parent = pager_get_page(cursor->pager, parent_page);
        uint32_t new_max = leaf_node_max_key(old_node);

        /* Update the key in parent that pointed to old_node */
        uint32_t old_child_idx = internal_node_find_child(parent, old_max);
        if (old_child_idx < *internal_node_num_keys(parent))
            *internal_node_key(parent, old_child_idx) = new_max;

        /* Insert new child into parent */
        uint32_t num_keys = *internal_node_num_keys(parent);
        if (num_keys >= INTERNAL_NODE_MAX_KEYS) {
            fprintf(stderr, "error: internal node full, need to implement internal split\n");
            exit(EXIT_FAILURE);
        }

        uint32_t right_child = *internal_node_right_child(parent);
        void *right_node = pager_get_page(cursor->pager, right_child);
        uint32_t right_max = node_get_max_key(cursor->pager, right_node);

        if (node_get_max_key(cursor->pager, new_node) > right_max) {
            /* New node becomes the rightmost child */
            *internal_node_child(parent, num_keys) = right_child;
            *internal_node_key(parent, num_keys) = right_max;
            *internal_node_right_child(parent) = new_page_num;
        } else {
            /* Insert in sorted position */
            uint32_t insert_idx = internal_node_find_child(parent,
                node_get_max_key(cursor->pager, new_node));

            for (uint32_t j = num_keys; j > insert_idx; j--) {
                memcpy(internal_node_cell(parent, j),
                       internal_node_cell(parent, j - 1),
                       INTERNAL_NODE_CELL_SIZE);
            }
            *internal_node_child(parent, insert_idx) = new_page_num;
            *internal_node_key(parent, insert_idx) =
                node_get_max_key(cursor->pager, new_node);
        }

        *internal_node_num_keys(parent) = num_keys + 1;
        node_set_parent(new_node, parent_page);
    }
}

/* ── Create new root after split ─────────────────────────────────────── */

static void create_new_root(Pager *pager, uint32_t *root_page_num,
                            uint32_t right_page)
{
    void *root = pager_get_page(pager, *root_page_num);

    /* Copy old root to a new page (becomes left child) */
    uint32_t left_page = pager_get_unused_page(pager);
    void *left_node = pager_get_page(pager, left_page);
    memcpy(left_node, root, PAGE_SIZE);
    node_set_root(left_node, 0);

    /* If left child is internal, update its children's parent pointers */
    if (node_get_type(left_node) == NODE_INTERNAL) {
        uint32_t nk = *internal_node_num_keys(left_node);
        for (uint32_t i = 0; i <= nk; i++) {
            uint32_t cp = *internal_node_child(left_node, i);
            void *child = pager_get_page(pager, cp);
            node_set_parent(child, left_page);
        }
    }

    /* Reinitialize root as an internal node */
    initialize_internal_node(root);
    node_set_root(root, 1);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_page;
    *internal_node_key(root, 0) = node_get_max_key(pager, left_node);
    *internal_node_right_child(root) = right_page;

    node_set_parent(left_node, *root_page_num);
    void *right_node = pager_get_page(pager, right_page);
    node_set_parent(right_node, *root_page_num);
}

/* ── Insert ──────────────────────────────────────────────────────────── */

void btree_insert(Cursor *cursor, uint32_t key, Row *value,
                  uint32_t *root_page_num)
{
    void *node = pager_get_page(cursor->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        /* Leaf full -- split */
        leaf_node_split_and_insert(cursor, key, value, root_page_num);
        return;
    }

    if (cursor->cell_num < num_cells) {
        /* Check for duplicate key */
        uint32_t existing_key = *leaf_node_key(node, cursor->cell_num);
        if (existing_key == key) {
            fprintf(stderr, "error: duplicate key %u\n", key);
            return;
        }
        /* Shift cells right to make room */
        for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
            memcpy(leaf_node_cell(node, i),
                   leaf_node_cell(node, i - 1),
                   LEAF_NODE_CELL_SIZE);
        }
    }

    *leaf_node_num_cells(node) = num_cells + 1;
    *leaf_node_key(node, cursor->cell_num) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

/* ── Delete (simplified: leaf-only compaction) ───────────────────────── */

void btree_delete(Cursor *cursor)
{
    void *node = pager_get_page(cursor->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor->cell_num >= num_cells) return;

    /* Shift cells left */
    for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++) {
        memcpy(leaf_node_cell(node, i),
               leaf_node_cell(node, i + 1),
               LEAF_NODE_CELL_SIZE);
    }

    *leaf_node_num_cells(node) = num_cells - 1;
}

/* ── Debug print ─────────────────────────────────────────────────────── */

void btree_print(Pager *pager, uint32_t page_num, int indent)
{
    void *node = pager_get_page(pager, page_num);

    if (node_get_type(node) == NODE_LEAF) {
        uint32_t num_cells = *leaf_node_num_cells(node);
        printf("%*sLeaf (page %u, size %u)\n", indent, "", page_num, num_cells);
        for (uint32_t i = 0; i < num_cells; i++) {
            Row row;
            deserialize_row(leaf_node_value(node, i), &row);
            printf("%*s  - key %u: (%u, %s, %s)\n", indent, "",
                   *leaf_node_key(node, i), row.id, row.name, row.email);
        }
    } else {
        uint32_t num_keys = *internal_node_num_keys(node);
        printf("%*sInternal (page %u, keys %u)\n", indent, "", page_num, num_keys);
        for (uint32_t i = 0; i <= num_keys; i++) {
            uint32_t child = *internal_node_child(node, i);
            btree_print(pager, child, indent + 2);
            if (i < num_keys)
                printf("%*s  key %u = %u\n", indent, "", i,
                       *internal_node_key(node, i));
        }
    }
}
