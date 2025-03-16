#include <init.h>
#include <panic.h>

LOAD_SECTION(__static_init_array, ".init_array.static");

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