#include <printf.h>
#include <panic.h>
#include <8250.h>
#include <stdint.h>
#include <fmt.h>
#include <log.h>
#include <render.h>
#include <workqueue.h>
#include <thread.h>

#define BUFFER_SIZE 512

static void *impl_arg;
static int (*kprintf_puts_impl)(void *, const char *);

static struct early_kprintf {
  // mtx_t lock;
  uint16_t port;
} early_kprintf = {
  .port = COM1_PORT,
};

extern __used uint32_t *framebuf_base;

static int early_kprintf_puts(void *arg, const char *s) {
  struct early_kprintf *p = arg;
  while (*s) {
    serial_port_write_char(p->port, *s);
    s++;
  }
  return 0;
}

static void printf_process_workqueue(void *arg) {
  (void)arg; // Unused parameter.
  while (1) {
    if (framebuf_base != NULL) {
      process_workqueue();
      thread_yield();
    }
  }
}

void kprintf_early_init() {
  serial_port_init(early_kprintf.port);
  impl_arg = &early_kprintf;
  kprintf_puts_impl = early_kprintf_puts;
}

void kprintf_init() {
  render_init();
  create_kernel_thread(printf_process_workqueue, 0, "process workqueue");
}

void kprintf_kputs(const char *str) {
  kprintf_puts_impl(impl_arg, str);
}

void kprintf(const char *format, ...) {
  char str[BUFFER_SIZE];
  va_list valist;
  va_start(valist, format);
  fmt_format(format, str, BUFFER_SIZE, FMT_MAX_ARGS, valist);
  va_end(valist);
  kprintf_puts_impl(impl_arg, str);
  // Removed call to log_print_str() to avoid scheduling a possibly uninitialized work item.
  if (framebuf_base != NULL) {
    log_print_str(str);
    render_deferred();
  }
}

void kvfprintf(const char *format, va_list valist) {
  char str[BUFFER_SIZE];
  fmt_format(format, str, BUFFER_SIZE, FMT_MAX_ARGS, valist);
  kprintf_puts_impl(impl_arg, str);
}

int ksnprintf(char *str, size_t size, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = fmt_format(format, str, size, FMT_MAX_ARGS, args);
  va_end(args);
  return ret;
}