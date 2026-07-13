#include "kernel/types.h"
#include "user/user.h"

static void
relay(int leftfd)
{
  int prime;

  if(read(leftfd, &prime, sizeof(prime)) != sizeof(prime)){
    close(leftfd);
    exit(0);
  }

  printf("prime %d\n", prime);

  int pipefd[2];
  if(pipe(pipefd) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "primes: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(pipefd[1]);
    close(leftfd);
    relay(pipefd[0]);
  }

  close(pipefd[0]);
  int n;
  while(read(leftfd, &n, sizeof(n)) == sizeof(n)){
    if(n % prime != 0)
      write(pipefd[1], &n, sizeof(n));
  }
  close(leftfd);
  close(pipefd[1]);
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pipefd[2];

  if(pipe(pipefd) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "primes: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(pipefd[1]);
    relay(pipefd[0]);
  }

  close(pipefd[0]);
  for(int n = 2; n <= 35; n++)
    write(pipefd[1], &n, sizeof(n));
  close(pipefd[1]);
  wait(0);
  exit(0);
}
