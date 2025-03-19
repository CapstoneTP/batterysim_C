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

// 전역 구조체 배열 (CAN 데이터 저장)
#define MAX_STRUCTS 5

typedef struct {
    uint32_t id;      // CAN ID
    uint8_t data[8];  // CAN 데이터
    uint8_t len;      // 데이터 길이
} CAN_Message;

typedef struct __attribute__((packed)) {
    uint8_t CompanyName[8]; // Assumed to be an ASCII string (not null-terminated)
} BMS_Company_Info_t;

BMS_Company_Info_t bms_company_info_t[MAX_STRUCT] = {
    {00, 00, 00, 00, 00, 00, 00, 00}
};


CAN_Message can_msgs[MAX_STRUCTS] = {
    {0x100, {0}, 8},
    {0x200, {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18}, 8},
    {0x300, {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}, 8},
    {0x400, {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38}, 8},
    {0x500, {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48}, 8}
};

pthread_mutex_t lock; // 구조체 접근을 위한 뮤텍스

// 사용자 입력을 받아 CAN 데이터 수정하는 스레드
void *input_thread(void *arg) {
    int index, value;
    
    while (1) {
        printf("수정할 구조체 인덱스 (0~%d)와 변경할 첫 번째 바이트 값 입력: ", MAX_STRUCTS - 1);
        printf("\n press A to increase index0[0]: ");
        if (scanf("%d %x", &index, &value) == 2) {
            if (index >= 0 && index < MAX_STRUCTS) {
                pthread_mutex_lock(&lock);
                can_msgs[index].data[0] = (uint8_t)value; // 첫 번째 바이트 값 변경
                pthread_mutex_unlock(&lock);
                printf("구조체 %d 수정 완료: 첫 번째 바이트 -> 0x%02X\n", index, value);
            } else {
                printf("잘못된 인덱스 입력\n");
            }
        } else {
            if (index == "A") {
                pthread_mutex_lock(&lock);
                bms_company_info_t.Company
            }
            
            while (getchar() != '\n'); // 버퍼 클리어
        }
    }
    return NULL;
}

// CAN 메시지를 전송하는 스레드
void *can_sender_thread(void *arg) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // CAN 소켓 생성
    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("소켓 생성 실패");
        return NULL;
    }

    strcpy(ifr.ifr_name, "vcan0");  // 가상 CAN 인터페이스 사용
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

    // CAN 패킷 전송 루프
    while (1) {
        for (int i = 0; i < MAX_STRUCTS; i++) {
            pthread_mutex_lock(&lock);

            // bms_company_info_t 데이터를 can_msgs[0].data에 복사
            memcpy(can_msgs[0].data, bms_company_info_t[0].CompanyName, 8);

            frame.can_id = can_msgs[i].id;
            frame.can_dlc = can_msgs[i].len;
            memcpy(frame.data, can_msgs[i].data, frame.can_dlc);
            pthread_mutex_unlock(&lock);

            if (write(sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                perror("CAN 패킷 전송 실패");
            }
            usleep(500000); // 500ms 대기
        }
    }
    
    close(sock);
    return NULL;
}

int main() {
    pthread_t tid1, tid2;
    
    pthread_mutex_init(&lock, NULL);

    
    
    // 입력 및 CAN 송신 스레드 실행
    pthread_create(&tid1, NULL, input_thread, NULL);
    pthread_create(&tid2, NULL, can_sender_thread, NULL);

    // 메인 스레드는 두 개의 스레드를 기다림
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    pthread_mutex_destroy(&lock);
    
    return 0;
}