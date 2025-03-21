#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "dbc.h"

/*================================================================
test.c
test for making double threads and get simultaneous programming
Press A to add value in index
=================================================================*/


//input_thread
#define MAX_STRUCTS 5
//CAN_sender_thread
#define INTERFACENAME "vcan0"
#define SLEEPTIME 500000  //500ms
//print_screen_thread
#define BAR_WIDTH 50        // 로딩 바의 전체 길이
#define LOAD_TIME 100       // 로딩 단위 시간 (단위: ms)

typedef struct {
    uint32_t id;      // CAN ID
    uint8_t data[8];  // CAN data
    uint8_t len;      // data length
} CAN_Message;

BMS_Company_Info_t bms_company_info_t = {
    {00, 00, 00, 00, 00, 00, 00, 00}
};


CAN_Message can_msgs[MAX_STRUCTS] = {
    {0x100, {0}, 8},
    {0x200, {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}, 8},
    {0x300, {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}, 8},
    {0x400, {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38}, 8},
    {0x500, {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48}, 8}
};

pthread_mutex_t lock; // mutex to use structure-located-memory

int ifinput = 0;

// User input thread    ||fix CAN data belongs to user input
void *input_thread(void *arg) {
    int index, value;
    
    while (1) {
        printf("수정할 구조체 인덱스 (0~%d)와 변경할 첫 번째 바이트 값 입력: ", MAX_STRUCTS - 1);
        printf("\n press A to increase index0[0]: ");
        if (scanf("%d %x", &index, &value) == 2) {
            pthread_mutex_lock(&lock);
            ifinput = 1;
            pthread_mutex_unlock(&lock);
            if (index >= 0 && index < MAX_STRUCTS) {
                pthread_mutex_lock(&lock);
                can_msgs[index].data[0] = (uint8_t)value;
                pthread_mutex_unlock(&lock);
                printf("구조체 %d 수정 완료: 첫 번째 바이트 -> 0x%02X\n", index, value);
            } else {
                if (index == 5) {
                    pthread_mutex_lock(&lock);
                    bms_company_info_t.CompanyName[0]++;
                    pthread_mutex_unlock(&lock);
                }
            }
        } else {
            printf("입력 오류");
            while (getchar() != '\n'); // buffer clear
        }
    }
    return NULL;
}

// CAN tx thread
void *can_sender_thread(void *arg) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // Generate CAN socket
    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("소켓 생성 실패");
        return NULL;
    }

    strcpy(ifr.ifr_name, INTERFACENAME);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("인터페이스 인덱스 가져오기 실패");
        close(sock);
        return NULL;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("소켓 바인드 실패");
        close(sock);
        return NULL;
    }

    // CAN packet tx loop
    while (1) {
        for (int i = 0; i < MAX_STRUCTS; i++) {
            pthread_mutex_lock(&lock);

            // Copy bms_company_info_t.CompnayName into can-msgs[0].data
            memcpy(can_msgs[0].data, bms_company_info_t.CompanyName, 8);

            frame.can_id = can_msgs[i].id;
            frame.can_dlc = can_msgs[i].len;
            memcpy(frame.data, can_msgs[i].data, frame.can_dlc);
            pthread_mutex_unlock(&lock);

            if (write(sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                perror("CAN 패킷 전송 실패");
            }
            usleep(SLEEPTIME);
        }
    }
    
    close(sock);
    return NULL;
}

void *print_screen_thread(void *arg) {
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
        printf("] %03d%%", (i * 100) / BAR_WIDTH);  // 진행률 표시
        fflush(stdout);
        if (i == BAR_WIDTH) i = 0;
    }

}

int main() {
    pthread_t tid1, tid2, tid3;
    
    pthread_mutex_init(&lock, NULL);

    
    
    // start InputThread && CANtxThread
    pthread_create(&tid1, NULL, input_thread, NULL);
    pthread_create(&tid2, NULL, can_sender_thread, NULL);
    pthread_create(&tid3, NULL, print_screen_thread, NULL);

    // Main Thread wait for both threads
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    pthread_mutex_destroy(&lock);
    
    return 0;
}