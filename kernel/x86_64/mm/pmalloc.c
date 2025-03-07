/*
__ref page_t *alloc_pages_size(size_t count, size_t page_size) {
    kassert(page_size == PGE_SIZE || page_size == PAGE_SIZE_2MB | page_size == PAGE_SIZE_1GB);
    zone_type_t zone_type = ZONE_ALLOC_DEFAULT;

    page_t *pages = NULL;

    page_t *pages = NULL;
    while (pages == NULL) {
        if (zone_type == MAX_ZONE_TYPE) {
            panic("out of memory");
        }

        // try all of the zones
        pages = alloc_pages_zone(zone_type, count, page_size);
        if (!pages)
            zone_type = zone_alloc_order[zone_type];
    }
    return pages;
}

__ref page_t *alloc_pages(size_t count) {
    return alloc_pages_size(count, PAGE_SIZE);
}
*/
