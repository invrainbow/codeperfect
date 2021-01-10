#include "common.hpp"

#if OS_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

int _test_new_process() {
    int stdin_pipe[2];
    int stdout_pipe[2];
    int pid;

    if (pipe(stdin_pipe) < 0) {
        perror("allocating pipe for child input redirect");
        return -1;
    }
    if (pipe(stdout_pipe) < 0) {
        close(stdin_pipe[PIPE_READ]);
        close(stdin_pipe[PIPE_WRITE]);
        perror("allocating pipe for child output redirect");
        return -1;
    }

    pid = fork();
    if (pid == -1) {
        // failed to create child
        close(stdin_pipe[PIPE_READ]);
        close(stdin_pipe[PIPE_WRITE]);
        close(stdout_pipe[PIPE_READ]);
        close(stdout_pipe[PIPE_WRITE]);
        return 0;
    }

    // child process
    if (pid == 0) {
        if (dup2(stdin_pipe[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
        if (dup2(stdout_pipe[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);

        // all these are for use by parent only
        close(stdin_pipe[PIPE_READ]);
        close(stdin_pipe[PIPE_WRITE]);
        close(stdout_pipe[PIPE_READ]);
        close(stdout_pipe[PIPE_WRITE]); 

        // run child process image
        exit(execlp("/bin/bash", "bash", "-c", "echo 8", NULL));
    }

    // parent process

    // close unused file descriptors, these are for child only
    close(stdin_pipe[PIPE_READ]);
    close(stdout_pipe[PIPE_WRITE]); 

    write(stdin_pipe[PIPE_WRITE], "hurrdurr", strlen("hurrudrr"));

    // done writing
    close(stdin_pipe[PIPE_WRITE]);

    printf("printing!!!\n");
    // read shit
    {
        char ch;
        while (read(stdout_pipe[PIPE_READ], &ch, 1) == 1)
            write(STDOUT_FILENO, &ch, 1);
    }
    printf("\nok we're done\n");

    // done reading
    close(stdout_pipe[PIPE_READ]);

    for (;; usleep(1)) {
        int status;
        auto w = waitpid(pid, &status, WNOHANG);
        if (w == -1) return 1;
        if (w == 0) continue;

        // TODO: here we can check status to see how the process exited
        break;
    }

    return 0;
}

#endif
