
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
#include "all_headers.h"

/*================================================================
ToDoLiSt
- [X] if this option selected, means this code works well
    - [ ] if this option selected, means this code need fix, check '<<- need fix'
- [X] erase '<<- delete'
- [x] change ifcharge into BMS_Status_t;<<Status
    -[x]debugging
=================================================================*/

#define VERSION "0.6"

//CANSenderThread
#define MAX_STRUCTS 9
//CAN_sender_thread
#define INTERVAL_TIME 300000  //300ms
#define SLEEPTIME 100000  //100ms
//print_screen_thread
#define BAR_WIDTH 50        // MAX-length of loading bar
#define LOAD_TIME 100       // load time (unit: ms)
//charging_batterypack_thread
#define RANDOM_PERCENT 10

//handle arrow key input
#define LEFT 'D'
#define RIGHT 'C'
#define UP 'A'
#define DOWN 'B'

pthread_mutex_t lock; // mutex to use structure-located-mem

typedef struct {
    uint32_t id;      // CAN ID
    uint8_t data[8];  // CAN data
    uint8_t len;      // data length
} CAN_Message;

int ifrunning = 1;
int input_mode = 0;     //which value will be change
int ifvoltageerror = 0; //check if battery voltage value is in range
int g_iftempfan = 0;      //if temp fan works

/*================================================================
functions for calculate SOC
=================================================================*/

// void Init_Battery(){
//     double local_Capacity1c = 0;
//     for(int i=0; i<BATTERY_CELLS; i++){
//         battery[i].SOC = SocFromOcv(charge_ocv[0]);
//         battery[i].V1 = 0;
//         battery[i].Charge_Current = -0.41;
//         battery[i].Capacity = 4.07611;
//         local_Capacity1c = (battery[i].Capacity * 3600) / 100;
//         battery[i].R0 = 0.00005884314;
//         battery[i].R1 = 0.01145801322;
//         battery[i].C1 = 4846.080679;
//         battery[i].Voltage_terminal = battery[i].Charge_Current * battery[i].R1 * (1 - exp(-DELTA_TIME / (battery[i].R1 * battery[i].C1)));
//         battery[i].Temperature = 25;
//         estimate[i].SOC = battery[i].SOC;
//         estimate[i].V1 = battery[i].V1;
//         estimate[i].Voltage_terminal = 0;
//     }
// }

// void Update_Temperature(){
//     double local_airtemp = 0.0; //Air temperature
//     double internal_heat[BATTERY_CELLS];
//     double heater_on, cooler_on, total_heat[BATTERY_CELLS];
//     for(int i=0; i<BATTERY_CELLS; i++){
//         internal_heat[i] = battery[i].R0 * pow(battery[i].Charge_Current, 2);
//         heater_on = (battery[i].Temperature < HEATER_ON_TEMP)? HEAT_COOL_POWER : 0;
//         cooler_on = (battery[i].Temperature > COOLER_ON_TEMP)? HEAT_COOL_POWER : 0;
//         total_heat[i] = internal_heat[i] + heater_on - cooler_on;
//         battery[i].Temperature += DELTA_TIME / 200 * (total_heat[i] - (battery[i].Temperature - local_airtemp) / 3);
//     }
// }

// void Update_Resistance(){
//     const double local_R0_reference = 0.00005884314;
//     const double local_R1_reference = 0.01145801322;
//     const double local_coeff = 0.003;
//     for(int i=0; i<BATTERY_CELLS; i++){
//         battery[i].R0 = local_R0_reference * (1 + local_coeff * (battery[i].Temperature - 25));
//         battery[i].R1 = local_R1_reference * (1 + local_coeff * (battery[i].Temperature - 25));
//     }
// }

