#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

// shell supports command line inputs of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

// initialize global variables
int status = 0;
int background = 0;
int childProcessCount = 0;
int childProcessPids[80];

// declare implemented functions at the top
void printStatus();

// function to check status of child processes
void checkChildProcesses(void) {
	int childExitMethod = -5;
	int exitStatus;
	int i;
	
	// loop through child process pids to check if finished running
	for (i = 0; i < childProcessCount; i++) {
		if (waitpid(childProcessPids[i], &childExitMethod, WNOHANG)) {
				// if finished, set status accordingly
				if (WIFEXITED(childExitMethod)) {
					exitStatus = WEXITSTATUS(childExitMethod);
				} else {
					exitStatus = WTERMSIG(childExitMethod);
				// print results
				printf("background pid %d is done: ", childProcessPids[i]);
				printStatus(exitStatus);
				fflush(stdout);
			}
		}
	}
}

// function to execute commands other than `exit`, `cd`, and `status`
void executeOtherCommands(char** args, int argsCount) {
	// if the command is supposed to run on the foreground
	if (background == 0) {
		// set last argument to NULL for execvp()
		args[argsCount] = NULL;

		// initialize with bogus values
		pid_t spawnPid = -5; 
		int childExitMethod = -5;

		// fork a child process
		spawnPid = fork();

		switch (spawnPid) {
			// error
			case -1:
				perror("Error\n");
				// exit(1);
				status = 1;
				break;

			// child process
			case 0:
				execvp(args[0], args);
				// return error here
				perror("execvp");
				// set value retrieved by built-in `status` command to 1
				status = 1;
				// printf("%d\n", status);
				// fflush(stdout);
				break;

			// parent process
			default:
				waitpid(spawnPid, &childExitMethod, 0);
				// set status accordingly
				if (WIFEXITED(childExitMethod)) {
					int exitStatus = WEXITSTATUS(childExitMethod);
					// printf("%d\n", status);
				} else {
					int exitStatus = WTERMSIG(childExitMethod);
					// printf("%d\n", status);
				}
				fflush(stdout);
		}
	// if the command is supposed to run on the background
	} else {
		// replace "&" to set last argument to NULL for execvp()
		args[argsCount - 1] = NULL;

		// initialize with bogus values
		pid_t spawnPid = -5;
		int childExitMethod = -5;

		// fork a child process
		spawnPid = fork();

		switch (spawnPid) {
			// error
			case -1:
				perror("Error\n");
				exit(1);
				break;

			// child process
			case 0:
				execvp(args[0], args);
				// return error here
				perror("execvp");
				// set value retrieved by built-in `status` command to 1
				status = 1;
				break;

			default:
				waitpid(spawnPid, &childExitMethod, WNOHANG);
				printf("background pid is %d\n", spawnPid);
				fflush(stdout);

				background = 0;
				childProcessPids[childProcessCount] = spawnPid;
				childProcessCount++;
		}
	}
}

// function to kill process and jobs, and exit the shell
void exitShell(void) {
	// need to further implement killing of all processes/jobs
	exit(0);
}

// function to change to given directory
// change to home directory if not given the destination
void changeDirectory(char *directory) {
	int dirChanged;

	if (strcmp(directory, "HOME") == 0) {
		dirChanged = chdir(getenv("HOME"));
	} else {
		dirChanged = chdir(directory);
	}
	
	// error/testing message
	// if (dirChanged != 0) {
	// 	printf("cd was unsuccessful\n");
	// } else {
	// 	char cwd[2048];
	// 	if (getcwd(cwd, sizeof(cwd)) != NULL) {
	// 		printf("CWD: %s\n", cwd);
	// 	}
	// }
	// fflush(stdout);
}

// function to print exit status or terminating signal
// of the last foreground process ran by the shell - placeholder
void printStatus(int status) {
	if (background == 1) {
		printf("terminated by signal %d\n", status);
	} else {
		printf("exit value %d\n", status);
	}
	fflush(stdout);
}

// function to get and parse command line input from user
int getAndParseCommand(void) {
	char command[MAX_COMMAND_LENGTH];
	char* args[MAX_COMMAND_ARGS];
	int argsCount = 0;
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

	// re-prompt for another command if input is a comment or a blank line
	if (args[0][0] == '#' || args[0][0] == '\n') {
		return 0;

	// if `exit` command was given, kill processes/jobs and exit the shell
	// execute this command in foreground only
	} else if (strcmp(args[0], "exit") == 0) {
		background = 0;
		exitShell();

	// if `cd` command was given, change the working directory
	// if the command was not given an argument, change to home directory
	// execute this command in foreground only
	} else if (strcmp(args[0], "cd") == 0) {
		background = 0;
		if (argsCount == 1 || strcmp(args[1], "&") == 0) {
			char dir[] = "HOME";
			changeDirectory(dir);
		} else {
			changeDirectory(args[1]);
		}

	// if `status` command was given, print exit status or terminating signal
	// execute this command in foreground only
	} else if (strcmp(args[0], "status") == 0) {
		background = 0;
		printStatus(status);

	// if we need to execute any other commands
	} else {
		executeOtherCommands(args, argsCount);
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
		// check status of background child processes
		checkChildProcesses();
		getAndParseCommand();
	}
	return 0;
}
