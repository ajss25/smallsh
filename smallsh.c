#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// shell supports command line inputs of up to 2048 characters and 512 arguments
#define MAX_COMMAND_LENGTH 2048
#define MAX_COMMAND_ARGS 512

// initialize global variables
int status = 0;
int background = 0;
int childProcessCount = 0;
int childProcessPids[80];
int foregroundOnlyMode = 0;

// initialize empty struct for SIGINT and SIGTSTP
struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

// function to handle SIGTSTP
void handle_SIGTSTP() {
	// enter foreground-only mode
	if (foregroundOnlyMode == 0) {
		foregroundOnlyMode = 1;
		char* enterMessage = "\nEntering foreground-only mode (& is now ignored)";
		write(STDOUT_FILENO, enterMessage, 49);
		fflush(stdout);
		char* rePrompt = "\n: ";
		write(STDOUT_FILENO, rePrompt, 3);
		fflush(stdout);
	// exit foreground-only mode
	} else {
		foregroundOnlyMode = 0;
		char* exitMessage = "\nExiting foreground-only mode";
		write(STDOUT_FILENO, exitMessage, 29);
		fflush(stdout);
		char* rePrompt = "\n: ";
		write(STDOUT_FILENO, rePrompt, 3);
		fflush(stdout);
	}
}

// declare implemented functions at the top
void printStatus();

// function to check status of child processes
void checkChildProcesses(void) {
	int childExitMethod = -5;
	int i;
	
	// loop through child process pids to check if finished running
	for (i = 0; i < childProcessCount; i++) {
		if (childProcessPids[i] == -5) {
			continue;
		}
		else if (waitpid(childProcessPids[i], &childExitMethod, WNOHANG)) {
				// print results
				printf("background pid %d is done: ", childProcessPids[i]);
				printStatus(childExitMethod);
				fflush(stdout);
				childProcessPids[i] = -5;

		}
	}
}

