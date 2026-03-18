#include "pager.h"

Pager *pager_open(const char *filename)
{
    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        fprintf(stderr, "error: unable to open file '%s': %s\n",
                filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);

    Pager *pager = calloc(1, sizeof(Pager));
    pager->fd = fd;
    pager->file_length = (uint32_t)file_length;
    pager->num_pages = file_length / PAGE_SIZE;

    if (file_length % PAGE_SIZE != 0) {
        fprintf(stderr, "error: db file is not a whole number of pages (%u bytes)\n",
                (unsigned)file_length);
        exit(EXIT_FAILURE);
    }

    return pager;
}

void *pager_get_page(Pager *pager, uint32_t page_num)
{
    if (page_num >= MAX_PAGES) {
        fprintf(stderr, "error: page number %u out of bounds (max %d)\n",
                page_num, MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        /* Cache miss -- allocate and maybe read from file */
        void *page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);

        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (page_num < num_pages) {
            lseek(pager->fd, (off_t)page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->fd, page, PAGE_SIZE);
            if (bytes_read < 0) {
                fprintf(stderr, "error: reading page %u: %s\n",
                        page_num, strerror(errno));
                free(page);
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;

        if (page_num >= pager->num_pages)
            pager->num_pages = page_num + 1;
    }

    return pager->pages[page_num];
}

uint32_t pager_get_unused_page(Pager *pager)
{
    return pager->num_pages;
}

void pager_flush(Pager *pager, uint32_t page_num)
{
    if (pager->pages[page_num] == NULL) return;

    off_t offset = lseek(pager->fd, (off_t)page_num * PAGE_SIZE, SEEK_SET);
    if (offset < 0) {
        fprintf(stderr, "error: seeking to page %u: %s\n",
                page_num, strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t written = write(pager->fd, pager->pages[page_num], PAGE_SIZE);
    if (written < 0) {
        fprintf(stderr, "error: writing page %u: %s\n",
                page_num, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void pager_close(Pager *pager)
{
    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] != NULL) {
            pager_flush(pager, i);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }
    }

    close(pager->fd);
    free(pager);
}
