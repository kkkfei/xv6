#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char c;
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);

    int pid = fork();
    if(pid == 0) {
        //child

        read(p1[0], &c, 1);
        printf("%d: received ping\n", getpid());

        c = 2;
        write(p2[1], &c, 1);
    } else {
        //parent
        c = 1;
        write(p1[1], &c, 1);

        read(p2[0], &c, 1);
        printf("%d: received pong\n", getpid());
    }

    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]);

    exit(0);
}
