#include <init.h>
#include <panic.h>
#include <queue.h>
#include <mm/pmm.h>

typedef struct callback_obj {
  init_callback_t callback;
  void *data;
  LIST_ENTRY(struct callback_obj) list;
} callback_obj_t;

LOAD_SECTION(__static_init_array, ".init_array.static");

LIST_HEAD(callback_obj_t) init_address_space_cb_list;

void do_static_initializers() {
    // execute all of the static initializers
    if (__static_init_array.virt_addr == 0) {
      panic("no static initializers found");
    }
  
    void (**init_funcs)() = (void *) __static_init_array.virt_addr;
    size_t num_init_funcs = __static_init_array.size / sizeof(void *);
    for (size_t i = 0; i < num_init_funcs; i++) {
      init_funcs[i]();
    }
  }

  void register_init_address_space_callback(init_callback_t callback, void *data) {
    callback_obj_t *obj = kmallocz(sizeof(callback_obj_t));
    obj->callback = callback;
    obj->data = data;
    LIST_ENTRY_INIT(&obj->list);
    LIST_ADD(&init_address_space_cb_list, obj, list);
  }
