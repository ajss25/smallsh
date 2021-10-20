#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// shell supports command line inputs of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

int main(void) {
	char command[MAX_COMMAND_LENGTH];
	char* args[MAX_COMMAND_ARGS];
	int argsCount = 0;
	int background = 0;
	pid_t pid = getpid();

	// prompt command line input, flush output buffer, get command line input
	printf(": ");
	fflush(stdout);
	fgets(command, MAX_COMMAND_LENGTH, stdin);

	int i = 0;
	char *token = strtok(command, " \n");

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
		token = strtok(NULL, " \n");
		i++;
		argsCount++;
	}

	// check if command needs to be executed in the background
	if (strcmp(args[argsCount - 1], "&") == 0) {
		background = 1;
	}
}
