#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

typedef void (*init_callback_t)(void *);

void do_static_initializers();

void register_init_address_space_callback(init_callback_t callback, void *data);

#endif
