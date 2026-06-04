// doomgeneric sound stubs for NuttyBadge (no SDL_mixer available on ESP32)

#include "config.h"
#include "doomfeatures.h"
#include "i_sound.h"
#include "i_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Sound module stubs
// ---------------------------------------------------------------------------

static boolean SoundStub_Init(boolean use_sfx_prefix)
{
    printf("NuttyBadge sound: no sound backend available (stub)\n");
    return false; // signal that sound init failed; DOOM will run without sound
}

static void SoundStub_Shutdown(void) { }
static int  SoundStub_GetSfxLumpNum(sfxinfo_t *sfxinfo) { return 0; }
static void SoundStub_Update(void) { }
static void SoundStub_UpdateSoundParams(int channel, int vol, int sep) { }
static int  SoundStub_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep) { return -1; }
static void SoundStub_StopSound(int channel) { }
static boolean SoundStub_SoundIsPlaying(int channel) { return false; }
static void SoundStub_CacheSounds(sfxinfo_t *sounds, int num_sounds) { }

static snddevice_t sound_stub_devices[] =
{
    SNDDEVICE_NONE,
};

sound_module_t DG_sound_module =
{
    .sound_devices    = sound_stub_devices,
    .num_sound_devices = 1,
    .Init             = SoundStub_Init,
    .Shutdown         = SoundStub_Shutdown,
    .GetSfxLumpNum    = SoundStub_GetSfxLumpNum,
    .Update           = SoundStub_Update,
    .UpdateSoundParams = SoundStub_UpdateSoundParams,
    .StartSound       = SoundStub_StartSound,
    .StopSound        = SoundStub_StopSound,
    .SoundIsPlaying   = SoundStub_SoundIsPlaying,
    .CacheSounds      = SoundStub_CacheSounds,
};

// ---------------------------------------------------------------------------
// Music module stubs
// ---------------------------------------------------------------------------

static boolean MusicStub_Init(void) { return false; }
static void    MusicStub_Shutdown(void) { }
static void    MusicStub_SetMusicVolume(int vol) { }
static void    MusicStub_PauseMusic(void) { }
static void    MusicStub_ResumeMusic(void) { }
static void   *MusicStub_RegisterSong(void *data, int len) { return NULL; }
static void    MusicStub_UnRegisterSong(void *handle) { }
static void    MusicStub_PlaySong(void *handle, boolean looping) { }
static void    MusicStub_StopSong(void) { }
static boolean MusicStub_MusicIsPlaying(void) { return false; }
static void    MusicStub_Poll(void) { }

static snddevice_t music_stub_devices[] =
{
    SNDDEVICE_NONE,
};

music_module_t DG_music_module =
{
    .sound_devices     = music_stub_devices,
    .num_sound_devices = 1,
    .Init              = MusicStub_Init,
    .Shutdown          = MusicStub_Shutdown,
    .SetMusicVolume    = MusicStub_SetMusicVolume,
    .PauseMusic        = MusicStub_PauseMusic,
    .ResumeMusic       = MusicStub_ResumeMusic,
    .RegisterSong      = MusicStub_RegisterSong,
    .UnRegisterSong    = MusicStub_UnRegisterSong,
    .PlaySong          = MusicStub_PlaySong,
    .StopSong          = MusicStub_StopSong,
    .MusicIsPlaying    = MusicStub_MusicIsPlaying,
    .Poll              = MusicStub_Poll,
};
