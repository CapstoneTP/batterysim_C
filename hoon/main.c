#include "all_headers.h"

/*================================================================
main.c
This program is a CAN packet sender which updates BMS status live-on-time.

Parameters:
-For each cells..
    - batterytemp
    - batteryvoltage
    - batterycurrent
    - batterySOH
==================================================================*/

//input_thread
#define MAX_STRUCTS 9
//CAN_sender_thread
#define INTERFACENAME "vcan0"
#define SLEEPTIME 100000  //500ms
//print_screen_thread
#define BAR_WIDTH 50        // MAX-length of loading bar
#define LOAD_TIME 100       // load time (unit: ms)
//set cursor position

pthread_mutex_t lock; // mutex to use structure-located-mem

typedef struct {
    uint32_t id;      // CAN ID
    uint8_t data[8];  // CAN data
    uint8_t len;      // data length
} CAN_Message;

int ifinput = 0;
int modified_index = 0;
int modified_value = 0;


void print_battery_bar(int soc){                // soc stands on 0x626, BMS_SOC_t
    int bar_length = (soc * BAR_WIDTH) / 100;
    int i;
    
    printf("\rBattery: [");
    for (i = 0; i < bar_length; i++) {
        printf("=");
    }
    for (; i < BAR_WIDTH; i++) {
        printf(" ");
    }
    printf("] %d%%", soc);
    fflush(stdout);
}

void print_temp(){
    pthread_mutex_lock(&lock);
    int temp1 = battery[0].batterytemp;
    int temp2 = battery[1].batterytemp;
    pthread_mutex_unlock(&lock);

    printf("  Temperature: [C1:%d°C] [C2:%d°C]", temp1, temp2);
}

void print_logo() {
    const char *logo =
        "██████╗ ███╗   ███╗███████╗\n"
        "██╔══██╗████╗ ████║██╔════╝\n"
        "██████╔╝██╔████╔██║███████╗\n"
        "██╔══██╗██║╚██╔╝██║╚════██║\n"
        "██████╔╝██║ ╚═╝ ██║███████║\n"
        "╚═════╝ ╚═╝     ╚═╝╚══════╝\n"
        "                           \n"
        "███████╗██╗███╗   ███╗     \n"
        "██╔════╝██║████╗ ████║     \n"
        "███████╗██║██╔████╔██║     \n"
        "╚════██║██║██║╚██╔╝██║     \n"
        "███████║██║██║ ╚═╝ ██║     \n"
        "╚══════╝╚═╝╚═╝     ╚═╝_ver 2.1   \n"
        "                           \n";

    printf("%s", logo);
}




CAN_Message can_msgs[MAX_STRUCTS] = {
    {0x620, {0}, 8},        //bms_company_info
    {0x621, {0}, 8},        //vin_car_info
    {0x622, {0}, 6},        //bms_status
    {0x623, {0}, 6},        //bms_battery_info
    {0x624, {0}, 6},        //bms_charge_current_limits
    {0x626, {0}, 6},        //bms_soc
    {0x627, {0}, 6},        //bms_temperature
    {0x628, {0}, 6},        //bms_resistance
    {0x629, {0}, 8}         //bms_dc_charging
};

// User defiend function
void refresh_CAN_container() {
    // Copy bms_structure into can sender
    // mutex already locked before calling refresh_CAN_container
    memcpy(can_msgs[0].data, &bms_company_info, 8);
    memcpy(can_msgs[1].data, &vin_car_info, 8);
    memcpy(can_msgs[2].data, &bms_status, 6);
    memcpy(can_msgs[3].data, &bms_battery_info, 6);
    memcpy(can_msgs[4].data, &bms_charge_current_limits, 6);
    memcpy(can_msgs[5].data, &bms_soc, 6);
    memcpy(can_msgs[6].data, &bms_temperature, 6);
    memcpy(can_msgs[7].data, &bms_resistance, 6);
    memcpy(can_msgs[8].data, &bms_dc_charging, 8);
}

// User input thread    ||fix CAN data belongs to user input
void *input_thread(void *arg) {
    char key_input = 0;
    int invalid_input = 0;
    while(1) {
        key_input = getchar();
        pthread_mutex_lock(&lock);
        switch(key_input) {
            case 'a':
                if (battery[0].batterytemp > 0) battery[0].batterytemp--;
                break;
            case 's':
                if (battery[0].batterytemp < 100) battery[0].batterytemp++;
                break;
            case 'd':
                if (bms_soc.SOC > 0) bms_soc.SOC--;
                break;
            case 'f':
                if (bms_soc.SOC < 100) bms_soc.SOC++;
                break;
            default:
                invalid_input = 1;
        }
        ifinput = 1;
        pthread_mutex_unlock(&lock);
    }
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
            refresh_CAN_container();
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
    printf(CLEAR_SCREEN);
    printf(CURSOR_HIDE);
    printf(SET_CURSOR_UL);
    print_logo();
    while(1) {
        pthread_mutex_lock(&lock);
        int local_ifinput = ifinput;
        int soc = bms_soc.SOC;
        ifinput = 0;
        pthread_mutex_unlock(&lock);
        print_battery_bar(soc);
        print_temp();

        usleep(100000);
    }

}

int main() {
    printf(CLEAR_SCREEN);              //clear whole screen


    // Get input without buffer ('\n')
    struct termios newt, oldt;
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    // Apply new settings immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

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

}