// void SimulateTerminalVoltage(){
//     for(int i=0; i<BATTERY_CELLS; i++){
//         battery[i].SOC -= COULOMIC_EFFICIENCY * DELTA_TIME / ((battery[i].Capacity * 3600) / 100) * battery[i].Charge_Current;
//         if(battery[i].SOC < 0) battery[i].SOC = 0;
//         if(battery[i].SOC > 100) battery[i].SOC = 100;
//         battery[i].V1 = battery[i].V1 * exp(-DELTA_TIME / (battery[i].R1 * battery[i].C1)) + battery[i].R1 * (1 - exp(-DELTA_TIME / (battery[i].R1 * battery[i].C1))) * battery[i].Charge_Current;
//         battery[i].Voltage_terminal = OcvFromSoc(battery[i].SOC) - battery[i].V1 - battery[i].R0 * battery[i].Charge_Current;
//     }
// }

/*================================================================
battery[
=================================================================*/

void EKFpredict(int k){
    double local_nconstant = exp(-DELTA_TIME / (cell_data[k].R1 * cell_data[k].C1));
    //cell_data[k].SOC -> SOC Empty error
    estimate[k].SOC = SocFromOcv(cell_data[k].voltage) - COULOMBIC_EFFICIENCY * DELTA_TIME / ((cell_data[k].capacity * 3600) / 100) * cell_data[k].charge_current;
    estimate[k].V1 = local_nconstant * battery[k].voltage_delay + cell_data[k].R1 * (1 - local_nconstant) * cell_data[k].charge_current;
}

void ComputeJacobianH(int k, double *local_H){
    double local_soc_high  = estimate[k].SOC + 0.05;
    double local_soc_low   = estimate[k].SOC - 0.05;
    if(local_soc_high > 100) local_soc_high = 100;
    if(local_soc_low < 0)    local_soc_low = 0;
    local_H[0] = (OcvFromSoc(local_soc_high) - OcvFromSoc(local_soc_low)) / (local_soc_high - local_soc_low);
    local_H[1] = -1;
    if(fabs(local_H[0]) < 1e-4f) local_H[0] = (local_H[0] >= 0)? 1e-4f : -1e-4f;
}

void SOCEKF(){
    double local_FP[2][2], local_Pp[2][2], local_H[2], local_HP[2], local_y, local_I_KH[2][2],local_error[2][2];                                    // ToDoLiSt[hihee]: local_Pp
    for(int i=0; i<BATTERY_CELLS; i++){
        if(!battery_state[i].init){
            battery_state[i].F[0][0] = 1.0; battery_state[i].F[0][1] = 0.0;
            battery_state[i].F[1][0] = 0.0; battery_state[i].F[1][1] = exp(-DELTA_TIME / (cell_data[i].R1 * cell_data[i].C1));
            battery_state[i].Q[0][0] = 0.0000001; battery_state[i].Q[0][1] = 0.0;
            battery_state[i].Q[1][0] = 0.0; battery_state[i].Q[1][1] = 0.0000001;
            battery_state[i].R = 500.0;
            battery_state[i].P[0][0] = 3000.0; battery_state[i].P[0][1] = 0.0;
            battery_state[i].P[1][0] = 0.0; battery_state[i].P[1][1] = 3000.0;
            battery_state[i].init = 1;
        }
        EKFpredict(i);
        /*
            EKF formula
            K = P^- * H^T * (H * P^- * H^T + R)^-1
            H = [d(Vt) / d(SOC), d(Vt) / d(V1)]
            Compute F * P array matrix multiplication
        */
        for(int k=0; k<2; ++k) for(int j=0; j<2; ++j){
            local_FP[k][j] = battery_state[i].F[k][0] * battery_state[i].P[0][j] + battery_state[i].F[k][1] * battery_state[i].P[1][j];
        }
        //Compute Pp -> F * P * F^T + Q (T meaning -> [0][2] makes -> [2][0] matrix)
        for(int k=0; k<2; ++k) for(int j=0; j<2; ++j){
            battery_state[i].Pp[k][j] = local_FP[k][j] * battery_state[i].F[j][0] + local_FP[k][1] * battery_state[i].F[j][1] + battery_state[i].Q[k][j];
        }
        //Linearize nonlinearity through gradient
        ComputeJacobianH(i, local_H);
        //Compute local_H * Pp
        local_HP[0] = local_H[0] * battery_state[i].Pp[0][0] + local_H[1] * battery_state[i].Pp[1][0];
        local_HP[1] = local_H[0] * battery_state[i].Pp[0][1] + local_H[1] * battery_state[i].Pp[1][1];
        //Residual
        estimate[i].Voltage_terminal = OcvFromSoc(estimate[i].SOC) - battery[i].voltage_delay - cell_data[i].R0 * cell_data[i].charge_current;
        local_y = cell_data[i].voltage - estimate[i].Voltage_terminal; //local_y is residual
        // EKF update step
        double S = local_H[0] * local_HP[0] + local_H[1] * local_HP[1] + battery_state[i].R;
        double K[2];
        K[0] = local_HP[0] / S;
        K[1] = local_HP[1] / S;

        estimate[i].SOC += K[0] * local_y;
        estimate[i].V1  += K[1] * local_y;

        for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) local_I_KH[k][j] = (k == j ? 1 : 0) - K[k] * local_H[j];
        //Update P
        for(int k=0; k<2; ++k) for(int j=0; j<2; ++j){
            local_error[k][j] = local_I_KH[k][0] * battery_state[i].Pp[0][j] + local_I_KH[k][1] * battery_state[i].Pp[1][j];
        }
        memcpy(battery_state[i].P, local_error, sizeof(battery_state[i].P));
    }
}

