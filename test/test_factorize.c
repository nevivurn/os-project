#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

char* name;
long long current_time_ms() {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    return start.tv_sec * 1000 + start.tv_nsec / 1000000;
}
int arr[50];
void factorize(int n) {
    long long cur_diff = current_time_ms();
    int initial_n = n, cnt = 0;
    for(int i=2; i<=initial_n; i++) {
        if(n % i == 0) {
            n /= i;
            arr[cnt++] = i;
            i--;
        }
    }
    char output_str[1000];
    sprintf(output_str, "%s: At time %lld: %d = ", name, cur_diff, initial_n);
    for(int i=0; i<cnt; i++) {
        char tmp_str[100];
        sprintf(tmp_str, "%d", arr[i]);
        strcat(output_str, tmp_str);
        if(i != cnt-1) strcat(output_str, " * ");
    }
    printf("%s\n", output_str);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        name = "test";
    } else {
        name = argv[1];
    }
    int start_num = 5000000;

    while (true) {
        factorize(start_num);
        start_num++;
    }

    return 0;
}