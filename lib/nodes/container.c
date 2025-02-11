#include <container.h>

container_t* container_create(node_t* node)
{
    container_t* container;
    if (!(container = (container_t*)malloc(sizeof(container_t))))
        return (void*)0;

    container->node = node;
    container->prev = (void*)0;
    container->next = (void*)0;
    return container;
}

void container_append(container_t** container, node_t* node)
{
    container_t* new_container = container_create(node);

    if (*container)
    {
        container_t* curr_node = *container;

        while (curr_node->next != (void*)0)
            curr_node = curr_node->next;

        curr_node->next = new_container;
        new_container->prev = curr_node;
    }
    else
        *container = new_container;
}

// CONFIRMED as working
unsigned long long container_get_length(container_t** container)
{
    if (*container)
    {
        container_t* curr_container = *container;

        unsigned long long length = 0;

        while (curr_container->next != (void*)0)
        {
            curr_container = curr_container->next;
            length++;
        }
        return (length + 1);
    }
    else
        return 0;
}

node_t* container_get(container_t** container, unsigned long long idx)
{
    unsigned long long container_entries = container_get_length(*(&container));

    if (container_entries == 0 || idx >= container_entries)
        return (void*)0;

    container_t* curr_container = *container;

    for (unsigned long long i = 0; (i < idx) && curr_container; i++)
        curr_container = curr_container->next;

    return curr_container->node;
}

_Bool container_remove(container_t** container, unsigned long long idx)
{
    unsigned long long container_length = container_get_length(*(&container));

    if (container_length == 0 || idx >= container_length)
        return 0;

    node_t* node = (void*)0;

    container_t* curr_conatiner = (*container);
    for (unsigned long long curr_idx = 0; (curr_idx < idx) && curr_conatiner; curr_idx++)
        curr_conatiner = curr_conatiner->next;

    if (!curr_conatiner)
        return 0;

    node = curr_conatiner->node;

    if (curr_conatiner->prev != (void*)0)
        curr_conatiner->prev->next = curr_conatiner->next;

    if (curr_conatiner->next)
        curr_conatiner->next->prev = curr_conatiner->prev;

    if (idx == 0)
        (*container) = curr_conatiner->next;

    return 1;
}

_Bool container_prepend(container_t** container, node_t* node)
{
    container_t* new_container = (container_t*)malloc(sizeof(container_t));

    new_container->node = node;

    new_container->next = (*container);
    new_container->prev = (void*)0;

    if ((*container) != (void*)0)
        (*container)->prev = new_container;

    (*container) = new_container;

    return 1;
}

/* Add a new node after an element in the list */
_Bool container_insert(container_t** container, node_t* node, int idx) {
    container_t* curr_container = *container;

    container_t* new_container;
    if (!(new_container = (container_t*)malloc(sizeof(container_t))))
        return 0;

    for (unsigned long long curr_idx = 0; (curr_idx < idx) && curr_container->next; curr_idx++)  // 'curr_node->next' may cause problems
        curr_container = curr_container->next;

    new_container->node = node;
    new_container->next = curr_container->next;
    curr_container->next = new_container;
    return 1;
}