void ChargeCurrentLimits(){
    double local_charge_current_cc = -1 * CELL_CAPACITY;
    double local_charge_current_min_cv = -0.05 * CELL_CAPACITY;
    double local_hystersis = 0.01;
    double local_voltage_control = 50;
    double local_ratio = 0;
    double local_charge_current_limits = local_charge_current_cc;
    for(int i=0; i<BATTERY_CELLS; i++){
        if(estimate[i].SOC > SOC_TAPER_START){
            local_ratio = (estimate[i].SOC - SOC_TAPER_START) / (SOC_TAPER_END - SOC_TAPER_START);
            if(local_ratio > 1) local_ratio = 1;
            local_charge_current_limits = local_charge_current_cc * (1 - 0.8 * local_ratio);
        }
        if(cell_data[i].voltage >= VOLTAGE_MAX - local_hystersis){
            local_charge_current_limits = local_charge_current_cc + local_voltage_control * (battery[i].voltage_terminal - VOLTAGE_MAX);
            if(local_charge_current_limits < local_charge_current_min_cv) local_charge_current_limits = local_charge_current_min_cv;
            if(local_charge_current_limits > 0.0) local_charge_current_limits = 0;
        }
        if(cell_data[i].Temperature < 0.0) local_charge_current_limits = 0.0;
        else if(cell_data[i].Temperature < 15.0) local_charge_current_cc *= 0.5;
        battery[i].charge_current = local_charge_current_limits;
    }
}


/*================================================================
functions for print screen
=================================================================*/

double ScaleVoltage(uint16_t raw) {
    return raw * 0.1;
}

uint16_t DeScaleVoltage(double voltage) {
    return (uint16_t)(voltage * 10.0f);
}

void InitBatteryArray() {
    for (int i = 0; i < BATTERY_CELLS; i++) {
        memcpy(&battery[i], &default_battery, sizeof(Battery_t));
    }
}

void PrintBatteryBar(int soc) {                // soc stands on 0x626, BMS_SOC_t
    int bar_length = (soc * BAR_WIDTH) / 100;
    int i;
    
    printf("Battery: [");
    for (i = 0; i < bar_length; i++) {
        printf("█");
    }
    for (; i < BAR_WIDTH; i++) {
        printf(" ");
    }
    printf("] %d%%  ", soc);
    if (ifvoltageerror) {
        printf(HIGHLIGHT"voltage or soc error"RESET"               \n");
    }
    else printf("                                 \n");
    fflush(stdout);
}

