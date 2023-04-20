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

#include "framework/commands.h"
#include "framework/queue.h"
#include <stdlib.h>
#include <assert.h>

typedef struct command_struct {
	void* input;
	commands_canfail_func operation;
	commands_nofail_func destroy;
} command_struct;

static void commands_erase(commands_struct* const commands);

commands_struct* commands_create() {
	return (commands_struct*)queue_create();
}

bool commands_destroy(commands_struct* const commands) {
	const bool status = commands_empty(commands);
	queue_destroy((queue_struct*)commands);
	return status;
}

bool commands_enqueue(commands_struct* const commands, void* const input, commands_canfail_func const operation, commands_nofail_func const destroy) {
	assert(commands != NULL && operation != NULL);
    
    command_struct* const command = (command_struct*)malloc(sizeof(command_struct));
	if (command == NULL) {
		return false;
	}

	command->input = input;
	command->operation = operation;
	command->destroy = destroy;
	if (!queue_enqueue((queue_struct*)commands, command)) {
		free(command);
		return false;
	}
	else {
		return true;
	}
}

bool commands_flush(commands_struct* const commands) {
	queue_struct* const queue = (queue_struct*)commands;
	command_struct* command;

	while ((command = (command_struct*)queue_dequeue(queue)) != NULL) {
		if (command->operation != NULL && !command->operation(command->input)) {
			free(command);
			commands_erase(commands);
			return false;
		}
		if (command->destroy != NULL) {
			command->destroy(command->input);
		}
		free(command);
	}

	return true;
}

bool commands_empty(commands_struct* const commands) {
	queue_struct* const queue = (queue_struct*)commands;
	command_struct* command;
    
    while ((command = (command_struct*)queue_dequeue(queue)) != NULL) {
		if (command->destroy != NULL) {
			command->destroy(command->input);
		}
		free(command);
	}

	return true;
}

static void commands_erase(commands_struct* const commands) {
	queue_struct* const queue = (queue_struct*)commands;
	command_struct* command;
    while ((command = (command_struct*)queue_dequeue(queue)) != NULL) {
		free(command);
	}
}
