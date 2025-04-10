#include "all_headers.h"
#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <termios.h>

// Global variables
pthread_mutex_t lock;
int ifrunning = 1;
int ifcharge = 0;
int input_mode = 0;     // Index for selected parameter adjustment
int ifvoltageerror = 0;
int iftempfan = 0;

#define MAX_STRUCTS 9
#define SLEEPTIME 100000 // 100ms delay
#define BAR_WIDTH 50
#define LOAD_TIME 100
#define RANDOM_PERCENT 10
#define VERSION "ncurses.1"

// CAN Message structure
typedef struct {
    uint32_t id;      // CAN ID
    uint8_t data[8];  // CAN data
    uint8_t len;      // Data length
} CAN_Message;

CAN_Message can_msgs[MAX_STRUCTS] = {
    {0x620, {0}, 8},
    {0x621, {0}, 8},
    {0x622, {0}, 6},
    {0x623, {0}, 6},
    {0x624, {0}, 6},
    {0x626, {0}, 6},
    {0x627, {0}, 6},
    {0x628, {0}, 6},
    {0x629, {0}, 8}
};

// Function to scale raw voltage value
double scale_voltage(uint16_t raw) {
    return raw * 0.1;
}

// Function to descale voltage value
uint16_t descale_voltage(double voltage) {
    return (uint16_t)(voltage * 10.0);
}

// Initialize battery array with default battery values
void init_battery_array() {
    for (int i = 0; i < BATTERY_CELLS; i++) {
        memcpy(&battery[i], &default_battery, sizeof(Battery_t));
    }
}

// Copy BMS structures to CAN messages container
void refresh_CAN_container() {
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

// Adjust values based on selected mode and increment/decrement direction
void change_value(int mode, int ifup) {
    if (ifup) {
        switch(mode) {
            case 0:
                if (bms_temperature.AirTemp < 127) bms_temperature.AirTemp++;
                break;
            case 1:
                if (battery[0].batterytemp < 127) battery[0].batterytemp++;
                break;
            case 2:
                if (battery[0].batteryvoltage < 9.0) battery[0].batteryvoltage += 0.1;
                break;
            case 3:
                if (battery[1].batterytemp < 127) battery[1].batterytemp++;
                break;
            case 4:
                if (battery[1].batteryvoltage < 5.5) battery[1].batteryvoltage += 0.1;
                break;
            default:
                break;
        }
    } else {
        switch(mode) {
            case 0:
                if (bms_temperature.AirTemp > -127) bms_temperature.AirTemp--;
                break;
            case 1:
                if (battery[0].batterytemp > -127) battery[0].batterytemp--;
                break;
            case 2:
                if (battery[0].batteryvoltage > 9.0) battery[0].batteryvoltage -= 0.1;
                break;
            case 3:
                if (battery[1].batterytemp > -127) battery[1].batterytemp--;
                break;
            case 4:
                if (battery[1].batteryvoltage > 5.5) battery[1].batteryvoltage -= 0.1;
                break;
            default:
                break;
        }
    }
}

// Return calibration correction based on battery temperature
double get_correct(double battery_temp) {
    double correct = 0;
    if (battery_temp >= 45)
        correct = -0.02;
    else if (battery_temp >= 0 && battery_temp < 45)
        correct = 0;
    else if (battery_temp >= -10 && battery_temp < 25)
        correct = 0.02;
    else if (battery_temp < -10)
        correct = 0.04;
    return correct;
}

// CAN sender thread: sends CAN frames periodically
void *can_sender_thread(void *arg) {
    const char *interface_name = (const char *)arg;
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation failed");
        return NULL;
    }
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("Interface index retrieval failed");
        close(sock);
        pthread_mutex_lock(&lock);
        ifrunning = 0;
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Socket bind failed");
        close(sock);
        return NULL;
    }
    while (ifrunning) {
        for (int i = 0; i < MAX_STRUCTS; i++) {
            pthread_mutex_lock(&lock);
            refresh_CAN_container();
            frame.can_id = can_msgs[i].id;
            frame.can_dlc = can_msgs[i].len;
            memcpy(frame.data, can_msgs[i].data, frame.can_dlc);
            pthread_mutex_unlock(&lock);
            if (write(sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                perror("CAN packet transmission failed");
            }
            usleep(SLEEPTIME);
        }
    }
    close(sock);
    return NULL;
}

// Battery charging simulation thread
void *charge_batterypack_thread(void *arg) {
    srand(time(NULL));
    while (ifrunning) {
        pthread_mutex_lock(&lock);
        int local_ifcharge = ifcharge;
        pthread_mutex_unlock(&lock);
        if (local_ifcharge) {
            usleep(300000);
            pthread_mutex_lock(&lock);
            for (int i = 0; i < BATTERY_CELLS; i++) {
                battery[i].batteryvoltage += 0.01;
            }
            for (int j = 0; j < BATTERY_CELLS; j++) {
                if ((rand() % RANDOM_PERCENT) == 0) {
                    battery[j].batterytemp += 0.5;
                }
            }
            pthread_mutex_unlock(&lock);
        } else {
            usleep(SLEEPTIME);
        }
    }
    return NULL;
}

