#ifndef WINDOW_H
#define WINDOW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void window_create(int pos_x, int pos_y, int width, int height, const char* title);
void window_render_from_texture(const void* pixels, size_t width, size_t height, int bpp);
uint8_t window_process_events(void);
void window_destroy(void);

// Définition du type de fonction attendu
typedef void (*window_key_callback_t)(uint8_t scancode, uint8_t is_pressed);

// La variable globale que tu peux assigner depuis le main
extern window_key_callback_t window_on_key_event;

// Ou une fonction setter (souvent plus propre)
void window_set_on_key(window_key_callback_t callback);
#endif
