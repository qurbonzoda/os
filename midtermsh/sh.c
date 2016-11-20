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

// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or >
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int safeClose(int);
int indexOf(int, int[], int);

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.

pid_t pids[1000];
int pidsLen = 0;

void
runcmd(struct cmd *cmd, int inFD, int outFD, int closingInFD, int closingOutFD)
{
/*
  //log
  fprintf(stderr, "runcmd in: %d, %d\n", inFD, outFD);
*/
  int p[2];
  struct execcmd *ecmd;
  struct pipecmd *pcmd;

  if(cmd == 0)
    return;

  pid_t pid;

  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    break;

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0) {
      break;
    }

    pid = fork1();
    if (pid == 0) {
      if (inFD != closingInFD) safeClose(closingInFD);
      if (outFD != closingOutFD) safeClose(closingOutFD);

      // setup in/out
      dup2(inFD, STDIN_FILENO);
      dup2(outFD, STDOUT_FILENO);

      //log
      fprintf(stderr, "forked process: %d with parent process: %d\nin: %d, out: %d\nargv:\n", getpid(), getppid(), inFD, outFD);
      for (int i = 0; i < 10; i++) {
        fprintf(stderr, "%s ", ecmd->argv[i]);
      } fprintf(stderr, "\n");

      if (execvp(ecmd->argv[0], ecmd->argv) == -1) {
        fprintf(stderr, "error occured while execvp\n");
        exit(-1);
      }
    }
    else {
      pids[pidsLen++] = pid;
    }

    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;

    if (pipe(p) == -1) {
        fprintf(stderr, "error occured while open\n");
        break;
    }

    runcmd(pcmd->left, inFD, p[1], p[0], outFD);
    runcmd(pcmd->right, p[0], outFD, inFD, p[1]);

    safeClose(p[0]);
    safeClose(p[1]);

    break;
  }
}

int read_all(int fd, char *buf, int buf_size) {
    int haveRead = 0;
    while (buf_size > haveRead) {
        int r = read(fd, buf + haveRead, buf_size - haveRead);
        haveRead += r;
        if (r == 0 || strchr(buf, '\n') != NULL) { return haveRead; }
    }
    return haveRead;
}

void write_all(int fd, char *buf, int buf_size) {
    int written = 0;
    while(buf_size > written) {
        int wrote = write(fd, buf + written, buf_size - written);
        written += wrote;
    }
}

int
getcmd(char *buf, int nbuf)
{
    // echo -ne "cat\nhello" | ./sh
  if (isatty(STDIN_FILENO))
    write(STDOUT_FILENO, "$\n", 2);

  memset(buf, 0, nbuf);
  int read = read_all(STDIN_FILENO, buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;

  return read;
}


void handler(int signo) {
  for (int i = 0; i < pidsLen; i++) {
		kill(pids[i], signo);
  }
}

int
main(void)
{

  struct sigaction sigact;

  sigact.sa_handler = *handler;

  sigset_t block_mask;
  sigfillset(&block_mask);

	sigact.sa_mask = block_mask; //block other signals while handling current signal

  for (int i = 1; i < 32; i++) {
    sigaction(i, &sigact, NULL);
  }

  int BUF_SIZE = 256;
  char buf[BUF_SIZE];

  int fd, r;

  // Read and run input commands.
  int curRead = 0, already = 0;
  while(1){
    curRead = getcmd(buf + already, BUF_SIZE - already);

    if (curRead <= 0) {
        if (already == 0) {
            break;
        }
        curRead = already;
    } else {
        curRead += already;
    }
    already = 0;

    printf("%d\n", curRead);

    pidsLen = 0;

    int inFD = STDIN_FILENO;

    char *nl = strchr(buf, '\n');
    if (nl != NULL && nl - buf != curRead - 1) {
        int pos = nl - buf;
        buf[pos] = '\0';
        pos++;

        int p[2];
        if (pipe(p) == -1) {
            fprintf(stderr, "error occured while pipe\n");
            break;
        }

        printf("[%s]\n[%s]\n", buf, buf + pos);

        write_all(p[1], buf + pos, curRead - pos);
        close(p[1]);

        inFD = p[0];
    }

    runcmd(parsecmd(buf), inFD, STDOUT_FILENO, STDIN_FILENO, STDOUT_FILENO);

    for (int i = 0; i < pidsLen; i++) {
        int pid = wait(&r);
        printf("finished pid: %d\n", pid);
    }

    if (inFD != STDIN_FILENO) {
        already = 0;
        while (BUF_SIZE > already) {
            int r = read(inFD, buf + already, BUF_SIZE - already);
            already += r;
            if (r <= 0) { break; }
        }
        close(inFD);
    }
  }
  exit(0);
}

int
safeClose(int fd)
{
  if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    return close(fd);
  }
  return -1;
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
