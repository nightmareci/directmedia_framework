/*
 * MIT License
 * 
 * Copyright (c) 2023 Brandon McGriff <nightmareci@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "render/frames.h"
#include "util/queue.h"
#include "main/app.h"
#include "opengl/opengl.h"
#include "SDL.h"
#include <stdlib.h>
#include <assert.h>

typedef struct command_struct {
	command_funcs_struct funcs;
	void* state;
} command_struct;

typedef struct frame_struct {
	queue_struct* commands;
	bool ended;
} frame_struct;

typedef struct frames_struct {
	queue_struct* frame_queues;
	frame_struct* latest_frame;
} frames_struct;

frames_struct* frames_create() {
	frames_struct* const frames = calloc(1u, sizeof(frames_struct));
	if (frames == NULL) {
		return NULL;
	}

	frames->frame_queues = queue_create();
	if (frames->frame_queues == NULL) {
		free(frames);
		return NULL;
	}

	return frames;
}

bool frames_destroy(frames_struct* const frames) {
	assert(
		frames != NULL &&
		frames->frame_queues != NULL
	);

	for (
		frame_struct* frame = (frame_struct*)queue_dequeue(frames->frame_queues);
		frame != NULL;
		queue_destroy(frame->commands), free(frame), frame = (frame_struct*)queue_dequeue(frames->frame_queues)
	) {
		for (
			command_struct* command = (command_struct*)queue_dequeue(frame->commands);
			command != NULL;
			free(command), command = (command_struct*)queue_dequeue(frame->commands)
		) {
			if (command->funcs.update != NULL && !command->funcs.update(command->state)) {
				return false;
			}

			if (command->funcs.destroy != NULL) {
				command->funcs.destroy(command->state);
			}
		}
		glFlush();
	}

	return true;
}

frames_status_enum frames_start(frames_struct* const frames) {
	assert(
		frames != NULL &&
		frames->frame_queues != NULL
	);

	frame_struct* const next_frame = malloc(sizeof(frame_struct));
	if (next_frame == NULL) {
		return FRAMES_STATUS_ERROR;
	}

	next_frame->commands = queue_create();
	if (next_frame->commands == NULL) {
		free(next_frame);
		return FRAMES_STATUS_ERROR;
	}

	next_frame->ended = false;

	if (!queue_enqueue(frames->frame_queues, next_frame)) {
		queue_destroy(next_frame->commands);
		free(next_frame);
		return FRAMES_STATUS_ERROR;
	}
	
	frames->latest_frame = next_frame;
	
	return FRAMES_STATUS_SUCCESS;
}

void frames_end(frames_struct* const frames) {
	assert(frames->latest_frame != NULL);

	frames->latest_frame->ended = true;
}

bool frames_enqueue_command(
	frames_struct* const frames,
	const command_funcs_struct* const funcs,
	void* const state
) {
	assert(
		frames != NULL &&
		frames->latest_frame != NULL &&
		frames->latest_frame->commands != NULL &&
		funcs != NULL
	);

	queue_struct* const commands = frames->latest_frame->commands;

	command_struct* const command = malloc(sizeof(command_struct));
	if (command == NULL) {
		return false;
	}
	
	command->funcs = *funcs;
	command->state = state;
	
	if (!queue_enqueue(commands, command)) {
		free(command);
		return false;
	}

	return true;
}

frames_status_enum frames_draw_latest(frames_struct* const frames) {
	assert(
		frames != NULL &&
		frames->frame_queues != NULL &&
		frames->latest_frame != NULL
	);

	frame_struct* const latest_frame = frames->latest_frame;
	#ifndef NDEBUG
	bool latest_frame_found = false;
	#endif
	for (
		frame_struct* frame = (frame_struct*)queue_dequeue(frames->frame_queues);
		frame != NULL;
		queue_destroy(frame->commands), free(frame), frame = (frame_struct*)queue_dequeue(frames->frame_queues)
	) {
		#ifndef NDEBUG
		if (frame == latest_frame) {
			latest_frame_found = true;
		}
		#endif

		const bool draw_now = frame == latest_frame;
		for (
			command_struct* command = (command_struct*)queue_dequeue(frame->commands);
			command != NULL;
			free(command), command = (command_struct*)queue_dequeue(frame->commands)
		) {
			if (command->funcs.update != NULL) {
				if (!command->funcs.update(command->state)) {
					return FRAMES_STATUS_ERROR;
				}
			}

			if (draw_now && !command->funcs.draw(command->state)) {
				return FRAMES_STATUS_ERROR;
			}
			
			if (command->funcs.destroy != NULL) {
				command->funcs.destroy(command->state);
			}
		}

		// TODO: Profile with and without this here, to see what performance/memory usage looks like. But it might be the case that this should be here just to avoid too many operations queued in the GL at once.
		if (frame != latest_frame) {
			glFlush();
		}
	}

	assert(latest_frame_found);

	if (latest_frame->ended) {
		frames->latest_frame = NULL; // TODO: In the concurrent version, probably CAS here from the previously read value of latest_frame to NULL.
		SDL_Window* const window = app_window_get();
		assert(window != NULL);
		SDL_GL_SwapWindow(window);
		return FRAMES_STATUS_SUCCESS;
	}
	else {
		return FRAMES_STATUS_NO_END;
	}
}
