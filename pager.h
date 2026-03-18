#ifndef PAGER_H
#define PAGER_H

#include "vdb.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    int      fd;
    uint32_t file_length;
    uint32_t num_pages;
    void    *pages[MAX_PAGES];
} Pager;

/* Open/create database file, return Pager. */
Pager   *pager_open(const char *filename);

/* Get a page by number. Reads from disk on first access. */
void    *pager_get_page(Pager *pager, uint32_t page_num);

/* Allocate a new page (returns its page number). */
uint32_t pager_get_unused_page(Pager *pager);

/* Flush a page to disk. */
void     pager_flush(Pager *pager, uint32_t page_num);

/* Flush all pages and close. */
void     pager_close(Pager *pager);

#endif /* PAGER_H */
