#include <node.h>

typedef int hfdj;

/*
node_t* node_create(void* payload)
{
    node_t* node = (node_t*)malloc(sizeof(node_t));

    node->payload = payload;
    node->next = (void*)0;
    node->prev = (void*)0;

    return node;
}

void node_append(node_t** node, void* payload)
{
    node_t* new_node;
    new_node = node_create(payload);

    if (*node)
    {
        node_t* curr_node = *node;

        while (curr_node->next != (void*)0)
        {
            curr_node = curr_node->next;
        }
        curr_node->next = new_node;
        new_node->prev = curr_node;
    }
    else
        *node = new_node;
}

unsigned long long node_get_length(node_t** node)
{
    if (*node)
    {
        node_t* curr_node = *node;

        unsigned long long length = 0;

        while (curr_node->next != (void*)0)
        {
            curr_node = curr_node->next;
            length++;
        }
        return (length + 1);
    }
    else
        return 0;
}

_Bool node_remove(node_t** node, unsigned long long idx)
{
    unsigned long long node_length = node_get_length(*(&node));

    if (node_length == 0 || idx >= node_length)
        return 0;

    node_t* curr_node = (*node);
    for (unsigned long long curr_idx = 0; (curr_idx < idx) && curr_node; curr_idx++)
        curr_node = curr_node->next;

    if (!curr_node)
        return 0;

    if (curr_node->prev != (void*)0)
        curr_node->prev->next = curr_node->next;

    if (curr_node->next)
        curr_node->next->prev = curr_node->prev;

    if (idx == 0)
        (*node) = curr_node->next;

    return 1;
}

void* node_get(node_t** node, unsigned long long idx)
{
    unsigned long long node_length = node_get_length(*(&node));

    if (node_length == 0 || idx >= node_length)
        return 0;

    node_t* curr_node = *node;

    for (unsigned long long i = 0; (i < idx) && curr_node; i++)
        curr_node = curr_node->next;

    return curr_node->payload;
}

Add a new node after an element in the list
_Bool node_insert(node_t** node, void* payload, unsigned long long idx) {
    node_t* curr_node = *node;

    node_t* new_node;
    if (!(new_node = (node_t*)malloc(sizeof(node_t))))
        return 0;

    for (unsigned long long curr_idx = 0; (curr_idx < idx) && curr_node->next; curr_idx++)  // 'curr_node->next' may cause problems
        curr_node = curr_node->next;

    new_node->payload = payload;
    new_node->next = curr_node->next;
    curr_node->next = new_node;

    return 1;
}

_Bool node_prepend(node_t** node, void* payload)
{
    1. allocate node
    node_t* new_node = (node_t*)malloc(sizeof(node_t));

    2. put in the data
    new_node->payload = payload;

    3. Make next of new node as head and previous as NULL
    new_node->next = (*node);
    new_node->prev = (void*)0;

    4. change prev of head node to new node
    if ((*node) != (void*)0)
        (*node)->prev = new_node;

    5. move the head to point to the new node
    (*node) = new_node;

    return 1;
}
*/