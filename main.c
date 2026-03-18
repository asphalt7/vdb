#include "table.h"
#include "repl.h"

int main(int argc, char *argv[])
{
    const char *filename = "vdb.db";

    if (argc > 1)
        filename = argv[1];

    Table *table = table_open(filename);
    repl_run(table);

    return 0;
}