// function to execute commands in the foreground
void executeFgCommands(char**args, int argsCount) {
	// check for I/O redirection
	// reference: https://stackoverflow.com/questions/9084099/re-opening-stdout-and-stdin-file-descriptors-after-closing-them
	int saveStdin = dup(0);
	int saveStdout = dup(1);

	// iterate over command to check for redirection and set file descriptors
	int sourceFD = -1;
	int targetFD = -1;
	int redirection = 0;

	int i;
	for (i = 0; i < argsCount; i++) {
		if (strcmp(args[i], "<") == 0) {
			redirection = 1;
			sourceFD = open(args[i+1], O_RDONLY);
			if (sourceFD == -1) {
				printf("cannot open %s for input\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		
		} else if (strcmp(args[i], ">") == 0) {
			redirection = 1;
			targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (targetFD == -1) {
				printf("cannot open %s for output\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		}
	}

	if (redirection == 1) {
		if (sourceFD != -1 && targetFD != -1) {
			close(0);
			close(1);
		} else if (sourceFD != -1) {
			close(0);
		} else {
			close(1);
		}
		
		// if source file descriptor was found, dup2
		if (sourceFD != -1) {
			int sourceResult = dup2(sourceFD, 0);
			if (sourceResult == -1) {
				perror("source dup2()");
				status = 2;
			}
		}
		// if target file descriptor was found, dup2
		if (targetFD != -1) {
			int targetResult = dup2(targetFD, 1);
			if (targetResult == -1) {
				perror("target dup2()");
				status = 2;
			}
		}

		// after using dup2() to set up redirection, 
		// nullify redirection symbols and the destination/source from the command 
		// to pass just the command into exec()
		int j;
		for (j = 1; j < argsCount; j++) {
			args[j] = NULL;
		}

		close(sourceFD);
		close(targetFD);
	} 

	else {
		// set last argument to NULL for execvp()
		if (background == 1 && foregroundOnlyMode == 1) {
			args[argsCount - 1] = NULL;
		} else {
			args[argsCount] = NULL;
		}
	}
	
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
			// for foreground processes, take default action for SIGINT
			SIGINT_action.sa_handler = SIG_DFL;
			sigaction(SIGINT, &SIGINT_action, NULL);

			execvp(args[0], args);
			// return error and set value retrieved by built-in `status` command to 1
			perror(args[0]);
			exit(1);
			break;

		// parent process
		default:
			waitpid(spawnPid, &childExitMethod, 0);
			// print and set status accordingly
			if (WIFSIGNALED(childExitMethod)) {
				status = childExitMethod;
				printStatus(childExitMethod);
			} else {
				status = childExitMethod;
			}
	}

	if (redirection == 1) {
		dup2(saveStdin, 0);
		dup2(saveStdout, 1);
		close(saveStdin);
		close(saveStdout);
	}
}

// function to execute commands in the background
void executeBgCommands(char** args, int argsCount) {	
	// check for I/O redirection
	// reference: https://stackoverflow.com/questions/9084099/re-opening-stdout-and-stdin-file-descriptors-after-closing-them
	int saveStdin = dup(0);
	int saveStdout = dup(1);

	// iterate over command to check for redirection and set file descriptors
	int sourceFD = -1;
	int targetFD = -1;
	int redirection = 0;

	int i;
	for (i = 0; i < argsCount; i++) {
		if (strcmp(args[i], "<") == 0) {
			redirection = 1;
			sourceFD = open(args[i+1], O_RDONLY);
			if (sourceFD == -1) {
				printf("cannot open %s for input\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		
		} else if (strcmp(args[i], ">") == 0) {
			redirection = 1;
			targetFD = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (targetFD == -1) {
				printf("cannot open %s for output\n", args[i+1]);
				fflush(stdout);
				status = 1;
				return;
			}
		}
	}

	// if redirection
	if (redirection == 1) {
		close(0);
		close(1);

		// if source file descriptor was found, dup2
		int sourceResult;
		if (sourceFD != -1) {
			sourceResult = dup2(sourceFD, 0);
		// else stdin to /dev/null
		} else {
			sourceResult = open("/dev/null", O_RDONLY);
		}
		if (sourceResult == -1) {
			perror("source dup2()");
			status = 2;
		}

		// if target file descriptor was found, dup2
		int targetResult;
		if (targetFD != -1) {
			targetResult = dup2(targetFD, 1);
		// else stdout to /dev/null
		} else {
			targetResult = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		}
		if (targetResult == -1) {
			perror("target dup2()");
			status = 2;
		}
		// after using dup2() to set up redirection, 
		// nullify redirection symbols and the destination/source from the command 
		// to pass just the command into exec()
		int j;
		for (j = 1; j < argsCount; j++) {
			args[j] = NULL;
		}
		close(sourceFD);
		close(targetFD);
	}

	// if no redirection, set stdin/stdout to /dev/null
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
			// return error and set value retrieved by built-in `status` command to 1
			perror(args[0]);
			exit(1);
			break;

		default:
			waitpid(spawnPid, &childExitMethod, WNOHANG);
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);

			background = 0;
			childProcessPids[childProcessCount] = spawnPid;
			childProcessCount++;
	}
	close(0);
	close(1);
	dup2(saveStdin, 0);
	dup2(saveStdout, 1);
	close(saveStdin);
	close(saveStdout);
}

// function to kill process and jobs, and exit the shell
// reference: https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-kill-send-signal-process
void exitShell(void) {
	int i;
	for (i = 0; i < childProcessCount; i++) {
		kill(childProcessPids[i], SIGTERM);
	}
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
}

// function to print exit status or terminating signal
// of the last foreground process ran by the shell
void printStatus(int status) {
	if (WIFEXITED(status)) {
		printf("exit value %d\n", WEXITSTATUS(status));
	} else {
		printf("terminated by signal %d\n", WTERMSIG(status));
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
	} else {
		background = 0;
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
		if (background == 0 || foregroundOnlyMode == 1) {
			executeFgCommands(args, argsCount);
		} else {
			executeBgCommands(args, argsCount);
		}
	}
	return 0;
}

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

	// prompt user for command line input until exited
	while (1) {
		// check status of background child processes
		checkChildProcesses();
		getAndParseCommand();
	}
	return 0;
}