void PrintInputMode(int mode) {
    const char* items[] = {"air_temp", "C1temp", "C1voltage", "C2temp", "C2voltage"};
    const int num_items = sizeof(items) / sizeof(items[0]);

    for (int i = 0; i < num_items; i++) {
        if (i == mode) {
            printf(HIGHLIGHT "[%s]" RESET " ", items[i]);
        } else {
            printf("[%s] ", items[i]);
        }
    }
    printf("\n\n");
}

void PrintCell() {
    int temp[BATTERY_CELLS];
    double voltage[BATTERY_CELLS];
    pthread_mutex_lock(&lock);
    for (int i = 0; i < BATTERY_CELLS; i++) {
        temp[i] = battery[i].temp;
        //voltage[i] = battery[i].batteryvoltage;
        voltage[i] = battery[i].voltage_terminal;
    }
    int local_status = bms_status.Status;
    int local_air_temp = bms_temperature.AirTemp;
    int local_iftempfan = g_iftempfan;
    pthread_mutex_unlock(&lock);

    for (int i = 0; i < BATTERY_CELLS; i++) {                       //print battery cells data
        // temperature color
        const char* temp_color = RESET;
        if (temp[i] <= -10) temp_color = BLUE;
        else if (temp[i] <= 0) temp_color = YELLOW;
        else if (temp[i] >= 45) temp_color = RED;
        else if (temp[i] >= 13) temp_color = GREEN;

        // voltage color
        const char* volt_color = RESET;
        if (voltage[i] <= 2.5) volt_color = RED;
        else if (voltage[i] <= 2.8) volt_color = YELLOW;
        else if (voltage[i] >= 4.2) volt_color = RED;
        else if (voltage[i] >= 4.0) volt_color = GREEN;

        if ((i + 1) == bms_battery_info.MaxVoltageID) volt_color = MAXHIGHLIGHT;
        if ((i + 1) == bms_battery_info.MinVoltageID) volt_color = MINHIGHLIGHT;
        if ((i + 1) == bms_temperature.MaxTempID) temp_color = MAXHIGHLIGHT;
        if ((i + 1) == bms_temperature.MinTempID) temp_color = MINHIGHLIGHT;

        printf("[C%.3d:%s%4.3d°C%s, %s%.2fv%s] ", i + 1, temp_color, temp[i], RESET, volt_color, voltage[i], RESET);

        if ( (i + 1) % CELLS_IN_LINE == 0) printf("\n");
    }
    printf("\n\n[air_temp: %d]", local_air_temp);

    if (local_status) printf(GREEN "  [charging] " RESET);
    else printf(RED "  [not charging now]" RESET);
    if (local_iftempfan == 1) printf(BLUE "  [Cooling fan active]               " RESET);
    else if (local_iftempfan == 2) printf(RED "  [Heater fan active]                " RESET);
    else printf("  [fan not activate]               " RESET);
}

