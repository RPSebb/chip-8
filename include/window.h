#ifndef WINDOW_H
#define WINDOW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void window_create(int pos_x, int pos_y, int width, int height, const char* title);
void window_render_from_texture(const void* pixels, size_t width, size_t height, int bpp);
uint8_t window_process_events(void);
void window_destroy(void);

typedef void (*window_key_callback_t)(uint8_t scancode, uint8_t is_pressed);
extern window_key_callback_t window_on_key_event;
void window_set_on_key(window_key_callback_t callback);
#endif
