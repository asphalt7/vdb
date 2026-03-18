#include "repl.h"
#include <ctype.h>

/* ── Input buffer ────────────────────────────────────────────────────── */

typedef struct {
    char   *buffer;
    size_t  buffer_length;
    ssize_t input_length;
} InputBuffer;

static InputBuffer *new_input_buffer(void)
{
    InputBuffer *ib = malloc(sizeof(InputBuffer));
    ib->buffer = NULL;
    ib->buffer_length = 0;
    ib->input_length = 0;
    return ib;
}

static void free_input_buffer(InputBuffer *ib)
{
    free(ib->buffer);
    free(ib);
}

static void read_input(InputBuffer *ib)
{
    ssize_t bytes_read = getline(&ib->buffer, &ib->buffer_length, stdin);

    if (bytes_read <= 0) {
        printf("\n");
        exit(EXIT_SUCCESS);
    }

    /* Strip trailing newline */
    ib->input_length = bytes_read - 1;
    ib->buffer[bytes_read - 1] = '\0';
}

/* ── Statement types ─────────────────────────────────────────────────── */

typedef enum {
    STMT_INSERT,
    STMT_SELECT,
    STMT_SELECT_ALL,
    STMT_DELETE,
    STMT_COUNT,
    STMT_UPDATE,
} StatementType;

typedef struct {
    StatementType type;
    Row           row;      /* for INSERT / UPDATE */
    uint32_t      id;       /* for SELECT id / DELETE id */
} Statement;

/* ── Meta-commands (.exit, .btree, .help) ────────────────────────────── */

static int exec_meta_command(InputBuffer *ib, Table *table)
{
    if (strcmp(ib->buffer, ".exit") == 0) {
        table_close(table);
        free_input_buffer(ib);
        printf("Bye.\n");
        exit(EXIT_SUCCESS);
    }
    if (strcmp(ib->buffer, ".btree") == 0) {
        printf("B+Tree structure:\n");
        table_print_tree(table);
        return 1;
    }
    if (strcmp(ib->buffer, ".constants") == 0) {
        printf("Constants:\n");
        printf("  ROW_SIZE:                %zu\n", (size_t)ROW_SIZE);
        printf("  LEAF_NODE_MAX_CELLS:     %zu\n", (size_t)LEAF_NODE_MAX_CELLS);
        printf("  LEAF_NODE_CELL_SIZE:     %zu\n", (size_t)LEAF_NODE_CELL_SIZE);
        printf("  LEAF_NODE_SPACE:         %zu\n", (size_t)LEAF_NODE_SPACE_FOR_CELLS);
        printf("  LEAF_NODE_HEADER_SIZE:   %zu\n", (size_t)LEAF_NODE_HEADER_SIZE);
        printf("  INTERNAL_NODE_MAX_KEYS:  %zu\n", (size_t)INTERNAL_NODE_MAX_KEYS);
        printf("  PAGE_SIZE:               %d\n", PAGE_SIZE);
        return 1;
    }
    if (strcmp(ib->buffer, ".help") == 0) {
        printf("vdb - a mini database\n\n");
        printf("SQL commands:\n");
        printf("  insert <id> <name> <email>   Insert a row\n");
        printf("  select                       Select all rows\n");
        printf("  select <id>                  Select a row by id\n");
        printf("  delete <id>                  Delete a row by id\n");
        printf("  update <id> <name> <email>   Update a row by id\n");
        printf("  count                        Count all rows\n");
        printf("\nMeta-commands:\n");
        printf("  .exit        Close the database and exit\n");
        printf("  .btree       Print B+Tree structure\n");
        printf("  .constants   Print storage constants\n");
        printf("  .help        Show this help message\n");
        return 1;
    }
    fprintf(stderr, "error: unrecognized command '%s'\n", ib->buffer);
    return -1;
}

/* ── Parse statement ─────────────────────────────────────────────────── */

