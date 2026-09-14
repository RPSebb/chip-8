// IA things
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

bool audio_init(void);
bool audio_start(void);
void audio_set_tone(bool on);
void audio_shutdown(void);

#endif
