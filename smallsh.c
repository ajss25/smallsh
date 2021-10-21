#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

// shell supports command line inputs of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

// function to kill process and jobs, and exit the shell
void exitShell(void) {
	// need to further implement killing of all processes/jobs
	exit(0);
}

// function to get and parse command line input from user
int getAndParseCommand(void) {
	char command[MAX_COMMAND_LENGTH];
	char* args[MAX_COMMAND_ARGS];
	int argsCount = 0;
	int background = 0;
	pid_t pid = getpid();

	// prompt command line input, flush output buffer, get command line input
	printf(": ");
	fflush(stdout);
	fgets(command, MAX_COMMAND_LENGTH, stdin);

	// rid of the newline characater from the command line input
	strtok(command, "\n");

	int i = 0;
	char *token = strtok(command, " ");

	// parse command line input into separate arguments
	while (token) {
		// replace substring of "$$" at the end of any argument with pid of the shell
		// reference: https://stackoverflow.com/questions/12784766/check-substring-exists-in-a-string-in-c
		if (strstr(token, "$$") != NULL) {
			int j;
			for (j = 0; j < strlen(token); j++) {
				if (token[j] == '$' && token[j+1] == '$') {
					// terminate token early to rid of "$$" substring
					// reference: https://stackoverflow.com/questions/20342559/how-to-cut-part-of-a-string-in-c
					token[j] = '\0';

					// create a new argument string
					char newArg[sizeof(token) - 2 + sizeof(pid)];
					sprintf(newArg, "%s%d", token, pid);
					args[i] = newArg;
				}
			}
		} else {
			args[i] = token;
		}
		token = strtok(NULL, " ");
		i++;
		argsCount++;
	}

	// check if command needs to be executed in the background
	if (strcmp(args[argsCount - 1], "&") == 0) {
		background = 1;
	}

	// return to re-prompt for another command if input is a comment or a blank line
	if (args[0][0] == '#' || args[0][0] == '\n') {
		return 0;
	// if exit command was given, kill processes/jobs and exit the shell
	} else if (strcmp(args[0], "exit") == 0) {
		exitShell();
	}

	// print functions for debugging
	// int x;
	// for (x = 0; x < argsCount; x++) {
	// 	printf("%s\n", args[x]);
	// }

	// printf("%d\n", background);
	return 0;
}

int main(void) {
	// prompt user for command line input until exited
	while (1) {
		getAndParseCommand();
	}
	return 0;
}
