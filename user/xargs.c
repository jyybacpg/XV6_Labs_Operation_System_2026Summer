#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define LINELEN 512

static int
readline(char *buf, int n)
{
  int i = 0;
  char c;

  while(i + 1 < n){
    int r = read(0, &c, 1);
    if(r < 0)
      return -1;
    if(r == 0)
      break;
    if(c == '\n')
      break;
    buf[i++] = c;
  }
  buf[i] = 0;
  return i;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: xargs command [args...]\n");
    exit(1);
  }

  char line[LINELEN];
  char *args[MAXARG];

  while(readline(line, sizeof(line)) >= 0){
    if(line[0] == 0)
      break;

    int narg = 0;
    for(int i = 1; i < argc && narg < MAXARG - 1; i++)
      args[narg++] = argv[i];
    if(narg >= MAXARG - 1){
      fprintf(2, "xargs: too many arguments\n");
      exit(1);
    }
    args[narg++] = line;
    args[narg] = 0;

    int pid = fork();
    if(pid < 0){
      fprintf(2, "xargs: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec(args[0], args);
      fprintf(2, "xargs: exec %s failed\n", args[0]);
      exit(1);
    }
    wait(0);
  }

  exit(0);
}
