#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <math.h>

int main(int argc, char *argv[]) {
    if(argc<2){
        printf("Unable to execute\n");
        return 1;
    }
    char*lastarg = argv[argc - 1];
    unsigned long n = atof(lastarg);
    n = n * n; 
    if(argc == 2) {
        printf("%lu\n",n);
        return 0;
    }
    else if(argc > 2) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Unable to execute");
            return 1;
        } 
        if (pid == 0) { 
            char nstr[15];
            sprintf(nstr,"%lu",n);
            argv[argc-1]=nstr;
            execv(argv[1], argv+1);
            perror("Unable to execute");
            return 1;
        }
        else {
            wait(NULL);
        }
    }
    return 0;
}
