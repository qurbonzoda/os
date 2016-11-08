#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include <iostream>
#include <fstream>

#include "utils.hpp"


const char *EXIT_RUNNING_DAEMON = "exit";
const char *PATH_TO_DAEMON_PID = "/tmp/rshd.pid";

int fork_perror() {
  int pid = fork();
  ensure_perror(pid != -1, "Couldn't fork");

  return pid;
}

void kill_running_daemon() {
    int daemon_pid_fd = open(PATH_TO_DAEMON_PID, O_RDONLY);
    ensure_perror(daemon_pid_fd != -1, "No daemon is running");

    char daemon_pid_str[10];
    int len = read(daemon_pid_fd, daemon_pid_str, 10);
    ensure_perror(len != -1, "Can't read daemon pid");

    close(daemon_pid_fd);

    daemon_pid_str[len] = '\0';

    int daemon_pid = atoi(daemon_pid_str);
    std::cout << daemon_pid << std::endl;

    ensure_perror(kill(daemon_pid, SIGINT) == 0, "Couldn't exit running daemon", "Running deamon is exitted");
}

int process_arg(char *arg) {
    if (strcmp(arg, EXIT_RUNNING_DAEMON) == 0) {
        kill_running_daemon();
        exit(0);
;    }
    else {
        ensure_perror(strlen(arg) <= 5, "Invalid port number");

        for (int i = 0; i < strlen(arg); i++) {
            ensure_perror(isdigit(arg[i]), "Invalid port number");
        }

        return atoi(arg);
    }
}

void set_new_sid() {
    std::cout << "setting new sid" << std::endl;
    ensure_perror(setsid(), "Couldn't set new sid");
}

void clean_up(int signum) {
    ensure(remove(PATH_TO_DAEMON_PID) != -1, "Couldn't clean-up", "Cleaned up");
    exit(0);
}

void setup_signal_handling() {
    std::cout << "setting up signal handling" << std::endl;

    struct sigaction sigact;

    sigact.sa_handler = *clean_up;

    sigset_t block_mask;
    sigfillset(&block_mask);

  	sigact.sa_mask = block_mask; //block other signals while handling current signal

    ensure_perror(sigaction(SIGINT, &sigact, NULL) != -1, "Can't register SIGINT handler");
}

void change_working_dir() {
    std::cout << "changing working dir" << std::endl;
    ensure_perror(chdir("/") != -1, "Coundn't change working directory to root directory");
}

void close_std_io() {
    std::cout << "closing std io" << std::endl;
    ensure(close(STDIN_FILENO) != -1
            && close(STDOUT_FILENO) != -1
            && close(STDERR_FILENO)  != -1, "Coundn't close standard io");
}

void write_daemon_pid(int daemon_pid) {
    std::cout << "printing pid: " << daemon_pid << std::endl;

    char daemon_pid_str[10];
    sprintf(daemon_pid_str, "%d", daemon_pid);

    mode_t file_permission = S_IRWXU;
    int openning_flag = O_WRONLY | O_CREAT | O_TRUNC;
    int daemon_pid_fd = open(PATH_TO_DAEMON_PID, openning_flag, file_permission);
    ensure_perror(daemon_pid_fd != -1, "Couldn't create or open PATH_TO_DAEMON_PID");

    write_all(daemon_pid_fd, daemon_pid_str, strlen(daemon_pid_str));

    close(daemon_pid_fd);
}

void become_daemon() {
    if (fork_perror() == 0) {
        set_new_sid();

        setup_signal_handling();

        int pid = fork_perror();
        if (pid == 0) {
            openlog("rshd", LOG_PID | LOG_CONS, LOG_DAEMON);

            change_working_dir();
            close_std_io();
            umask(0);
        }
        else {
            write_daemon_pid(pid);
            exit(0);
        }
    }
    else {
        exit(0);
    }
}

int main(int argc, char **argv) {
    std::cout << argc << std::endl;
    ensure_perror(argc == 2, "Usage: rshd <port_number>");

    int listenning_port = process_arg(argv[1]);       // escapable

    become_daemon();

    return 0;
}
