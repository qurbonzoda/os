#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void handler(int signo, siginfo_t *siginfo, void *context) {
	printf("%s from %d\n", (signo == SIGUSR1 ? "SIGUSR1" : "SIGUSR2"), (int)(siginfo->si_pid));
	exit(0);
}

int main() {
  struct sigaction sigact;

  sigact.sa_sigaction = *handler;
  sigact.sa_flags |= SA_SIGINFO;

  sigset_t block_mask;
  sigfillset(&block_mask);

	sigact.sa_mask = block_mask;

	if (sigaction(SIGUSR1, &sigact, NULL) != 0 || sigaction(SIGUSR2, &sigact, NULL) != 0) {
      printf("%d error\n", errno);
     	return errno;
  }

  sleep(10);
	sigprocmask(SIG_BLOCK, &sigact.sa_mask, 0);

	printf("No signals were caught\n");
  return 0;
}
