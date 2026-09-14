#define COBJMACROS   // permet d'utiliser les macros C (IUnknown_Release, etc.) sur les interfaces COM
#define _USE_MATH_DEFINES
#define INITGUID
#include <stdio.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include "../include/audio.h"

// ---- Config de l'onde ----
#define TONE_FREQ_HZ   440.0   // Mi — un poil plus grave, moins strident qu'un 440Hz carré
#define AMPLITUDE      0.25    // volume objectif, indépendant du mixer système

// ---- État global du module ----
static IAudioClient*        g_audio_client = NULL;
static IAudioRenderClient*  g_render_client = NULL;
static HANDLE               g_audio_event = NULL;
static HANDLE               g_thread = NULL;
static volatile LONG        g_running = 0;   // 0/1, contrôle l'arrêt du thread
static volatile LONG        g_tone_on = 0;   // 0/1, lu par le thread, écrit par le moteur CHIP-8

static WAVEFORMATEX         g_format = {0};
static double               g_phase = 0.0;   // phase courante de l'onde, en continu entre les buffers
static float                g_envelope = 0.0f; // fondu doux 0..1, évite les clics on/off

static DWORD WINAPI audio_render_thread(LPVOID param) {
    (void)param;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    UINT32 buffer_frame_count = 0;
    hr = IAudioClient_GetBufferSize(g_audio_client, &buffer_frame_count);
    if (FAILED(hr)) {
        CoUninitialize();
        return 1;
    }

    // Démarre le flux audio
    hr = IAudioClient_Start(g_audio_client);
    if (FAILED(hr)) {
        printf("audio_render_thread: IAudioClient_Start failed, hr=0x%08lx\n", hr);
        CoUninitialize();
        return 1;
    }
    printf("audio_render_thread: démarré, buffer_frame_count=%u, nChannels=%u, nSamplesPerSec=%lu\n",
           buffer_frame_count, g_format.nChannels, g_format.nSamplesPerSec);

    while (InterlockedCompareExchange(&g_running, 0, 0)) {
        // Attend que WASAPI ait besoin de nouvelles données (event-driven, pas de polling actif)
        DWORD wait_result = WaitForSingleObject(g_audio_event, 2000);
        if (wait_result != WAIT_OBJECT_0) {
            continue; // timeout ou erreur, on re-vérifie g_running et on continue
        }

        UINT32 padding_frames = 0;
        hr = IAudioClient_GetCurrentPadding(g_audio_client, &padding_frames);
        if (FAILED(hr)) { break; }

        UINT32 frames_available = buffer_frame_count - padding_frames;
        if (frames_available == 0) { continue; }

        BYTE* data = NULL;
        hr = IAudioRenderClient_GetBuffer(g_render_client, frames_available, &data);
        if (FAILED(hr)) {
            printf("audio_render_thread: GetBuffer failed, hr=0x%08lx\n", hr);
            break;
        }

        bool tone_on = InterlockedCompareExchange(&g_tone_on, 0, 0) != 0;
        static bool last_tone_on = false;
        if (tone_on != last_tone_on) {
            printf("audio_render_thread: tone_on = %d\n", tone_on);
            last_tone_on = tone_on;
        }
        float* samples = (float*)data; // g_format est configuré en IEEE float, voir audio_init()

        double phase_inc = 2.0 * M_PI * TONE_FREQ_HZ / g_format.nSamplesPerSec;

        for (UINT32 i = 0; i < frames_available; i++) {

            float target = tone_on ? 1.0f : 0.0f;
            float envelope_step = 1.0f / ((float)g_format.nSamplesPerSec / 200.0f); // ~5ms de fondu
            if (g_envelope < target) { g_envelope = fminf(g_envelope + envelope_step, target); }
            else if (g_envelope > target) { g_envelope = fmaxf(g_envelope - envelope_step, target); }

            float value = (float)AMPLITUDE * g_envelope * sinf((float)g_phase);
            g_phase += phase_inc;
            if (g_phase >= 2.0 * M_PI) { g_phase -= 2.0 * M_PI; }

            for (WORD ch = 0; ch < g_format.nChannels; ch++) {
                *samples++ = value;
            }
        }

        IAudioRenderClient_ReleaseBuffer(g_render_client, frames_available, 0);
    }

    IAudioClient_Stop(g_audio_client);
    CoUninitialize();
    return 0;
}

