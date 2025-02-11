#ifndef NODE_H
#define NODE_H

#include <krnl_stdlib.h>

typedef struct node_t
{
    void* payload;
    struct node_t* prev;
    struct node_t* next;
} node_t;

void node_append(node_t** node, void* payload);
unsigned long long node_get_length(node_t** node);
_Bool node_remove(node_t** node, unsigned long long idx);
void* node_get(node_t** node, unsigned long long idx);
_Bool node_insert(node_t** node, void* payload, int idx);
_Bool node_prepend(node_t** node, void* payload);

#endif