static int parse_statement(InputBuffer *ib, Statement *stmt)
{
    if (strncmp(ib->buffer, "insert", 6) == 0) {
        stmt->type = STMT_INSERT;
        char name_buf[256] = {0};
        char email_buf[256] = {0};
        int args = sscanf(ib->buffer, "insert %u %255s %255s",
                          &stmt->row.id, name_buf, email_buf);
        if (args < 3) {
            fprintf(stderr, "syntax error: insert <id> <name> <email>\n");
            return -1;
        }
        if (strlen(name_buf) > COL_NAME_SIZE) {
            fprintf(stderr, "error: name too long (max %d chars)\n", COL_NAME_SIZE);
            return -1;
        }
        if (strlen(email_buf) > COL_EMAIL_SIZE) {
            fprintf(stderr, "error: email too long (max %d chars)\n", COL_EMAIL_SIZE);
            return -1;
        }
        strncpy(stmt->row.name, name_buf, COL_NAME_SIZE);
        stmt->row.name[COL_NAME_SIZE] = '\0';
        strncpy(stmt->row.email, email_buf, COL_EMAIL_SIZE);
        stmt->row.email[COL_EMAIL_SIZE] = '\0';
        return 0;
    }

    if (strncmp(ib->buffer, "select", 6) == 0) {
        /* "select" (all) or "select <id>" */
        char *rest = ib->buffer + 6;
        while (*rest == ' ') rest++;
        if (*rest == '\0') {
            stmt->type = STMT_SELECT_ALL;
        } else {
            stmt->type = STMT_SELECT;
            stmt->id = (uint32_t)atoi(rest);
        }
        return 0;
    }

    if (strncmp(ib->buffer, "delete", 6) == 0) {
        stmt->type = STMT_DELETE;
        char *rest = ib->buffer + 6;
        while (*rest == ' ') rest++;
        if (*rest == '\0') {
            fprintf(stderr, "syntax error: delete <id>\n");
            return -1;
        }
        stmt->id = (uint32_t)atoi(rest);
        return 0;
    }

    if (strncmp(ib->buffer, "update", 6) == 0) {
        stmt->type = STMT_UPDATE;
        char name_buf[256] = {0};
        char email_buf[256] = {0};
        int args = sscanf(ib->buffer, "update %u %255s %255s",
                          &stmt->row.id, name_buf, email_buf);
        if (args < 3) {
            fprintf(stderr, "syntax error: update <id> <name> <email>\n");
            return -1;
        }
        strncpy(stmt->row.name, name_buf, COL_NAME_SIZE);
        stmt->row.name[COL_NAME_SIZE] = '\0';
        strncpy(stmt->row.email, email_buf, COL_EMAIL_SIZE);
        stmt->row.email[COL_EMAIL_SIZE] = '\0';
        return 0;
    }

    if (strncmp(ib->buffer, "count", 5) == 0) {
        stmt->type = STMT_COUNT;
        return 0;
    }

    fprintf(stderr, "error: unrecognized statement '%s'\n", ib->buffer);
    return -1;
}

/* ── Execute statement ───────────────────────────────────────────────── */

static void count_cb(Row *row, void *ctx)
{
    (void)row;
    (*(uint32_t *)ctx)++;
}

static void print_row(Row *row, void *ctx)
{
    (void)ctx;
    printf("(%u, %s, %s)\n", row->id, row->name, row->email);
}

static void exec_statement(Statement *stmt, Table *table)
{
    switch (stmt->type) {
    case STMT_INSERT:
        if (table_insert(table, &stmt->row) == 0)
            printf("Inserted.\n");
        break;

    case STMT_SELECT: {
        Row row;
        if (table_select(table, stmt->id, &row) == 0)
            printf("(%u, %s, %s)\n", row.id, row.name, row.email);
        else
            printf("Row with id %u not found.\n", stmt->id);
        break;
    }

    case STMT_SELECT_ALL:
        table_select_all(table, print_row, NULL);
        break;

    case STMT_DELETE:
        if (table_delete(table, stmt->id) == 0)
            printf("Deleted.\n");
        else
            printf("Row with id %u not found.\n", stmt->id);
        break;

    case STMT_UPDATE: {
        /* Delete + re-insert */
        if (table_delete(table, stmt->row.id) < 0) {
            printf("Row with id %u not found.\n", stmt->row.id);
            break;
        }
        table_insert(table, &stmt->row);
        printf("Updated.\n");
        break;
    }

    case STMT_COUNT: {
        uint32_t count = 0;
        table_select_all(table, count_cb, &count);
        printf("%u row(s).\n", count);
        break;
    }
    }
}

/* ── REPL loop ───────────────────────────────────────────────────────── */

void repl_run(Table *table)
{
    InputBuffer *ib = new_input_buffer();

    printf("vdb - a mini database (type '.help' for commands)\n");

    while (1) {
        printf("vdb> ");
        fflush(stdout);
        read_input(ib);

        if (ib->input_length == 0) continue;

        /* Meta-commands start with '.' */
        if (ib->buffer[0] == '.') {
            exec_meta_command(ib, table);
            continue;
        }

        Statement stmt;
        if (parse_statement(ib, &stmt) == 0)
            exec_statement(&stmt, table);
    }
}
