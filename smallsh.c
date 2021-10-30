#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>

// smallsh supports commands of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

// initialize global variables
int status = 0;
bool background = false;
int childProcessCount = 0;
int childProcessPids[80];
bool foregroundOnlyMode = false;

// initialize empty structs for SIGINT and SIGTSTP
// reference: Exploration: Signal Handling API
struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

/*
 * ctrl-z command will enter/exit shell into/from foreground-only mode
 * display informative message and prompt user to enter a command
 * reference: Exploration: Signal Handling API
 */
void handle_SIGTSTP() {
	// if shell is not in foreground-only mode, enter foreground-only mode
	if (!foregroundOnlyMode) {
		foregroundOnlyMode = true;

		char* enterMessage = "\nEntering foreground-only mode (& is now ignored)";
		write(STDOUT_FILENO, enterMessage, 49);
		fflush(stdout);

		char* rePrompt = "\n: ";
		write(STDOUT_FILENO, rePrompt, 3);
		fflush(stdout);
	// else, exit foreground-only mode
	} else {
		foregroundOnlyMode = false;

		char* exitMessage = "\nExiting foreground-only mode";
		write(STDOUT_FILENO, exitMessage, 29);
		fflush(stdout);

		char* rePrompt = "\n: ";
		write(STDOUT_FILENO, rePrompt, 3);
		fflush(stdout);
	}
}

/*
 * 'status' command
 * print exit status or the terminating signal of the last foreground process ran by the shell
 * reference: Exploration: Process API - Monitoring Child Processes
 */
void printStatus(int status) {
	if (WIFEXITED(status)) {
		printf("exit value %d\n", WEXITSTATUS(status));
	} else {
		printf("terminated by signal %d\n", WTERMSIG(status));
	}
	fflush(stdout);
}

/*
 * check the status of background child processes
 * display informative message upon finding a process that completed
 */
void checkChildProcesses(void) {
	// initialize childExitMethod with bogus value
	int childExitMethod = -5;
	
	// check child process pids to check if any completed
	int i;
	for (i = 0; i < childProcessCount; i++) {
		if (childProcessPids[i] == -5) {
			continue;
		}
		else if (waitpid(childProcessPids[i], &childExitMethod, WNOHANG)) {
				// display message when a child process completed
				printf("background pid %d is done: ", childProcessPids[i]);
				printStatus(childExitMethod);
				fflush(stdout);
				// set completed process' pid to a bogus value to no longer check
				childProcessPids[i] = -5;
		}
	}
}

/*
 * function to execute commands in the foreground
 * check for I/O redirection and fork a child process
 * references: 
 * Explorations: Processes and I/O, Process API - Creating and Terminating Processes, Signal Handling API
 */
