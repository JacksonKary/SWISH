#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // Tokenize string s
    // Assume each token is separated by a single space (" ")
    // Use the strtok() function to accomplish this
    // Add each token to the 'tokens' parameter (a string vector)
    // Return 0 on success, -1 on error
    char *delimeter = " ";
    char *token;
    if ((token = strtok(s, delimeter)) == NULL) {  // if s is empty or only contains delimiters, return error
        perror("Invalid argument");
        return -1;
    }

    while (token != NULL) {  // add the rest of the arguments in
        strvec_add(tokens, token);
        token = strtok(NULL, delimeter);
    }

    return 0;
}

int run_command(strvec_t *tokens) {
    // Need to do two items of setup before exec()'ing
    // 1. Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults.
    // The code in main() within swish.c sets these handlers to the SIG_IGN value.
    // Adapt this code to use sigaction() to set the handlers to the SIG_DFL value.
    // 2. Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID
    struct sigaction sac;
    sac.sa_handler = SIG_DFL;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    if (setpgid(getpid(), 0) != 0) {
        perror("setpgid");
        return 1;
    }
    // Extend this function to perform output redirection before exec()'ing
    // Check for '<' (redirect input), '>' (redirect output), '>>' (redirect and append output)
    // entries inside of 'tokens' (the strvec_find() function will do this for you)
    // Open the necessary file for reading (<), writing (>), or appending (>>)
    // Use dup2() to redirect stdin (<), stdout (> or >>)
    // DO NOT pass redirection operators and file names to exec()'d program
    // E.g., "ls -l > out.txt" should be exec()'d with strings "ls", "-l", NULL

    int index;
    int endProgram = 0;

    if ((index = strvec_find(tokens, "<")) != -1) {  // "<" present: redirects input to tokens[0] program from standard input to the file specified after "<"
                                                        // opens for read only
        endProgram = index;
        const char *read_file;
        if ((read_file = strvec_get(tokens, index + 1)) == NULL) {  // gets next token which should be file name, checks for error
            perror("No file specified after \"<\"");
            return -1;
        }
        int fdr;
        if ((fdr = open(read_file, O_RDONLY, S_IRUSR|S_IWUSR)) == -1) {  // open read_file - open for reading, checks for error
            perror("Failed to open input file");
            close(fdr);
            return -1;
        }
        // No errors, use dup2 to redirect input
        if (dup2(fdr, STDIN_FILENO) == -1) {
            perror("dup2");
            if (close(fdr) != 0) {
                perror("Failed to close file");
            }
            return -1;
        }
    }
    if ((index = strvec_find(tokens, ">")) != -1) {  // ">" present: redirects the output of tokens[0] from standard output
                                                        // to the file listed after ">" - create or trunc the output file
        if (endProgram == 0) {
            endProgram = index;
        }
        const char *write_file;
        if ((write_file = strvec_get(tokens, index + 1)) == NULL) {  // gets next token which should be file name, checks for error
            perror("No file specified after \">\"");
            return -1;
        }
        int fdw;
        if ((fdw = open(write_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) == -1) {  // open write_file - create or truncate existing file
            perror("Failed to open output file");
            close(fdw);
            return -1;
        }
        // No errors, use dup2 to redirect output
        if (dup2(fdw, STDOUT_FILENO) == -1) {
            perror("dup2");
            if (close(fdw) != 0) {
                perror("Failed to close file");
            }
            return -1;
        }
    } else if ((index = strvec_find(tokens, ">>")) != -1) {  // ">>" present: same as ">" but no TRUNC, only append or create the output file
        if (endProgram == 0) {
            endProgram = index;
        }
        const char *write_file;
        if ((write_file = strvec_get(tokens, index + 1)) == NULL) {  // gets next token which should be file name, checks for error
            perror("No file specified after \">>\"");
            return -1;
        }
        int fdw;
        if ((fdw = open(write_file, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR)) == -1) {  // open write_file - create or append if it already exists
            perror("Failed to open output file");
            close(fdw);
            return -1;
        }
        // No errors, use dup2 to redirect output
        if (dup2(fdw, STDOUT_FILENO) == -1) {
            perror("dup2");
            if (close(fdw) != 0) {
                perror("Failed to close file");
            }
            return -1;
        }
    }
    if (endProgram != 0) {
        strvec_take(tokens, endProgram);
    }
    // Execute the specified program (token 0) with the
    // specified command-line arguments
    // THIS FUNCTION SHOULD BE CALLED FROM A CHILD OF THE MAIN SHELL PROCESS
    // Build a string array from the 'tokens' vector and pass this into execvp()
    char *strarr[MAX_ARGS + 1];  // + 1 is to make space for the NULL sentinel value
    int i = 0;
    char *temp;
    while ((temp = strvec_get(tokens, i)) != NULL && i < (MAX_ARGS + 1)) {
        strarr[i] = temp;
        i++;
    }
    strarr[i] = NULL;
    if (execvp(strarr[0], strarr) == -1) {
        perror("exec");
        return -1;
    }

    // Not reachable after a successful exec(), but retain here to keep compiler happy
    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // Implement the ability to resume stopped jobs in the foreground
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Call tcsetpgrp(STDIN_FILENO, <job_pid>) where job_pid is the job's process ID
    // 3. Send the process the SIGCONT signal with the kill() system call
    // 4. Use the same waitpid() logic as in main -- dont' forget WUNTRACED
    // 5. If the job has terminated (not stopped), remove it from the 'jobs' list
    // 6. Call tcsetpgrp(STDIN_FILENO, <shell_pid>). shell_pid is the *current*
    //    process's pid, since we call this function from the main shell process
    if (is_foreground == 1) {  // "fg"
        const char *c = strvec_get(tokens, 1);  // get index of job_t in jobs_list_t supplied by user as second command line argument (tokens[1])
        int job_index = atoi(c);  // convert index from string to integer
        job_t *job_to_resume;
        if ((job_to_resume = job_list_get(jobs, job_index)) == NULL) {  // get job_t from specified index, check for errors
            fprintf(stderr, "Job index out of bounds\n");
            return -1;
        }
        if (tcsetpgrp(STDIN_FILENO, job_to_resume->pid) != 0) {  // move job_to_resume to the foreground, check for errors
            perror("tcsetpgrp");
            return -1;
        }
        if (kill(job_to_resume->pid, SIGCONT) != 0) {  // send continue signal to job_to_resume, check for errors
            perror("kill");
            return -1;
        }
        int wstatus;
        if (waitpid(job_to_resume->pid, &wstatus, WUNTRACED) == -1) {  // wait for job to terminate or stop, check for errors
            perror("waitpid");
            return -1;
        }
        if (WIFSTOPPED(wstatus)) {  // if job stopped, keep it in job list
            return 0;
        }
        // ELSE (job terminated) - job has been waited for AND didn't get stopped
        if (job_list_remove(jobs, job_index) != 0) {  // remove from job list, check for errors
            perror("job_list_remove");
            return -1;
        }
        if (tcsetpgrp(STDIN_FILENO, getpid()) != 0) {  // move terminal to foreground, check for errors
            perror("tcsetpgrp");
            return -1;
        }
    }  // else - "bg"
    // Implement the ability to resume stopped jobs in the background.
    // This really just means omitting some of the steps used to resume a job in the foreground:
    // 1. DO NOT call tcsetpgrp() to manipulate foreground/background terminal process group
    // 2. DO NOT call waitpid() to wait on the job
    // 3. Make sure to modify the 'status' field of the relevant job list entry to JOB_BACKGROUND
    //    (as it was JOB_STOPPED before this)
    else {
        const char *c = strvec_get(tokens, 1);  // get index of job_t in jobs_list_t supplied by user as second command line argument (tokens[1])
        int job_index = atoi(c);  // convert index from string to integer
        job_t *job_to_resume;
        if ((job_to_resume = job_list_get(jobs, job_index)) == NULL) {  // get job_t from specified index, check for errors
            fprintf(stderr, "Job index out of bounds\n");
            return -1;
        }
        if (kill(job_to_resume->pid, SIGCONT) != 0) {  // send continue signal to job_to_resume, check for errors
            perror("kill");
            return -1;
        }
        job_to_resume->status = JOB_BACKGROUND;
        if (tcsetpgrp(STDIN_FILENO, getpid()) != 0) {  // move terminal to foreground, check for errors
            perror("tcsetpgrp");
            return -1;
        }
    }
    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // Wait for a specific job to stop or terminate
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Make sure the job's status is JOB_BACKGROUND (no sense waiting for a stopped job)
    // 3. Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    // 4. If the process terminates (is not stopped by a signal) remove it from the jobs list
    const char *c = strvec_get(tokens, 1);  // get index of job_t in jobs_list_t supplied by user as second command line argument (tokens[1])
    int job_index = atoi(c);  // convert index from string to integer
    job_t *job_to_resume;
    if ((job_to_resume = job_list_get(jobs, job_index)) == NULL) {  // get job_t from specified index, check for errors
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }
    if (job_to_resume->status != JOB_BACKGROUND) {  // check job's status, check for error
        fprintf(stderr, "Job index is for stopped process not background process\n");
        return -1;
    }
    int wstatus;
    if (waitpid(job_to_resume->pid, &wstatus, WUNTRACED) == -1) {  // wait for job to terminate or stop, check for errors
        perror("waitpid");
        return -1;
    }
    if (WIFSTOPPED(wstatus)) {  // if job stopped, keep it in job list
        return 0;
    }
    // ELSE (job terminated) - job has been waited for AND didn't get stopped
    if (job_list_remove(jobs, job_index) != 0) {  // remove from job list, check for errors
        perror("job_list_remove");
        return -1;
    }

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    // Wait for all background jobs to stop or terminate
    // 1. Iterate through the jobs list, ignoring any stopped jobs
    // 2. For a background job, call waitpid() with WUNTRACED.
    // 3. If the job has stopped (check with WIFSTOPPED), change its
    //    status to JOB_STOPPED. If the job has terminated, do nothing until the
    //    next step (don't attempt to remove it while iterating through the list).
    // 4. Remove all background jobs (which have all just terminated) from jobs list.
    //    Use the job_list_remove_by_status() function.
    job_t *temp = jobs->head;
    int wstatus;
    for (int i = 1; i < jobs->length; i++) {  // iterate through each job in the jobs list
        if (temp->status == JOB_STOPPED) {
            temp = temp->next;
            continue;
        }
        if (waitpid(temp->pid, &wstatus, WUNTRACED) == -1) { // wait for job to terminate or stop, check for errors
            perror("waitpid");
            return -1;
        }
        if (WIFSTOPPED(wstatus)) {  // if job stopped, change its status and move on
            temp->status = JOB_STOPPED;
            temp = temp->next;
            continue;
        }
        temp = temp->next;
    }
    job_list_remove_by_status(jobs, JOB_BACKGROUND);  // remove all (now waited for) non-stopped jobs with the status JOB_BACKGROUND

    return 0;
}
