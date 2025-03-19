#include <stdio.h>
#include <unistd.h>  // usleep()

#define BAR_WIDTH 50  // 로딩 바의 전체 길이
#define LOAD_TIME 100  // 로딩 단위 시간 (단위: ms)

int main() {
    int i;
    
    printf("Loading: [");  // 시작 텍스트
    for (i = 0; i < BAR_WIDTH; i++) {
        printf(" ");  // 초기 빈 공간
    }
    printf("] 0%%");  // 퍼센트 표시
    fflush(stdout);  // 즉시 출력

    for (i = 0; i <= BAR_WIDTH; i++) {
        usleep(LOAD_TIME * 1000);  // 지연 (ms 단위 변환)

        printf("\rLoading: [");  // 캐리지 리턴으로 줄 덮어쓰기
        int j;
        for (j = 0; j < i; j++) {
            printf("=");  // 채워진 부분
        }
        for (; j < BAR_WIDTH; j++) {
            printf(" ");  // 남은 빈 공간
        }
        printf("] %d%%", (i * 100) / BAR_WIDTH);  // 진행률 표시
        fflush(stdout);
    }

    printf("\nDone!\n");
    return 0;
}