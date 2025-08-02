#include "../byteptr.h"
#include "../doomstat.h"
#include "../i_sound.h"
#include "../m_fixed.h"
#include "../s_sound.h"
#include "../w_wad.h"
#include "../z_zone.h"

#include <SDL.h>
#ifdef HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

#ifdef HAVE_SDL_MIXER
Mix_Music* music;
#endif

#ifdef HAVE_SDL_MIXER
CV_PossibleValue_t midibackend_cons_t[] = { {0,"Native"},{1,"FluidSynth"},{0,NULL} };
consvar_t cv_midibackend = {"midibackend", "Native", "Controls whether SDL2 plays audio using native playback or FluidSynth.", CAT_SOUND, CV_SAVE, midibackend_cons_t, NULL, 0, NULL, NULL};
consvar_t cv_soundfontpath = {"soundfontpath", "NOT CONFIGURED! Replace this with a path to a .sf2 file.", "Configures FluidSynth's soundfont path.", CAT_SOUND, CV_SAVE, NULL, NULL, 0, NULL, NULL};
#endif

byte sound_started = 0;

#ifdef HAVE_SDL_MIXER
//lazy copy paste from 2.1.25's SDL2 implementation
//i'm too stupid to implement this myself ok
static Mix_Chunk* ds2chunk(void* stream)
{
	unsigned short ver, freq;
	unsigned int samples, i, newsamples;
	char* sound;

	char* s;
	short* d;
	short o;
	fixed_t step, frac;

	short* stream2 = stream;

	// lump header
	ver = READUSHORT(stream2); // sound version format?
	if (ver != 3) // It should be 3 if it's a doomsound...
		return NULL; // onos! it's not a doomsound!
	freq = READUSHORT(stream2);
	samples = READULONG(stream2);

	// convert from signed 8bit ???hz to signed 16bit 44100hz.
	switch (freq)
	{
	case 44100:
		if (samples >= UINT32_MAX >> 2)
			return NULL; // would wrap, can't store.
		newsamples = samples;
		break;
	case 22050:
		if (samples >= UINT32_MAX >> 3)
			return NULL; // would wrap, can't store.
		newsamples = samples << 1;
		break;
	case 11025:
		if (samples >= UINT32_MAX >> 4)
			return NULL; // would wrap, can't store.
		newsamples = samples << 2;
		break;
	default:
		frac = (44100 << FRACBITS) / (int)freq;
		if (!(frac & 0xFFFF)) // other solid multiples (change if FRACBITS != 16)
			newsamples = samples * (frac >> FRACBITS);
		else // strange and unusual fractional frequency steps, plus anything higher than 44100hz.
			newsamples = FixedMul(FixedDiv(samples, freq), 44100) + 1; // add 1 to counter truncation.
		if (newsamples >= UINT32_MAX >> 2)
			return NULL; // would and/or did wrap, can't store.
		break;
	}
	sound = Z_Malloc(newsamples << 2, PU_SOUND, NULL); // samples * frequency shift * bytes per sample * channels

	s = (char*)stream2;
	d = (short*)sound;

	i = 0;
	switch (freq)
	{
	case 44100: // already at the same rate? well that makes it simple.
		while (i++ < samples)
		{
			o = ((short)(*s++) + 0x80) << 8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	case 22050: // unwrap 2x
		while (i++ < samples)
		{
			o = ((short)(*s++) + 0x80) << 8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	case 11025: // unwrap 4x
		while (i++ < samples)
		{
			o = ((short)(*s++) + 0x80) << 8; // changed signedness and shift up to 16 bits
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
			*d++ = o; // left channel
			*d++ = o; // right channel
		}
		break;
	default: // convert arbitrary hz to 44100.
		step = 0;
		frac = ((int)freq << FRACBITS) / 44100 + 1; //Add 1 to counter truncation.
		while (i < samples)
		{
			o = (short)(*s + 0x80) << 8; // changed signedness and shift up to 16 bits
			while (step < FRACUNIT) // this is as fast as I can make it.
			{
				*d++ = o; // left channel
				*d++ = o; // right channel
				step += frac;
			}
			do {
				i++; s++;
				step -= FRACUNIT;
			} while (step >= FRACUNIT);
		}
		break;
	}

	int finaloutput = (char*)d - sound;

	// return Mixer Chunk.
	return Mix_QuickLoad_RAW(sound, finaloutput);
}
#endif

void *I_GetSfx(sfxinfo_t *sfx)
{
#ifdef HAVE_SDL_MIXER
    byte*               dssfx;
    int                 size;

    if (sfx->lumpnum<0)
        sfx->lumpnum = S_GetSfxLumpNum (sfx);

    //CONS_Printf ("I_GetSfx(%d)\n", sfx->lumpnum);

    size = W_LumpLength (sfx->lumpnum);

    // PU_CACHE because the data is copied to the DIRECTSOUNDBUFFER, the one here will not be used
    dssfx = (byte*) W_CacheLumpNum (sfx->lumpnum, PU_SOUND);

    // return the LPDIRECTSOUNDBUFFER, which will be stored in S_sfx[].data
    return (void *)ds2chunk(dssfx);
#else
	sfx = NULL;
	return NULL;
#endif
}

void I_FreeSfx(sfxinfo_t *sfx)
{
	sfx = NULL;
}

void I_StartupSound(void){
#ifdef _WIN32
	SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
#endif

#ifdef HAVE_SDL_MIXER
	CV_RegisterVar(&cv_midibackend);
	CV_RegisterVar(&cv_soundfontpath);
#endif

#ifdef HAVE_SDL_MIXER
	Mix_AllocateChannels(cv_numChannels.value);
#endif
}

void I_ShutdownSound(void){
	CONS_Printf("I_ShutdownSound...\n");
#ifdef HAVE_SDL_MIXER
	Mix_CloseAudio();
	Mix_Quit();
#endif
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

//
//  SFX I/O
//

int I_StartSound(int id, int vol, int sep, int pitch, int priority, int channel)
{
#ifdef HAVE_SDL_MIXER
	if (!nosound) {
		//CONS_Printf("I_StartSound(): Playing sound id %d!\n", id);
		int handle = Mix_PlayChannel(channel, S_sfx[id].data, 0);
		Mix_Volume(channel, vol << 2);
		return handle;
	}
#endif
}

void I_StopSound(int handle)
{
#ifdef HAVE_SDL_MIXER
	if (!nosound) {
		Mix_HaltChannel(handle);
	}
#else
	handle = 0;
#endif
}

int I_SoundIsPlaying(int handle)
{
#ifdef HAVE_SDL_MIXER
	return Mix_Playing(handle);
#else
	handle = 0;
	return -1;
#endif
}

void I_SubmitSound() {}
void I_UpdateSound() {}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
	handle = vol = sep = pitch = 0;
}

void I_SetSfxVolume(int volume){}

//
//  MUSIC I/O
//
byte music_started = 0;

void I_InitMusic(void){
#ifdef HAVE_SDL_MIXER
	if (!nomusic) {
		SDL_setenv("SDL_MIXER_DEBUG_MIDI", "1", 1);
		SDL_Init(SDL_INIT_AUDIO);
		Mix_Init(MIX_INIT_MID);
		if (cv_midibackend.value == 0) {
			SDL_setenv("SDL_MIXER_MIDI_DRIVER", "native", 1);
		} else {
			SDL_setenv("SDL_MIXER_MIDI_DRIVER", "fluidsynth", 1);
			SDL_setenv("SDL_SOUNDFONTS", cv_soundfontpath.string, 1);
			Mix_SetSoundFonts(cv_soundfontpath.string);
		}
		Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
	}
#endif
}

void I_ShutdownMusic(void){
	CONS_Printf("I_ShutdownMusic...\n");
#ifdef HAVE_SDL_MIXER
	if (!nomusic)
		Mix_CloseAudio();
		Mix_FreeMusic(music);
#endif
}

void I_PauseSong(int handle)
{
	handle = 0;
}

void I_ResumeSong(int handle)
{
	handle = 0;
}

void I_SetMusicVolume(int volume)
{
#ifdef HAVE_SDL_MIXER
	Mix_VolumeMusic(volume << 2);
#endif
}

int I_RegisterSong(void *data, int len)
{
#ifdef HAVE_SDL_MIXER
	if (!nomusic) {
		SDL_RWops* rw = SDL_RWFromConstMem(data, len);
		music = Mix_LoadMUSType_RW(rw, MUS_MID, 1);
	}
#else
	data = NULL;
	len = 0;
	return -1;
#endif
}

void I_PlaySong(int handle, int looping)
{
#ifdef HAVE_SDL_MIXER
	if (!nomusic) {
		Mix_PlayMusic(music, -1);
	}
#else
	handle = 0;
	looping = 0;
#endif
}

void I_StopSong(int handle)
{
	handle = 0;
}

void I_UnRegisterSong(int handle)
{
	handle = 0;
}