// Battery temperature simulation thread
void *temp_batterypack_thread(void *arg) {
    while (ifrunning) {
        double mintemp = 1e9;
        int mintempid = 0;
        double maxtemp = -1e9;
        int maxtempid = 0;
        double totaltemps = 0;
        usleep(500000);
        pthread_mutex_lock(&lock);
        double local_air_temp = bms_temperature.AirTemp;
        for (int i = 0; i < BATTERY_CELLS; i++) {
            if (mintemp > battery[i].batterytemp) {
                mintemp = battery[i].batterytemp;
                mintempid = i + 1;
            }
            if (maxtemp < battery[i].batterytemp) {
                maxtemp = battery[i].batterytemp;
                maxtempid = i + 1;
            }
            totaltemps += battery[i].batterytemp;
            double temp_gap = local_air_temp - battery[i].batterytemp;
            if (bms_temperature.Temperature > 46) {
                battery[i].batterytemp -= 0.5;
                iftempfan = 1; // Cooling fan active
            } else if (bms_temperature.Temperature < -11) {
                battery[i].batterytemp += 0.5;
                iftempfan = 2; // Heater fan active
            } else {
                iftempfan = 0;
            }
            battery[i].batterytemp += (temp_gap / 20);
        }
        bms_temperature.Temperature = (totaltemps / BATTERY_CELLS);
        bms_temperature.MaxTemp = maxtemp;
        bms_temperature.MaxTempID = maxtempid;
        bms_temperature.MinTemp = mintemp;
        bms_temperature.MinTempID = mintempid;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// Battery voltage and SOC simulation thread
void *voltage_batterypack_thread(void *arg) {
    while (ifrunning) {
        double total_corrected_voltages = 0;
        double minvoltage = 1e9;
        int minvoltageid = 0;
        double maxvoltage = -1e9;
        int maxvoltageid = 0;
        usleep(100000);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < BATTERY_CELLS; i++){
            double corrected_voltage = battery[i].batteryvoltage + get_correct(battery[i].batterytemp);
            if (minvoltage > corrected_voltage) {
                minvoltage = corrected_voltage;
                minvoltageid = i + 1;
            }
            if (maxvoltage < corrected_voltage) {
                maxvoltage = corrected_voltage;
                maxvoltageid = i + 1;
            }
            total_corrected_voltages += corrected_voltage;
        }
        int percent = (int)ceil(((total_corrected_voltages / BATTERY_CELLS) - VOLTAGE_MIN) /
                                  (VOLTAGE_MAX - VOLTAGE_MIN) * 100.0);
        if (percent > 100) percent = 100;
        if (percent < 0) percent = 0;
        bms_soc.SOC = percent;
        bms_soc.DOD = bms_soc.Capacity * ((double)(100 - percent) / 100);
        bms_battery_info.Voltage = (uint16_t)(total_corrected_voltages / 2);
        bms_battery_info.MinVoltage = (uint8_t)(minvoltage * 10);
        bms_battery_info.MinVoltageID = minvoltageid;
        bms_battery_info.MaxVoltage = (uint8_t)(maxvoltage * 10);
        bms_battery_info.MaxVoltageID = maxvoltageid;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// Update the UI using ncurses
void update_ui() {
    pthread_mutex_lock(&lock);
    int soc = bms_soc.SOC;
    int air_temp = bms_temperature.AirTemp;
    int local_ifcharge = ifcharge;
    int local_iftempfan = iftempfan;
    int battery_temp[BATTERY_CELLS];
    double battery_voltage[BATTERY_CELLS];
    for (int i = 0; i < BATTERY_CELLS; i++) {
        battery_temp[i] = battery[i].batterytemp;
        battery_voltage[i] = battery[i].batteryvoltage;
    }
    int local_input_mode = input_mode;
    pthread_mutex_unlock(&lock);

    clear();
    mvprintw(0, 0, "BMS SIMULATOR v%s", VERSION);

    // Display input mode selection
    const char* items[] = {"air_temp", "C1temp", "C1voltage", "C2temp", "C2voltage"};
    for (int i = 0; i < 5; i++) {
        if (i == local_input_mode) {
            attron(A_REVERSE);
            mvprintw(2, i * 14, "[%s]", items[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(2, i * 14, "[%s]", items[i]);
        }
    }

    // Draw battery bar
    mvprintw(4, 0, "Battery:");
    int bar_len = (soc * BAR_WIDTH) / 100;
    mvprintw(5, 0, "[");
    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < bar_len)
            addch('=');
        else
            addch(' ');
    }
    printw("] %d%%", soc);

    // Display battery cell information in a grid layout with improved spacing
    int row = 7;
    for (int i = 0; i < BATTERY_CELLS; i++) {
        int col = (i % 5) * 25; // 5 cells per row, each column 25 characters wide
        mvprintw(row, col, "C%03d: %3d°C, %.2fv", i + 1, battery_temp[i], battery_voltage[i]);
        if ((i + 1) % 5 == 0) {
            row++;
        }
    }

    // Display status information
    int status_row = row + 2;
    mvprintw(status_row, 0, "Air Temp: %d°C", air_temp);
    if (local_ifcharge)
        mvprintw(status_row, 20, "[Charging]");
    else
        mvprintw(status_row, 20, "[Not Charging]");
    if (local_iftempfan == 1)
        mvprintw(status_row, 40, "[Cooling fan]");
    else if (local_iftempfan == 2)
        mvprintw(status_row, 40, "[Heater fan]");
    else
        mvprintw(status_row, 40, "[Fan off]");

    refresh();
}

// Process user input from ncurses
void process_input(int ch) {
    pthread_mutex_lock(&lock);
    switch(ch) {
        case ' ':
            ifcharge = !ifcharge;
            break;
        case KEY_UP:
            change_value(input_mode, 1);
            break;
        case KEY_DOWN:
            change_value(input_mode, 0);
            break;
        case KEY_RIGHT:
            if (input_mode < 4) input_mode++;
            break;
        case KEY_LEFT:
            if (input_mode > 0) input_mode--;
            break;
        case 'g':
            if (bms_soc.SOC > 0) bms_soc.SOC--;
            break;
        case 'G':
            if (bms_soc.SOC < 100) bms_soc.SOC++;
            break;
        case 27: // ESC key to exit
            ifrunning = 0;
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&lock);
}

// Perform initial configuration using ncurses prompts
void initial_setup() {
    char input_str[10];
    int soc, soh, designed_capacity, air_temp;
    echo(); // Enable echo for user input
    nodelay(stdscr, FALSE); // Blocking input for initial setup

    clear();
    mvprintw(0, 0, "Enter SOC (0-100): ");
    refresh();
    getstr(input_str);
    soc = atoi(input_str);
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;

    mvprintw(1, 0, "Enter SOH (0-100): ");
    refresh();
    getstr(input_str);
    soh = atoi(input_str);
    if (soh < 0) soh = 0;
    if (soh > 100) soh = 100;

    mvprintw(2, 0, "Enter designed capacity (mAh): ");
    refresh();
    getstr(input_str);
    designed_capacity = atoi(input_str);
    if (designed_capacity < 0) designed_capacity = 0;

    mvprintw(3, 0, "Enter air temperature (℃): ");
    refresh();
    getstr(input_str);
    air_temp = atoi(input_str);
    if (air_temp < -40) air_temp = -40;
    if (air_temp > 127) air_temp = 127;

    pthread_mutex_lock(&lock);
    default_battery.batteryvoltage = VOLTAGE_MIN + ((VOLTAGE_MAX - VOLTAGE_MIN) * soc / 100);
    bms_soc.SOH = soh;
    batterypack.DesignedCapacity = designed_capacity;
    bms_temperature.AirTemp = air_temp;
    bms_soc.Capacity = batterypack.DesignedCapacity * ((double)bms_soc.SOH / 100);
    pthread_mutex_unlock(&lock);

    noecho(); // Disable echo for main loop
    nodelay(stdscr, TRUE); // Non-blocking input for main loop
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <can_interface>\n", argv[0]);
        return 1;
    }

    // Initialize battery array with default values
    init_battery_array();

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Initial configuration via ncurses prompts
    initial_setup();

    // Create simulation threads
    pthread_t tid_can, tid_charge, tid_temp, tid_voltage;
    pthread_mutex_init(&lock, NULL);

    pthread_create(&tid_can, NULL, can_sender_thread, argv[1]);
    pthread_create(&tid_charge, NULL, charge_batterypack_thread, NULL);
    pthread_create(&tid_temp, NULL, temp_batterypack_thread, NULL);
    pthread_create(&tid_voltage, NULL, voltage_batterypack_thread, NULL);

    // Main loop: update UI and process user input
    int ch;
    while (ifrunning) {
        ch = getch();
        if (ch != ERR) {
            process_input(ch);
        }
        update_ui();
        usleep(100000); // Delay 100ms between refreshes
    }

    // Cleanup and exit
    endwin();
    pthread_mutex_destroy(&lock);
    pthread_join(tid_can, NULL);
    pthread_join(tid_charge, NULL);
    pthread_join(tid_temp, NULL);
    pthread_join(tid_voltage, NULL);

    return 0;
}