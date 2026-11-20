/* Silent sound interface for the Loopy build. */

#include "i_sound.h"
#include "s_sound.h"

void I_InitSound(void) {}
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_ShutdownSound(void) {}
void I_SetChannels(void) {}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}

int I_GetSfxLumpNum(sfxinfo_t *sound)
{
    (void)sound;
    return -1;
}

int I_StartSound(int id, int volume, int separation, int pitch, int priority)
{
    (void)id;
    (void)volume;
    (void)separation;
    (void)pitch;
    (void)priority;
    return -1;
}

void I_StopSound(int handle)
{
    (void)handle;
}

int I_SoundIsPlaying(int handle)
{
    (void)handle;
    return 0;
}

void I_UpdateSoundParams(int handle, int volume, int separation, int pitch)
{
    (void)handle;
    (void)volume;
    (void)separation;
    (void)pitch;
}

void I_SetMusicVolume(int volume)
{
    (void)volume;
}

void I_PauseSong(int handle)
{
    (void)handle;
}

void I_ResumeSong(int handle)
{
    (void)handle;
}

int I_RegisterSong(void *data)
{
    (void)data;
    return -1;
}

void I_PlaySong(int handle, int looping)
{
    (void)handle;
    (void)looping;
}

void I_StopSong(int handle)
{
    (void)handle;
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

void S_Init(int sfx_volume, int music_volume)
{
    (void)sfx_volume;
    (void)music_volume;
}

void S_Start(void) {}
void S_StopMusic(void) {}
void S_PauseSound(void) {}
void S_ResumeSound(void) {}

void S_StartSound(void *origin, int sound_id)
{
    (void)origin;
    (void)sound_id;
}

void S_StartSoundAtVolume(void *origin, int sound_id, int volume)
{
    (void)origin;
    (void)sound_id;
    (void)volume;
}

void S_StopSound(void *origin)
{
    (void)origin;
}

void S_StartMusic(int music_id)
{
    (void)music_id;
}

void S_ChangeMusic(int music_id, int looping)
{
    (void)music_id;
    (void)looping;
}

void S_UpdateSounds(void *listener)
{
    (void)listener;
}

void S_SetMusicVolume(int volume)
{
    (void)volume;
}

void S_SetSfxVolume(int volume)
{
    (void)volume;
}
