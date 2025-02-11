#ifndef CONTAINER_H
#define CONTAINER_H

#include "node.h"

typedef struct container_t
{
    node_t* node;
    struct container_t* prev;
    struct container_t* next;
} container_t;

void container_append(container_t** container, node_t* node);
unsigned long long container_get_length(container_t** node);
_Bool container_remove(container_t** container, unsigned long long idx);
node_t* container_get(container_t** container, unsigned long long idx);
_Bool container_insert(container_t** container, node_t* node, int idx);
_Bool container_prepend(container_t** container, node_t* node);

#endif