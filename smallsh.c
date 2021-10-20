#include <stdio.h>
#include <string.h>

// shell supports command line inputs of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

int main(void) {
	char command[MAX_COMMAND_LENGTH];
	char* args[MAX_COMMAND_ARGS];
	int argsCount = 0;
	int background = 0;

	// prompt command line input, flush output buffer, get command line input
	printf(": ");
	fflush(stdout);
	fgets(command, MAX_COMMAND_LENGTH, stdin);

	// parse command line input into separate arguments
	int i = 0;
	char *token = strtok(command, " \n");

	while (token) {
		args[i] = token;
		token = strtok(NULL, " \n");
		i++;
		argsCount++;
	}

	// check if command needs to be executed in the background
	if (strcmp(args[argsCount - 1], "&") == 0) {
		background = 1;
	}
}
