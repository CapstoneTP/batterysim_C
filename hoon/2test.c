#include "all_headers.h"

#define RANDOM_PERCENT 20

int main(){
    while(1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        srand(now.tv_sec * now.tv_usec);
        int random = rand() % RANDOM_PERCENT;
        printf("%d", random);
        sleep(1);
    }
    return 0;
}