#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // Adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // Print the shell's current working directory
            // Use the getcwd() system call
            char buf[CMD_LEN];
            if (getcwd(buf, CMD_LEN) == NULL) {
                perror("getcwd");
                strvec_clear(&tokens);
                job_list_free(&jobs);
                printf("%s", PROMPT);
                continue;
            }
            printf("%s\n", buf);
        }

        else if (strcmp(first_token, "cd") == 0) {
            // Change the shell's current working directory
            // Use the chdir() system call
            // If the user supplied an argument (token at index 1), change to that directory
            // Otherwise, change to the home directory by default
            // This is available in the HOME environment variable (use getenv())
            const char *second_token = strvec_get(&tokens, 1);
            if (second_token == NULL) {  // if there is no second command line argument
                // get home dir
                char *temp;
                if ((temp = getenv("HOME")) == NULL) {  // get HOME directory, check for error
                    perror("getenv");
                    strvec_clear(&tokens);
                    job_list_free(&jobs);
                    // reprompt user
                    printf("%s", PROMPT);
                    continue;
                }
                if (chdir(temp) != 0) {  // change directory to HOME, check for error
                    perror("chdir");
                    strvec_clear(&tokens);
                    job_list_free(&jobs);
                    // reprompt user
                    printf("%s", PROMPT);
                    continue;
                }
            }
            else if (chdir(second_token) != 0) {  // change directory to second argument, check for error
                perror("chdir");
                strvec_clear(&tokens);
                job_list_free(&jobs);
                printf("%s", PROMPT);
                continue;
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == JOB_BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else {
            // If the last token input by the user is "&", start the current
            // command in the background.
            // 1. Determine if the last token is "&". If present, use strvec_take() to remove
            //    the "&" from the token list.
            // 2. Modify the code for the parent (shell) process: Don't use tcsetpgrp() or
            //    use waitpid() to interact with the newly spawned child process.
            // 3. Add a new entry to the jobs list with the child's pid, program name,
            //    and status JOB_BACKGROUND.
            const char *last_token = strvec_get(&tokens, tokens.length - 1);
            if (strcmp(last_token, "&") == 0) {  // last token of command input is "&"
                strvec_take(&tokens, tokens.length - 1); // remove "&" from tokens
                pid_t child_pid = fork();
                if (child_pid == -1) {  // error - child process not created
                    perror("fork");
                    strvec_clear(&tokens);
                    job_list_free(&jobs);
                    // reprompt user
                    printf("%s", PROMPT);
                    continue;
                } else if (child_pid == 0) {  // child process
                    if (run_command(&tokens) != 0) {  // run command, check for error
                        return 1;  // if error, return from child process
                    }
                }  // else - parent
                if (job_list_add(&jobs, child_pid, first_token, JOB_BACKGROUND) != 0) {
                    perror("job_list_add");
                    // reprompt user
                    printf("%s", PROMPT);
                    continue;
                }
            }
            // If the user input does not match any built-in shell command,
            // treat the input as a program name and command-line arguments
            // Take the following steps:
            //   1. Use fork() to spawn a child process
            //   2. Call run_command() in the child process
            //   2. In the parent, use waitpid() to wait for the program to exit
            pid_t child_pid = fork();
            if (child_pid == -1) {  // child process not created
                perror("fork");
                strvec_clear(&tokens);
                job_list_free(&jobs);
                // reprompt user
                printf("%s", PROMPT);
                continue;
            } else if (child_pid == 0) {  // child process
                if (run_command(&tokens) != 0) {  // run command, check for error
                    return 1;  // if error, return from child process
                }
            }  // else - parent process
            int wstatus;
            // pid_t terminated_pid = waitpid(child_pid, &wstatus, 0);


            // Set the child process as the target of signals sent to the terminal
            // via the keyboard.
            // To do this, call 'tcsetpgrp(STDIN_FILENO, <child_pid>)', where child_pid is the
            // child's process ID just returned by fork(). Do this in the parent process.
            if (tcsetpgrp(STDIN_FILENO, child_pid) != 0) {  // move child process to foreground, check for errors
                perror("tcsetpgrp");
                // reprompt user
                printf("%s", PROMPT);
                continue;
            }
            // Handle the issue of foreground/background terminal process groups.
            // Do this by taking the following steps in the shell (parent) process:
            // 1. Modify the call to waitpid(): Wait specifically for the child just forked, and
            //    use WUNTRACED as the third argument to detect if it has stopped from a signal
            // 2. After waitpid() has returned, call tcsetpgrp(STDIN_FILENO, <pid>) where pid is
            //    the process ID of the shell process (use getpid() to obtain it)
            // 3. If the child status was stopped by a signal, add it to 'jobs', the
            //    the terminal's jobs list.
            // Can detect if this has occurred using WIFSTOPPED on the status
            // variable set by waitpid()
            if (waitpid(child_pid, &wstatus, WUNTRACED) == -1) {  // wait for child process to terminate or stop, check for errors
                perror("waitpid");
                // reprompt user
                printf("%s", PROMPT);
                continue;
            }
            if (WIFSTOPPED(wstatus)) {  // if child process stopped, add it to job list, check for errors
                if (job_list_add(&jobs, child_pid, first_token, JOB_STOPPED) != 0) {
                    perror("job_list_add");
                    // reprompt user
                    printf("%s", PROMPT);
                    continue;
                }
            }
            if (tcsetpgrp(STDIN_FILENO, getpid()) != 0) {  // move terminal to foreground once child process terminates, check for errors
                perror("tcsetpgrp");
                // reprompt user
                printf("%s", PROMPT);
                continue;
            }
        }

        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }

    job_list_free(&jobs);
    return 0;
}
