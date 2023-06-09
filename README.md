# SWISH
SWISH - Simple Working Implementation Shell, written in C. Handles a small set of shell commands, including redirection.

Whenever you are using a terminal program, you are really interacting with a shell process. Command-line shells allow one access to the capabilities of a computer using simple, interactive means. Type the name of a program and the shell will bring it to life as a new process, run it, and show output.

The goal of this project is to write a simplified command-line shell called swish. This shell will be less functional in many ways from standard shells like bash (the default on most Linux machines), but will still have some useful features.

## This project uses a number of important systems programming topics:

- String tokenization using strtok()
- Getting and setting the current working directory with getcwd() and chdir()
- Program execution using fork() and wait()
- Child process management using wait() and waitpid()
- Input and output redirection with open() and dup2()
- Signal handling using setpgrp(), tcsetpgrp(), and sigaction()
- Managing foreground and background job execution using signals and kill()

## Supported Commands:

- <code>pwd</code>: Print the shell's current working directory
- <code>cd</code>: Change the shell's current working directory
- <code>exit</code>: Close the shell process
- <code>jobs</code>: Print out current list of pending jobs
- <code>fg</code>: Move stopped job into foreground
- <code>bg</code>: Move stopped job into background
- <code>wait-for</code>: Wait for a specific job identified by its index in job list
- <code>wait-all</code>: Wait for all background jobs
- <code>&</code>: (Mode/option at end of command line argument) Start the current command in the background.

If the user input does not match any built-in shell command, treat the input as a program name and command-line arguments.

## Diagram of the lifecycle of processes in SWISH:
![image](https://github.com/JacksonKary/SWISH/assets/117691954/5ce06de0-b111-4c8f-89ee-2625038ab099)


## What is in this directory?
<ul>
  <li>  <code>swish.c</code> : Implements the command-line interface for the swish shell.
  <li>  <code>swish_funcs.h</code> : Header file for swish helper functions.
  <li>  <code>swish_funcs.c</code> : Implementations of swish helper functions.
  <li>  <code>job_list.h</code> : Header file for a linked list data structure to store terminal jobs.
  <li>  <code>job_list.c</code> : Implementation of the linked list data structure for terminal jobs.
  <li>  <code>string_vector.h</code> : Header file for a vector data structure to store strings.
  <li>  <code>string_vector.c</code> : Implementation of the string vector data structure.
  <li>  <code>Makefile</code> : Build file to compile and run test cases.
  <li>  <code>test_cases</code> Folder, which contains:
  <ul>
    <li>  <code>input</code> : Input files used in automated testing cases.
    <li>  <code>resources</code> : More input files.
    <li>  <code>output</code> : Expected output.
  </ul>
  <li>  <code>testius</code> : Python script that runs the tests.
</ul>

## Running Tests

A Makefile is provided as part of this project. This file supports the following commands:

<ul>
  <li>  <code>make</code> : Compile all code, produce an executable program.
  <li>  <code>make clean</code> : Remove all compiled items. Useful if you want to recompile everything from scratch.
  <li>  <code>make clean-tests</code> : Remove all files produced during execution of the tests.
  <li>  <code>make test</code> : Run all test cases.
  <li>  <code>make test testnum=5</code> : Run test case #5 only.
</ul>
