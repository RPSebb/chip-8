#include <stdio.h>
#include <unistd.h>
#include "chip-8.c"
#include "../include/audio.h"
#include "../include/window.h"

#define WIDTH 640
#define HEIGHT 320
uint32_t screen_buffer[64 * 32];

int main(int argc, char* argv[]) {
    if(argc < 2) {
        printf("Usage: %s <path/to/rom.ch8>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if(!f) {
        printf("Erreur : impossible d'ouvrir la ROM.\n");
        return 1;
    }

    // 1. Calcul de la taille du fichier
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 2. Vérification de la taille max autorisée (3584 octets max)
    #define MAX_ROM_SIZE (4096 - 0x200)

    if(size > MAX_ROM_SIZE) {
        printf("Erreur : la ROM est trop grande pour la mémoire (%ld octets / %d max).\n", size, MAX_ROM_SIZE);
        fclose(f);
        return 1;
    }

    // 3. Lecture DIRECTE dans la RAM à partir de 0x200
    size_t bytes_read = fread(&ram[0x200], 1, size, f);
    fclose(f);

    if(bytes_read != (size_t)size) {
        printf("Erreur lors de la lecture du fichier ROM.\n");
        return 1;
    }

    program_counter = 0x200;
    memcpy(&ram[0x050], fontset, 80);

    window_create(0, 0, WIDTH, HEIGHT, "CHIP 8");
    window_set_on_key(process_event);

    if(!audio_init() || !audio_start()) {
        printf("Attention : audio indisponible.\n");
    }

    uint8_t run = 1;
    while(run) {
        run = window_process_events();

        for(int i = 0; i < 10; i++) {
            process_instruction();
        }

        if(dt > 0) { dt--; }
        if(st > 0) { st--; }
        audio_set_tone(st > 0);
        for(int y = 0; y < 32; y++) {
            for(int x = 0; x < 64; x++) {
                bool pixel_on = (pixels[y] >> (63 - x)) & 1;
                screen_buffer[y * 64 + x] = pixel_on ? 0xFFFFFFFF : 0xFF000000;
            }
        }
        window_render_from_texture(screen_buffer, 64, 32, 32);

        usleep(16'667);
    }

    audio_shutdown();
    window_destroy();
}
