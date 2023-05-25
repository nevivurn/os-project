#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "syscalls.h"

/*
1. Run this program:  `./test_main 20 > test.log`
2. Run analysis: `grep -P 'P\d+: At time \d+: ' test.log | awk '{print $1}' | sort | uniq -c | sort -nr`
Example output:
  14939 P20:
  14198 P19:
  13388 P18:
  12652 P17:
  11927 P16:
  11197 P15:
  10473 P14:
   9747 P13:
   8932 P12:
   8201 P11:
   7467 P10:
   6715 P9:
   5968 P8:
   5218 P7:
   4479 P6:
   3720 P5:
   2977 P4:
   2236 P3:
   1410 P2:
    662 P1:
*/
pid_t pid1, pid2;
int pid_arr[20], max_weight;

void handle_sigint(int sig) {
    for(int i=0; i<max_weight; i++) {
        kill(pid_arr[i], SIGINT);
    }
    exit(0);
}
int main(int argc, char** argv) {
    // get max weight as input
    if(argc < 2) {
        printf("Usage: ./test_main <max_weight>\n");
        exit(EXIT_FAILURE);
    }
    max_weight = atoi(argv[1]);
    signal(SIGINT, handle_sigint);
    for(int weight=1; weight<=max_weight; weight++) {
        printf("create process with weight %d\n", weight); 
        char weight_str[10];
        sprintf(weight_str, "%d", weight);
        pid_t pid = fork();
        if (pid == 0) {
            char name_str[10];
            sprintf(name_str, "P%d", weight);
            execl("./test_factorize", "test_factorize", name_str, NULL);
        } else { 
            pid_arr[weight-1] = pid;
            long ret = syscall(SYS_sched_setweight, pid, weight);
            if (ret < 0) {
                perror("sched_setweight");
                exit(EXIT_FAILURE);
            }
        }
    }

    int status;
    for(int i=0; i<max_weight; i++) {
        waitpid(pid_arr[i], &status, 0);
    }

    return 0;
}