void PrintLogo(int option) {
    if (option == 0) {
        const char *logo =
            "██████╗ ███╗   ███╗███████╗\n"
            "██╔══██╗████╗ ████║██╔════╝      \n"
            "██████╔╝██╔████╔██║███████╗      \n"
            "██╔══██╗██║╚██╔╝██║╚════██║      \n"
            "██████╔╝██║ ╚═╝ ██║███████║      \n"
            "╚═════╝ ╚═╝     ╚═╝╚══════╝\n"
            "                           \n"
            "███████╗  ██╗  ███╗   ███╗     \n"
            "██╔════╝  ██║  ████╗ ████║     \n"
            "███████╗  ██║  ██╔████╔██║     \n"
            "╚════██║  ██║  ██║╚██╔╝██║     \n"
            "███████║  ██║  ██║ ╚═╝ ██║     \n"
            "╚══════╝  ╚═╝  ╚═╝     ╚═╝_ver "VERSION" \n"
            "                           \n";

        printf("%s", logo);
    }
    if (option == 1) {
        const char *logo =
        "██████╗ ███╗   ███╗███████╗\n"
        "██╔══██╗████╗ ████║██╔════╝        Use ' ← → ' arrow keys to select value you want to change\n"
        "██████╔╝██╔████╔██║███████╗        Use ' ↑ ↓ ' arrow keys to change value [increase or decrease]\n"
        "██╔══██╗██║╚██╔╝██║╚════██║        Press space to toggle charging\n"
        "██████╔╝██║ ╚═╝ ██║███████║        Press ESC twice to quit program\n"
        "╚═════╝ ╚═╝     ╚═╝╚══════╝\n"
        "                           \n"
        "███████╗  ██╗  ███╗   ███╗     \n"
        "██╔════╝  ██║  ████╗ ████║     \n"
        "███████╗  ██║  ██╔████╔██║     \n"
        "╚════██║  ██║  ██║╚██╔╝██║     \n"
        "███████║  ██║  ██║ ╚═╝ ██║     \n"
        "╚══════╝  ╚═╝  ╚═╝     ╚═╝_ver "VERSION" \n"
        "                           \n";

    printf("%s", logo);
    }
}


