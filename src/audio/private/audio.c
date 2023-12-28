#include "audio/private/audio_private.h"
#include "main/app.h"
#include "data/data.h"
#include "util/log.h"
#include "SDL_thread.h"
#include "SDL_atomic.h"
#include <limits.h>
#include <assert.h>

#ifndef NDEBUG
static SDL_atomic_t inited_thread_set = { 0 };
static SDL_threadID inited_thread;
#endif

static data_cache_object* data_cache = NULL;
static bool opened_audio = false;
static int num_active_channels;
static int num_alloc_channels;

#ifdef NDEBUG
#define assert_inited_thread() ((void)0)
#else
static void assert_inited_thread() {
	assert(SDL_AtomicGet(&inited_thread_set));
	SDL_MemoryBarrierAcquire();
	assert(inited_thread == SDL_ThreadID());
}
#endif

bool audio_init() {
	log_printf("Initializing audio\n");

	assert(data_cache == NULL);
	assert(!opened_audio);

#ifndef NDEBUG
	assert(!SDL_AtomicGet(&inited_thread_set));
	SDL_MemoryBarrierAcquire();

	inited_thread = SDL_ThreadID();
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&inited_thread_set, 1);
#endif

	data_cache = data_cache_create(app_resource_path_get(), app_save_path_get());
	if (data_cache == NULL) {
		log_printf("Error creating data cache for audio\n");
		return false;
	}

	opened_audio = Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 2048) >= 0;
	if (!opened_audio) {
		log_printf("Error opening audio device: %s\n", Mix_GetError());
		return false;
	}
	num_active_channels = 0;
	num_alloc_channels = MIX_CHANNELS;

	log_printf("Successfully initialized audio\n");

	return true;
}

void audio_deinit() {
	assert_inited_thread();

	if (opened_audio) {
		Mix_CloseAudio();
		opened_audio = false;
	}

	if (data_cache != NULL) {
		data_cache_destroy(data_cache);
		data_cache = NULL;
	}

#ifndef NDEBUG
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&inited_thread_set, 0);
#endif
}

void audio_master_volume_set(const float volume) {
	assert(volume >= 0.0f);
	assert(volume <= 1.0f);

	Mix_MasterVolume(volume * MIX_MAX_VOLUME);
}

static bool alloc_channel() {
	assert(num_active_channels < INT_MAX);

	num_active_channels++;
	if (num_active_channels > num_alloc_channels) {
		if (Mix_AllocateChannels(num_active_channels) != num_active_channels) {
			log_printf("Error allocating an audio channel for playing music: %s\n", Mix_GetError());
			num_active_channels--;
			return false;
		}
		num_alloc_channels = num_active_channels;
	}

	return true;
}

bool audio_sound_play(const char* const sound_filename, const float volume) {
	assert_inited_thread();
	assert(data_cache != NULL);
	assert(opened_audio);
	assert(sound_filename != NULL);
	assert(volume >= 0.0f);
	assert(volume <= 1.0f);

	data_load_status status;
	const data_object* const sound = data_cache_get(data_cache, DATA_TYPE_SOUND, DATA_PATH_RESOURCE, sound_filename, &status, false);
	if (status != DATA_LOAD_STATUS_SUCCESS) {
		return false;
	}

	if (!alloc_channel()) {
		return false;
	}
	Mix_VolumeChunk(sound->sound, (int)(volume * MIX_MAX_VOLUME));
	return Mix_PlayChannel(-1, sound->sound, 0) >= 0;
}

void audio_sound_all_stop() {
	assert_inited_thread();
	assert(opened_audio);

	Mix_HaltGroup(-1);
	num_active_channels--;
}

void audio_music_volume_set(const float volume) {
	assert_inited_thread();
	assert(opened_audio);
	assert(volume >= 0.0f);
	assert(volume <= 1.0f);

	Mix_VolumeMusic((int)(volume * MIX_MAX_VOLUME));
}

bool audio_music_play(const char* const music_filename) {
	assert_inited_thread();
	assert(data_cache != NULL);
	assert(opened_audio);
	assert(music_filename != NULL);

	data_load_status status;
	const data_object* const music = data_cache_get(data_cache, DATA_TYPE_MUSIC, DATA_PATH_RESOURCE, music_filename, &status, false);
	if (status != DATA_LOAD_STATUS_SUCCESS) {
		return false;
	}

	if (!alloc_channel()) {
		return false;
	}
	return Mix_PlayMusic(music->music, -1) >= 0;
}

void audio_music_stop() {
	assert_inited_thread();
	assert(opened_audio);

	Mix_HaltMusic();
	num_active_channels--;
}
