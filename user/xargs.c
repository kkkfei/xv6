#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define BUFF_SIZE 256
 


int
main(int argc, char *argv[])
{
    if(argc < 2){
        fprintf(2, "find: need an argument!");
        exit(0);
    }

    char buff[BUFF_SIZE];
    int leftSize = sizeof(buff);
    int sz;
    char *p = buff;
    while( (sz = read(0, p, leftSize)) > 0)
    {
        p += sz;
        leftSize -= sz;
    }

    p = buff;
    char *start = buff;

    char* new_argv[16];
    memcpy(new_argv, argv+1, sizeof(char *) * (argc - 1));

    int startIdx = argc-1;
    int idx = startIdx;
    while(*p) {
        if(*p == ' ') {
            *p = 0;
            new_argv[idx++] = start;

            do {
                ++p;
            } while(*p == ' ');

            start = p;

        } else if(*p == '\n')
        {
            *p = 0;
            new_argv[idx++] = start;
            new_argv[idx] = 0;

            int pid = fork();
            if(pid == 0)
            {
                // printf("\nexec1: %s ", argv[1]);
                // for(int i=0; i<idx; ++i)
                // {
                //     printf(" %s ", new_argv[i]);
                // }
                // printf("\n");

                exec(argv[1], new_argv);
            } 
            wait(0);
            idx = startIdx;
            start = p+1;
        }
        ++p;
    }
 
    new_argv[idx++] = start;
    new_argv[idx] = 0;
    int pid = fork();
    if(pid == 0)
    {
        // printf("\nexec2: %s", argv[1]);
        // for(int i=0; i<idx; ++i)
        // {
        //     printf(" %s ", new_argv[i]);
        // }
        // printf("\n");

        exec(argv[1], new_argv);
    }
 


    free(new_argv);
    wait(0);
 
    exit(0);
}