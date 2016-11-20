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
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <termios.h>

#include <iostream>
#include <fstream>
#include <string>

#include "utils.hpp"


const char *EXIT_RUNNING_DAEMON = "exit";
const char *PATH_TO_DAEMON_PID = "/tmp/rshd.pid";

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
    }
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
    int fork_pid = fork();
    ensure_perror(fork_pid != -1, "Couldn't fork");
    if (fork_pid == 0) {
        set_new_sid();

        setup_signal_handling();

        int pid = fork();
        ensure_perror(pid != -1, "Couldn't fork");
        if (pid == 0) {
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

class connection {
    static const int DATA_SIZE = 4096;
    char *data;
    int epoll;
public:
    int sender;
    int receiver;

    connection(int sender, int receiver, int epoll) : sender(sender), receiver(receiver), epoll(epoll) {
        data = new char[DATA_SIZE];
    }

    void send() {
        int readBytes = read_all(sender, data, DATA_SIZE);
        std::string msg = "read " + std::to_string(readBytes);
        ensure(readBytes != -1, "Couldn't read sent data", msg.c_str());
        write_all(receiver, data, readBytes);
    }

    ~connection() {
        epoll_ctl(epoll, EPOLL_CTL_DEL, sender, NULL);
        epoll_ctl(epoll, EPOLL_CTL_DEL, receiver, NULL);

        close(sender);
        close(receiver);

        delete[] data;
    }
};

int get_server_socket(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ensure(server_socket != -1, "Couldn't create server socket");

    sockaddr_in sockaddress;
    memset(&sockaddress, 0, sizeof(sockaddress));
    sockaddress.sin_family = AF_INET;
    sockaddress.sin_addr.s_addr = INADDR_ANY;
    sockaddress.sin_port = htons(port);

    ensure(bind(server_socket, (sockaddr *)&sockaddress, sizeof(sockaddress)) != -1, "Couldn't bind");

    return server_socket;
}

void add_server_socket_to_epoll(int epoll_fd, int server_socket) {
    epoll_event event;
    event.data.fd = server_socket;
    event.events = EPOLLIN | EPOLLET | EPOLLPRI;

    ensure(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) != -1, "Coundn't add server socket to epoll");
}

void add_conn_pty_to_epoll(int epoll_fd, connection *conn_pty) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.ptr = conn_pty;

    ensure(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_pty->sender, &event) != -1, "Coundn't add_conn_pty_to_epoll");
}

void add_pty_conn_to_epoll(int epoll_fd, connection *pty_conn) {
    epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = pty_conn;

    ensure(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pty_conn->sender, &event) != -1, "Coundn't add_pty_conn_to_epoll");
}

int accept_connection(int server_socket) {
    sockaddr_in conn_addr;
    socklen_t conn_addr_size = sizeof(conn_addr);

    int conn_sock = accept4(server_socket, (sockaddr *)&conn_addr, &conn_addr_size, SOCK_CLOEXEC);
    ensure(conn_sock != -1, "Coundn't accept connection", "accepted a connection");

    return conn_sock;
}

int open_and_setup_pty_master() {
    int pty_master = posix_openpt(O_RDWR | O_CLOEXEC);
    ensure(pty_master != -1, "Coundn't open pty master");

    ensure(grantpt(pty_master) != -1, "Coundn't grantpt");
    ensure(unlockpt(pty_master) != -1, "Coundn't unlockpt");

    return pty_master;
}

int open_and_setup_pty_slave(int pty_master) {
    int pty_slave = open(ptsname(pty_master), O_RDWR | O_CLOEXEC);
    ensure(pty_slave != -1, "Coundn't open pty_slave");

    termios pty_slave_attr;
    ensure(tcgetattr(pty_slave, &pty_slave_attr) != -1, "Coundn't get pty_slave attrs");

    pty_slave_attr.c_lflag &= ~ECHO;
    pty_slave_attr.c_lflag |= ISIG;
    ensure(tcsetattr(pty_slave, TCSANOW, &pty_slave_attr) != -1, "Couldn't set pty_slave attrs");

    return pty_slave;
}


void service_epoll_events(int epoll_fd, int server_socket) {
    int MAX_EPOLL_EVENTS = 20;
    epoll_event events[MAX_EPOLL_EVENTS];

    while(1) {
        int events_happened = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

        for (size_t i = 0; i < events_happened; i++) {
            if (events[i].data.fd == server_socket) {
                int conn_sock = accept_connection(server_socket);

                int pty_master = open_and_setup_pty_master();

                connection *conn_pty = new connection(conn_sock, pty_master, epoll_fd);
                connection *pty_conn = new connection(pty_master, conn_sock, epoll_fd);

                add_conn_pty_to_epoll(epoll_fd, conn_pty);
                add_pty_conn_to_epoll(epoll_fd, pty_conn);

                int pid = fork();
                ensure(pid != -1, "Couldn't fork to run sh");
                if (pid == 0) {
                    ensure(setsid() != -1, "Coundn't set new sid for sh");

                    int pty_slave = open_and_setup_pty_slave(pty_master);

                    dup2(pty_slave, STDIN_FILENO);
                    dup2(pty_slave, STDOUT_FILENO);
                    dup2(pty_slave, STDERR_FILENO);

                    execl("/bin/sh", "sh", NULL);
                }
            }
            else {
                connection *conn = (connection *)(events[i].data.ptr);
                if (events[i].events & EPOLLRDHUP) {
                    delete conn;
                }
                else {
                    conn->send();
                }
            }
        }
    }
}


int main(int argc, char **argv) {
    ensure_perror(argc == 2, "Usage: rshd <port_number>");

    int listenning_port = process_arg(argv[1]);

    become_daemon();

    openlog("rshd", LOG_PID | LOG_CONS, LOG_DAEMON);

    int server_socket = get_server_socket(listenning_port);
    listen(server_socket, 7);

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    add_server_socket_to_epoll(epoll_fd, server_socket);

    service_epoll_events(epoll_fd, server_socket);

    return 0;
}
