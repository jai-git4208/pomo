#include <stdio.h>
#include <time.h>
#include <unistd.h>

typedef enum {w, b} State;
#define wd (25 * 60)
#define bd (5 * 60)

void printTimer(State state, int sec){
    int min = sec / 60;
    int secs = sec % 60;
    const char *state_str = (state == w) ? "WORK" : "BREAK";
    printf("\r%s : %02d:%02d", state_str, min, secs);
    fflush(stdout);
}

int main(){
    State current = w;
    while(1){
        int duration = (current == w) ? wd : bd;

        for(int i = duration; i >= 0; i--){
            printTimer(current, i);
            sleep(1);

        }
        printf("\n");//new lien

        //state change
        if (current == w){
            printf("Break!\n");
            current = b;
        }
        else {
            printf("work time! \n");
            current = w;
        }
    }
    return 0;
}