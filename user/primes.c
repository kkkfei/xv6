#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void solve(int fd)
{
    int p, n;
    int pip[2];
    int sz = sizeof(int);
    if(read(fd, &p, sz) == sz) 
    {
        printf("prime %d\n", p);
    } else {
        return;
    }

    pipe(pip);
    int pid = fork();
    if(pid == 0)
    {
        //child
        close(pip[1]);
        solve(pip[0]);
        close(pip[0]);

    } else {
        
        close(pip[0]);
        while(read(fd, &n, sz) == sz) {
            if(n % p != 0)
            {
                write(pip[1], &n, sz);
            }
        }
        close(pip[1]);
        wait(0);
    }
 
}


int
main(int argc, char *argv[])
{
    int pip[2];

    pipe(pip);
    int pid = fork();
    if(pid == 0)
    {
        close(pip[1]);
        //child
        solve(pip[0]);
        close(pip[0]);

    } else {
                
        close(pip[0]);
        for(int i=2; i<=35; ++i)
        {
             write(pip[1], &i, sizeof(i));
        }
        close(pip[1]);
        wait(0);
    }
    exit(0);
}