void executeFgCommands(char**args, int argsCount) {
	// save stdin/stdout to re-open them after completion of command with redirection
	// reference: https://stackoverflow.com/questions/9084099/re-opening-stdout-and-stdin-file-descriptors-after-closing-them
	int saveStdin = dup(0);
	int saveStdout = dup(1);

	int sourceFD = -1;
	int targetFD = -1;
	bool redirection = false;

	// iterate over entered command to check for redirection and set file descriptors
	int i;
	for (i = 0; i < argsCount; i++) {
		// if command contains "<", mark redirection as true and open input file
		if (strcmp(args[i], "<") == 0) {
			redirection = true;
			sourceFD = open(args[i+1], O_RDONLY);
			// display error if file cannot be opened for input and set status to 1
			if (sourceFD == -1) {
				printf("cannot open %s for input\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		// if command contains ">", mark redirection as true and open output file
		} else if (strcmp(args[i], ">") == 0) {
			redirection = true;
			targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
			// display error if file cannot be opened for output and set status to 1
			if (targetFD == -1) {
				printf("cannot open %s for output\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		}
	}
	// if redirection in command, close stdin/stdout depending on file descriptors being opened
	if (redirection) {
		if (sourceFD != -1 && targetFD != -1) {
			close(0);
			close(1);
		} else if (sourceFD != -1) {
			close(0);
		} else {
			close(1);
		}
		
		// if source file descriptor was found, use dup2 for redirection
		if (sourceFD != -1) {
			int sourceResult = dup2(sourceFD, 0);
			if (sourceResult == -1) {
				perror("source dup2()");
				status = 2;
			}
		}

		// if target file descriptor was found, use dup2 for redirection
		if (targetFD != -1) {
			int targetResult = dup2(targetFD, 1);
			if (targetResult == -1) {
				perror("target dup2()");
				status = 2;
			}
		}

		// nullify redirection symbols, source, and destination from the command
		int j;
		for (j = 1; j < argsCount; j++) {
			args[j] = NULL;
		}
		// close file descriptors
		close(sourceFD);
		close(targetFD);
	} 

	// if there is no redirection in the command, set last argument as NULL for execvp()
	else {
		if (background && foregroundOnlyMode) {
			args[argsCount - 1] = NULL;
		} else {
			args[argsCount] = NULL;
		}
	}
	
	// initialize pid and exit method of child process with bogus values
	pid_t spawnPid = -5; 
	int childExitMethod = -5;

	// fork a child process
	spawnPid = fork();

	switch (spawnPid) {
		// if error, display error and exit with a status of 1
		case -1:
			perror("fork() error\n");
			exit(1);
			break;

		// child process
		case 0:
			// for foreground processes, take default action for SIGINT
			SIGINT_action.sa_handler = SIG_DFL;
			sigaction(SIGINT, &SIGINT_action, NULL);

			// execute the command
			execvp(args[0], args);
			// if error, display error and exit with a status of 1
			perror(args[0]);
			exit(1);
			break;

		// parent process
		default:
			waitpid(spawnPid, &childExitMethod, 0);
			// print and set status of the child process
			if (WIFSIGNALED(childExitMethod)) {
				status = childExitMethod;
				printStatus(childExitMethod);
			} else {
				status = childExitMethod;
			}
	}
	// if redirection in the command, re-open stdin/stdout
	if (redirection) {
		dup2(saveStdin, 0);
		dup2(saveStdout, 1);
		close(saveStdin);
		close(saveStdout);
	}
}

/*
 * function to execute commands in the background
 * check for I/O redirection and fork a child process
 * reference: Explorations: Processes and I/O, Process API - Creating and Terminating Processes
 */
void executeBgCommands(char** args, int argsCount) {	
	// save stdin/stdout to re-open them after completion of command with redirection
	// reference: https://stackoverflow.com/questions/9084099/re-opening-stdout-and-stdin-file-descriptors-after-closing-them
	int saveStdin = dup(0);
	int saveStdout = dup(1);

	int sourceFD = -1;
	int targetFD = -1;
	int redirection = false;

	// iterate over command to check for redirection and set file descriptors
	int i;
	for (i = 0; i < argsCount; i++) {
		if (strcmp(args[i], "<") == 0) {
			// if command contains "<", mark redirection as true and open input file
			redirection = true;
			sourceFD = open(args[i+1], O_RDONLY);
			// display error if file cannot be opened for input and set status to 1
			if (sourceFD == -1) {
				printf("cannot open %s for input\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		// if command contains ">", mark redirection as true and open output file
		} else if (strcmp(args[i], ">") == 0) {
			redirection = true;
			targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
			// display error if file cannot be opened for output and set status to 1
			if (targetFD == -1) {
				printf("cannot open %s for output\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		}
	}

	if (redirection) {
		// if redirection in command, close stdin/stdout
		close(0);
		close(1);

		// if source file descriptor was found, dup2 for redirection
		int sourceResult;
		if (sourceFD != -1) {
			sourceResult = dup2(sourceFD, 0);
		// else, set stdin to /dev/null
		} else {
			sourceResult = open("/dev/null", O_RDONLY);
		}
		// check for errors using dup2
		if (sourceResult == -1) {
			perror("source dup2()");
			status = 2;
		}

		// if target file descriptor was found, dup2 for redirection
		int targetResult;
		if (targetFD != -1) {
			targetResult = dup2(targetFD, 1);
		// else, set stdout to /dev/null
		} else {
			targetResult = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		}
		// check for errors using dup2
		if (targetResult == -1) {
			perror("target dup2()");
			status = 2;
		}
		// nullify redirection symbols, source, and destination from the command
		int j;
		for (j = 1; j < argsCount; j++) {
			args[j] = NULL;
		}
		// close file descriptors
		close(sourceFD);
		close(targetFD);
	}

	// if there is no redirection in the command, set stdin/stdout to /dev/null
	else {
		int sourceResult = open("/dev/null", O_RDONLY);
		if (sourceResult == -1) {
			perror("source dup2()");
			status = 2;
		}

		int targetResult = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (targetResult == -1) {
			perror("target dup2()");
			status = 2;
		}
		close(sourceResult);
		close(sourceResult);
		
		// replace "&" to set last argument to NULL for execvp()
		args[argsCount - 1] = NULL;
	}
	// initialize pid and exit method of child process with bogus values
	pid_t spawnPid = -5;
	int childExitMethod = -5;

	// fork a child process
	spawnPid = fork();

	switch (spawnPid) {
		// if error, display error and exit with a status of 1
		case -1:
			perror("fork() error\n");
			exit(1);
			break;

		// child process
		case 0:
			// execute the command
			execvp(args[0], args);
			// if error, display error and exit with a status of 1
			perror(args[0]);
			exit(1);
			break;

		// parent process
		default:
			waitpid(spawnPid, &childExitMethod, WNOHANG);
			// display message about child process running in the background
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);
			// set background flag back to false for subsequent commands
			background = false;
			// add forked child process pid to array to check its status
			childProcessPids[childProcessCount] = spawnPid;
			childProcessCount++;
	}
	// re-open stdin/stdout
	close(0);
	close(1);
	dup2(saveStdin, 0);
	dup2(saveStdout, 1);
	close(saveStdin);
	close(saveStdout);
}

/*
 * 'exit' command
 * function to kill process and jobs and exit the shell
 * reference: https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-kill-send-signal-process
 */
void exitShell(void) {
	int i;
	for (i = 0; i < childProcessCount; i++) {
		kill(childProcessPids[i], SIGTERM);
	}
	exit(0);
}

/*
 * 'cd' command
 * function to change directory
 * change to home directory if 'cd' was not given a destination
 */
void changeDirectory(char *directory) {
	int dirChanged;
	if (strcmp(directory, "HOME") == 0) {
		dirChanged = chdir(getenv("HOME"));
	} else {
		dirChanged = chdir(directory);
	}
}

/*
 * function to get and parse command from user
 * prompt user for command and parse command for variable expansion/comments
 * additional checks for running command in the background, and built-in commands
 */

int getAndParseCommand(void) {
	char command[MAX_COMMAND_LENGTH];
	char* args[MAX_COMMAND_ARGS];
	int argsCount = 0;
	pid_t pid = getpid();

	// prompt and get command
	printf(": ");
	fflush(stdout);
	fgets(command, MAX_COMMAND_LENGTH, stdin);

	// rid of the newline characater from the command line input
	strtok(command, "\n");

	int i = 0;
	char *token = strtok(command, " ");

	// parse command line input into separate arguments
	while (token) {
		// replace substring of "$$" within the command with pid of the shell
		// reference: https://stackoverflow.com/questions/12784766/check-substring-exists-in-a-string-in-c
		if (strstr(token, "$$") != NULL) {
			int j;
			for (j = 0; j < strlen(token); j++) {
				if (token[j] == '$' && token[j+1] == '$') {
					// terminate token early to rid of "$$" substring
					// reference: https://stackoverflow.com/questions/20342559/how-to-cut-part-of-a-string-in-c
					token[j] = '\0';

					// create a new argument string to expand the $$ variable and store the argument
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

	// check and flag if command is to be executed in the background
	if (strcmp(args[argsCount - 1], "&") == 0) {
		background = true;
	} else {
		background = false;
	}

	// re-prompt for another command if input is a comment or a blank line
	if (args[0][0] == '#' || args[0][0] == '\n') {
		return 0;

	// if 'exit' command was given, kill processes/jobs and exit the shell
	} else if (strcmp(args[0], "exit") == 0) {
		// execute this command in foreground only
		background = false;
		exitShell();

	// if 'cd' command was given, change the working directory
	} else if (strcmp(args[0], "cd") == 0) {
		// execute this command in foreground only
		background = false;
		if (argsCount == 1 || strcmp(args[1], "&") == 0) {
			char dir[] = "HOME";
			changeDirectory(dir);
		} else {
			changeDirectory(args[1]);
		}

	// if 'status' command was given, print exit status or terminating signal of the last foreground process ran by the shell
	} else if (strcmp(args[0], "status") == 0) {
		// execute this command in foreground only
		background = false;
		printStatus(status);

	// execute any other commands
	} else {
		if (!background || foregroundOnlyMode) {
			executeFgCommands(args, argsCount);
		} else {
			executeBgCommands(args, argsCount);
		}
	}
	return 0;
}

/*
 * main function to register signal handlers and
 * continuously prompt user for command until shell exits
 * reference: Exploration: Signal Handling API
 */
int main(void) {
	// register SIG_IGN as signal handler for SIGINT_action struct
	SIGINT_action.sa_handler = SIG_IGN;
	// register SIGINT_action as hanlder for SIGINT to ignore SIGINT
	sigaction(SIGINT, &SIGINT_action, NULL);

	// register handle_SGTSTP as signal hanlder for SIGTSTP_action struct
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// block catchable signals
	sigfillset(&SIGTSTP_action.sa_mask);

	// use flag to restart after signal handler is done 
	SIGTSTP_action.sa_flags = SA_RESTART;
	// register SIGTSTP_action as handler for SIGTSTP to handle foreground modes
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// continuously run the shell for command line input until exited
	while (1) {
		// before getting and parsing command, check status of background child processes each time
		checkChildProcesses();
		getAndParseCommand();
	}
	return 0;
}