double GetCorrectVoltage(double battery_temp) {           // no mutex lock
    double correct = 0;
    if (battery_temp >= 45) correct = -0.02;
    else if (battery_temp >= 12 && battery_temp < 45) correct = 0;
    else if (battery_temp >= -10 && battery_temp < 12) correct = 0.02;
    else if (battery_temp < -10) correct = 0.04;
    return correct;
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

// User defined function
void RefreshCANContainer() {
    // Copy bms_structure into can sender
    // mutex already locked before calling RefreshCANContainer
    memcpy(can_msgs[0].data, &bms_company_info, 8);
    memcpy(can_msgs[1].data, &vin_car_info, 8);
    memcpy(can_msgs[2].data, &bms_status, 6);
    memcpy(can_msgs[3].data, &bms_battery_info, 6);
    memcpy(can_msgs[4].data, &bms_charge_current_limits, 6);
    memcpy(can_msgs[5].data, &bms_soc, 6);
    memcpy(can_msgs[6].data, &bms_temperature, 6);
    memcpy(can_msgs[7].data, &bms_resistance, 6);
    memcpy(can_msgs[8].data, &bms_dc_charging, 8);

    // get cell_data(sensored) from battery index

}

void ChangeValue(int mode, int ifup) {
    if (ifup) {
        switch(mode) {
            case 0:
                if (bms_temperature.AirTemp < 127) bms_temperature.AirTemp ++; break;
            case 1:
                if (battery[0].temp < 127) battery[0].temp++; break;
            case 2:
                if (battery[0].voltage_terminal < 9.0) battery[0].voltage_terminal += 0.1; break;
            case 3:
                if (battery[1].temp < 127) battery[1].temp++; break;
            case 4:
                if (battery[1].voltage_terminal < 9.0) battery[1].voltage_terminal += 0.1; break;
            default:
                break;
        }
    }
    else if (!ifup) {
        switch(mode) {
            case 0:
                if (bms_temperature.AirTemp > -127) bms_temperature.AirTemp --; break;
            case 1:
                if (battery[0].temp > -127) battery[0].temp--; break;
            case 2:
                if (battery[0].voltage_terminal > 5.5) battery[0].voltage_terminal -= 0.1; break;
            case 3:
                if (battery[1].temp > -127) battery[1].temp--; break;
            case 4:
                if (battery[1].voltage_terminal > 5.5) battery[1].voltage_terminal -= 0.1; break;
            default:
                break;
        }
    }
}


// get input and reset battery cells when program starts
void SimInitializer() {
    int soc, air_temp;
    printf("input SOC you want (0~100): ");
    scanf("%d", &soc);
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;

    printf("input air temp you want (℃): ");
    scanf("%d", &air_temp);
    if (air_temp < -40) air_temp = -40;
    if (air_temp > 127) air_temp = 127;

    pthread_mutex_lock(&lock);
    default_battery.voltage_terminal = OcvFromSoc(soc);
    bms_temperature.AirTemp = air_temp;
    pthread_mutex_unlock(&lock);
}

// User input thread    ||fix CAN data belongs to user input
void *input_thread(void *arg) {                                     //tid1
    char key_input = 0;
    int invalid_input = 0;
    while(ifrunning) {
        key_input = getchar();
        pthread_mutex_lock(&lock);
        switch(key_input) {
            case ' ':       //whiespace key pressed
                bms_status.Status =!bms_status.Status;
                break;
            case 'a':
                if (battery[0].temp > 0) battery[0].temp--;
                break;
            case 'A':
                if (battery[0].temp < 100) battery[0].temp++;
                break;
            case 'g':
                if (bms_soc.SOC > 0) bms_soc.SOC--;
                break;
            case 'G':
                if (bms_soc.SOC < 100) bms_soc.SOC++;
                break;
            case '\033': { // ESC || arrow key and...
                // Check if this is an arrow key sequence
                char next_char = getchar();
                if (next_char == '[') {
                    char arrow = getchar();
                    switch (arrow) {        //use ChangeValue()
                        case UP:   // Up arrow
                            ChangeValue(input_mode, 1); break;
                        case DOWN:   // Down arrow
                            ChangeValue(input_mode, 0); break;
                        case RIGHT: // Right arrow
                            if (input_mode < 4) input_mode++;
                            break;
                        case LEFT: // Left arrow
                            if (input_mode > 0) input_mode--;
                            break;
                        default:
                            invalid_input = 1;
                    }
                } else {
                    // simple ESC input
                    ifrunning = 0;
                    pthread_mutex_unlock(&lock);
                    return NULL;
                }
                break;
            }
            default:
                invalid_input = 1;
                break;
        }
        pthread_mutex_unlock(&lock);
    }
}

// CAN tx thread
void *can_sender_thread(void *arg) {                                        //tid2
    const char *interface_name = (const char *)arg;
    int tx_sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // Generate CAN socket
    if ((tx_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("소켓 생성 실패");
        return NULL;
    }

    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(tx_sock, SIOCGIFINDEX, &ifr) < 0) {
        printf(HIGHLIGHT);
        perror("\npress any key to continue\n인터페이스 인덱스 가져오기 실패");
        printf(RESET);
        close(tx_sock);
        pthread_mutex_lock(&lock);
        ifrunning = 0;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(tx_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("소켓 바인드 실패");
        close(tx_sock);
        return NULL;
    }

    // CAN packet tx loop
    while (ifrunning) {
        for (int i = 0; i < MAX_STRUCTS; i++) {
            pthread_mutex_lock(&lock);
            RefreshCANContainer();
            frame.can_id = can_msgs[i].id;
            frame.can_dlc = can_msgs[i].len;
            memcpy(frame.data, can_msgs[i].data, frame.can_dlc);
            pthread_mutex_unlock(&lock);

            if (write(tx_sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                perror("CAN 패킷 전송 실패");
            }
            usleep(SLEEPTIME);
        }
    }
    
    close(tx_sock);
    return NULL;
}

void *can_receiver_thread(void *arg) {              //tid10
    const char *interface_name = (const char *)arg;
    int rx_sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // Generate CAN socket
    if ((rx_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("소켓 생성 실패");
        return NULL;
    }

    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(rx_sock, SIOCGIFINDEX, &ifr) < 0) {
        printf(HIGHLIGHT);
        perror("\npress any key to continue\n인터페이스 인덱스 가져오기 실패");
        printf(RESET);
        close(rx_sock);
        pthread_mutex_lock(&lock);
        ifrunning = 0;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(rx_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("소켓 바인드 실패");
        close(rx_sock);
        return NULL;
    }

    while (ifrunning) {
        int nbytes = read(rx_sock, &frame, sizeof(struct can_frame));
        if (nbytes > 0) {
            if (frame.can_id == 0x010) {
                if (frame.data[0] == 0x01) {
                    pthread_mutex_lock(&lock);
                    bms_status.Status = 1;
                    pthread_mutex_unlock(&lock);
                }
                else if (frame.data[0] == 0x00) {
                    pthread_mutex_lock(&lock);
                    bms_status.Status = 0;
                    pthread_mutex_unlock(&lock);
                }
            }
            if (frame.can_id == 0x11) {
                if (frame.data[0] == 0x01) {
                    pthread_mutex_lock(&lock);
                    if (bms_status.Status == 0) InitBatteryArray();
                    pthread_mutex_unlock(&lock);
                }
            }
        } else if (nbytes < 0) {
            perror("CAN 수신 실패");
            break;
        }
    }
}


void *print_screen_thread(void *arg) {              //tid3
    printf(CLEAR_SCREEN);
    printf(CURSOR_HIDE);
    while(ifrunning) {
        printf(SET_CURSOR_UL);
        pthread_mutex_lock(&lock);
        int soc = bms_soc.SOC;
        int local_input_mode = input_mode;
        pthread_mutex_unlock(&lock);
        PrintLogo(1);
        PrintInputMode(local_input_mode);
        PrintBatteryBar(soc);
        PrintCell();

        usleep(100000);
    }
}

void *charge_batterypack_thread(void *arg) {            //tid4
    srand(time(NULL));
    while (ifrunning) {
        pthread_mutex_lock(&lock);
        int local_status = bms_status.Status;
        pthread_mutex_unlock(&lock);
        if (local_status) {
            usleep (300000);
            //choose random chance
            int random = 0;
            random = rand() % RANDOM_PERCENT;
            pthread_mutex_lock(&lock);
            //increase voltage
            for (int i = 0 ; i < BATTERY_CELLS; i++) {
                battery[i].voltage_terminal += 0.01;
            }
            //randomly increase temp for a few random cells
            for (int j = 0; j < BATTERY_CELLS; j++) {
                if ((rand() % RANDOM_PERCENT) == 0) {
                    battery[j].temp += 0.5;
                }
            }
            pthread_mutex_unlock(&lock);
        } else {
            usleep(100000);
        }
    }
}

// tid5
void *temp_batterypack_thread(void *arg) {
    while(ifrunning) {                                  //every logics work on runtime, always. (if there's any input or not)ㅋ
        double mintemp = 1e9;
        int mintempid = 0;
        double maxtemp = -1e9;
        int maxtempid = 0;
        double totaltemps = 0;
        usleep(500000);
        pthread_mutex_lock(&lock);
        double local_air_temp = bms_temperature.AirTemp;
        for (int i = 0; i < BATTERY_CELLS; i++) {
            if (mintemp > battery[i].temp) {
                mintemp = battery[i].temp;
                mintempid = i + 1;
            }
            if (maxtemp < battery[i].temp) {
                maxtemp = battery[i].temp;
                maxtempid = i + 1;
            }
            totaltemps += battery[i].temp;
        }
        bms_temperature.Temperature = (totaltemps / BATTERY_CELLS);     //get average temps
        bms_temperature.MaxTemp = maxtemp;
        bms_temperature.MaxTempID = maxtempid;
        bms_temperature.MinTemp = mintemp;
        bms_temperature.MinTempID = mintempid;
        pthread_mutex_unlock(&lock);
    }
}

void *voltage_batterypack_thread(void *arg) {                   //tid6
    while (ifrunning) {
        while(bms_status.Status) {
            pthread_mutex_lock(&lock);
            //Update_Temperature(); // Update temperature
            //Update_Resistance();  // Change resistance from temperature
            //SimulateTerminalVoltage();
            SOCEKF();
            ChargeCurrentLimits();
            pthread_mutex_unlock(&lock);
        }
    }
}

//tid8
void *battery_idle_thread(void *arg) {
    while (ifrunning) {
    usleep(INTERVAL_TIME);
    // temp idle
    double mintemp = 1e9;
    int mintempid = 0;
    double maxtemp = -1e9;
    int maxtempid = 0;
    double internal_heat = 0;
    double heater_power_w = 5.0;
    double cooler_power_w = 5.0;
    double local_terminal_voltage[BATTERY_CELLS];
    pthread_mutex_lock(&lock);
    for(int i=0; i<BATTERY_CELLS; i++) {
        // temperature && resistance
        // Calculate temp from air temp
        internal_heat = battery[i].R0 * pow(battery[i].charge_current, 2);
        double heater_power = (battery[i].temp <= 15)? heater_power_w : 0;
        double cooler_power = (battery[i].temp >= 35)? cooler_power_w : 0;
        double totalheat = internal_heat + heater_power - cooler_power;
        if(heater_power != 0) g_iftempfan = 2;
        else if(cooler_power != 0) g_iftempfan = 1;
        else g_iftempfan = 0;
        battery[i].temp += (1.0 / 200.0 * (totalheat - (battery[i].temp - bms_temperature.AirTemp) / 3.0));
        double diff_temperature = battery[i].temp - 25;

        // Calculate resistance from temp
        battery[i].R0 = 0.00005884314 * (1 + 0.003 * diff_temperature);
        battery[i].R1 = 0.01145801322 * (1 + 0.003 * diff_temperature);

        // simulator terminal voltage
        double tau = battery[i].R1 * battery[i].C1;
        double e = exp(-1 / tau);
        battery[i].voltage_delay = battery[i].voltage_delay * e + battery[i].R1 * (1. - e) * battery[i].charge_current;

        
        double ocv = OcvFromSoc(battery[i].SOC);
        battery[i].voltage_terminal = ocv - battery[i].voltage_delay - battery[i].R0 * battery[i].charge_current;
        // copy battery data to cell_data(sensor measured value)
        memcpy (&cell_data[i].Temperature, &battery[i], 7);
    }
    pthread_mutex_unlock(&lock);
    
    }

}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);  // turn off buffering for stdout, print screen instantly (without enter)

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <can_interface>\n", argv[0]);
        return 1;
    }

    printf(CLEAR_SCREEN);              // clear whole screen
    printf(SET_CURSOR_UL);             // set cursor UpLeft
    PrintLogo(0);
    printf("\n\n");
    SimInitializer();

    InitBatteryArray();
    printf("waiting for start .");
    usleep(1000000);
    printf("\rwaiting for start ..");
    usleep(300000);
    printf("\rwaiting for start ...");
    usleep(700000);

    // Get input without buffer ('\n')
    struct termios newt, oldt;
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    // Apply new settings immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    pthread_t tid1, tid2, tid3, tid4, tid5, tid6, tid7, tid8;
    

    pthread_mutex_init(&lock, NULL);
    
    // start CANSenderThread && CANtxThread
    pthread_create(&tid1, NULL, input_thread, NULL);
    pthread_create(&tid2, NULL, can_sender_thread, argv[1]);
    pthread_create(&tid3, NULL, can_receiver_thread, argv[1]);
    pthread_create(&tid4, NULL, print_screen_thread, NULL);
    pthread_create(&tid5, NULL, charge_batterypack_thread, NULL);
    pthread_create(&tid6, NULL, temp_batterypack_thread, NULL);
    pthread_create(&tid7, NULL, voltage_batterypack_thread, NULL);
    pthread_create(&tid8, NULL, battery_idle_thread, NULL);

    // Main Thread wait for both threads
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);
    pthread_join(tid5, NULL);
    pthread_join(tid6, NULL);
    pthread_join(tid7, NULL);
    pthread_join(tid8, NULL);

    pthread_mutex_destroy(&lock);
    printf(CURSOR_SHOW);
    printf("\n");

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}