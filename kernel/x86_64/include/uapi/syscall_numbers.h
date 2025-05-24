#pragma once
/*  ──────────────────────────────────────────────────────────
 *  Linux‑compatible numbers (subset) so you can later reuse
 *  glibc/musl headers if you wish.
 *  ────────────────────────────────────────────────────────── */
#define __NR_read      0
#define __NR_write     1
#define __NR_brk       12
#define __NR_exit      60
#define __NR_max       61     /* keep one past the last */
