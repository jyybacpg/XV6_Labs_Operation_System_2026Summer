#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int to_child[2];
  int to_parent[2];
  char ch = 'x';

  if(pipe(to_child) < 0 || pipe(to_parent) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(to_child[1]);
    close(to_parent[0]);
    if(read(to_child[0], &ch, 1) != 1)
      exit(1);
    printf("%d: received ping\n", getpid());
    if(write(to_parent[1], &ch, 1) != 1)
      exit(1);
    close(to_child[0]);
    close(to_parent[1]);
    exit(0);
  }

  close(to_child[0]);
  close(to_parent[1]);
  if(write(to_child[1], &ch, 1) != 1)
    exit(1);
  if(read(to_parent[0], &ch, 1) != 1)
    exit(1);
  printf("%d: received pong\n", getpid());
  close(to_child[1]);
  close(to_parent[0]);
  wait(0);
  exit(0);
}
