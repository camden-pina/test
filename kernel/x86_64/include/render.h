#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

// Initialize the deferred rendering subsystem.
void render_init(void);

// Trigger a deferred console rendering update.
void render_deferred(void);

// This function may be called by a timer interrupt or main loop to process pending rendering work.
void render_tick(void);

#endif
