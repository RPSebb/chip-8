#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// Initialise WASAPI (COM, device, client audio, format).
// Retourne true si succès. A appeler une seule fois, avant audio_start().
bool audio_init(void);

// Lance le thread de rendu audio en arrière-plan (non bloquant).
// A appeler une seule fois, après audio_init().
bool audio_start(void);

// Active/désactive le son (onde carrée continue).
// Thread-safe, non bloquant : écrit juste un flag atomique.
// A appeler depuis ta boucle CHIP-8 à chaque frame en fonction de st.
void audio_set_tone(bool on);

// Arrête proprement le thread audio et libère les ressources COM.
// A appeler une seule fois, à la fermeture du programme.
void audio_shutdown(void);

#endif // AUDIO_H
