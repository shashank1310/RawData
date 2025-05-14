#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <errno.h>

// ARM syscall number for pidfd_open
#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

int main(void) {
    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        // Child process: sleep and exit
        sleep(2);
        _exit(42);
    }

    // Parent process: open pidfd for child
    int pidfd = syscall(__NR_pidfd_open, child, 0);
    if (pidfd < 0) {
        perror("pidfd_open syscall");
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 2;
    }

    printf("pidfd for child %d: %d\n", child, pidfd);

    struct pollfd pfd = {
        .fd = pidfd,
        .events = POLLIN,
    };
    printf("Polling pidfd...\n");
    int ret = poll(&pfd, 1, 5000); // 5 second timeout
    if (ret < 0) {
        perror("poll");
        close(pidfd);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 3;
    } else if (ret == 0) {
        printf("Timeout waiting for child to exit!\n");
        close(pidfd);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 4;
    }

    if (pfd.revents & POLLIN) {
        printf("Child exited!\n");
    } else {
        printf("Unexpected poll revents: %x\n", pfd.revents);
    }
    close(pidfd);

    int status = 0;
    waitpid(child, &status, 0);
    if (WIFEXITED(status)) {
        printf("Child exit code: %d\n", WEXITSTATUS(status));
    } else {
        printf("Child did not exit normally.\n");
    }
    return 0;
}
