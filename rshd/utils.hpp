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
#include <syslog.h>

void ensure(bool is_true, const char *err_msg, const char *debug_msg) {
    if (!is_true) {
        syslog(LOG_ERR, "%s, desciption: %s\n", err_msg, strerror(errno));
        exit(-1);
    } else if (debug_msg) {
        syslog(LOG_DEBUG, "%s\n", debug_msg);
    }
}
void ensure(bool is_true, const char *msg) {
    ensure(is_true, msg, NULL);
}

void ensure_perror(bool is_true, const char *err_msg, const char *print_msg) {
    if (!is_true) {
        perror(err_msg);
        exit(-1);
    } else if (print_msg) {
        printf("%s\n", print_msg);
    }
}
void ensure_perror(bool is_true, const char *msg) {
    ensure_perror(is_true, msg, NULL);
}

void write_all(int fd, const char *buf, int len) {
ensure_perror(true, "", "gonna write all");
    int written = 0;
    while(len - written != 0) {
        int wrote = write(fd, buf + written, len - written);
        ensure(wrote != -1, "Coundn't write");
        written += wrote;
    }
    ensure_perror(true, "", "wrote all");
}
