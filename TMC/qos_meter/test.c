#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

void func()
{
    printf("this is func\n");
}

int main()
{
    signal(SIGALRM, func); 
    alarm(2);

    while (1);
    return 0;
}