bool audio_init(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        printf("audio_init: CoInitializeEx failed, hr=0x%08lx\n", hr);
        return false;
    }

    IMMDeviceEnumerator* enumerator = NULL;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                           &IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr)) {
        printf("audio_init: CoCreateInstance failed, hr=0x%08lx\n", hr);
        return false;
    }

    IMMDevice* device = NULL;
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
    IUnknown_Release(enumerator);
    if (FAILED(hr)) {
        printf("audio_init: GetDefaultAudioEndpoint failed, hr=0x%08lx\n", hr);
        return false;
    }

    hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&g_audio_client);
    IUnknown_Release(device);
    if (FAILED(hr)) {
        printf("audio_init: IMMDevice_Activate failed, hr=0x%08lx\n", hr);
        return false;
    }

    WAVEFORMATEX* mix_format = NULL;
    hr = IAudioClient_GetMixFormat(g_audio_client, &mix_format);
    if (FAILED(hr) || !mix_format) {
        printf("audio_init: GetMixFormat failed, hr=0x%08lx\n", hr);
        return false;
    }

    if (mix_format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
        mix_format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
        printf("audio_init: format inattendu (wFormatTag=%u), abandon.\n", mix_format->wFormatTag);
        CoTaskMemFree(mix_format);
        return false;
    }

    REFERENCE_TIME buffer_duration = 20 * 10000; // 20ms, en unités de 100ns (WASAPI)

    hr = IAudioClient_Initialize(
        g_audio_client,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0, // secondary buffer, doit être 0 en mode shared
        mix_format,   // <-- on passe le pointeur original complet, pas une copie tronquée
        NULL
    );

    g_format = *mix_format;
    CoTaskMemFree(mix_format);

    if (FAILED(hr)) {
        printf("audio_init: IAudioClient_Initialize failed, hr=0x%08lx\n", hr);
        return false;
    }

    g_audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_audio_event) {
        printf("audio_init: CreateEvent failed, GetLastError=%lu\n", GetLastError());
        return false;
    }

    hr = IAudioClient_SetEventHandle(g_audio_client, g_audio_event);
    if (FAILED(hr)) {
        printf("audio_init: SetEventHandle failed, hr=0x%08lx\n", hr);
        return false;
    }

    hr = IAudioClient_GetService(g_audio_client, &IID_IAudioRenderClient, (void**)&g_render_client);
    if (FAILED(hr)) {
        printf("audio_init: GetService(IAudioRenderClient) failed, hr=0x%08lx\n", hr);
        return false;
    }

    return true;
}

bool audio_start(void) {
    if (!g_audio_client || !g_render_client) { return false; }

    InterlockedExchange(&g_running, 1);
    g_thread = CreateThread(NULL, 0, audio_render_thread, NULL, 0, NULL);
    return g_thread != NULL;
}

void audio_set_tone(bool on) {
    InterlockedExchange(&g_tone_on, on ? 1 : 0);
}

void audio_shutdown(void) {
    InterlockedExchange(&g_running, 0);
    if (g_audio_event) { SetEvent(g_audio_event); }

    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }

    if (g_render_client) { IUnknown_Release(g_render_client); g_render_client = NULL; }
    if (g_audio_client)  { IUnknown_Release(g_audio_client);  g_audio_client = NULL; }
    if (g_audio_event)   { CloseHandle(g_audio_event); g_audio_event = NULL; }

    CoUninitialize();
}
