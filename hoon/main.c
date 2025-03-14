#include "all_headers.h"

/*================================================================
main.c
This program is a CAN packet sender which updates BMS status live-on-time.

Parameters:
- voltage (3.2v - 3.7v)
- temperature
- charging status (full, charging, pause, discharge)
==================================================================*/

//MACROS for debugging
#define VOLTAGENOW 3.7
#define TEMPNOW 41
#define BATTMAX 3.7
#define BATTMIN 3.2
#define CARVIN "KMHEM44CPLU123456"
#define BATTCYCLE 172

//MACORS for color printing
#define BLUE    "\033[34m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define RESET   "\033[0m" 

void print_battery(float voltage, int temp) {
    int percentage = (int)(((voltage - BATTMIN) / (BATTMAX - BATTMIN)) * 100);
    if (percentage < 0) {
        printf(RED "Battery Voltage LOW\n" RESET);
        percentage = 0;
    }
    if (percentage > 100) {
        printf(RED "Battery Voltage HIGH\n" RESET);
        percentage = 100;
    }
    int bars = percentage / 4;  // Scale percentage to 4% increments
    if (bars == 0 && percentage > 0) bars = 1;  // Ensure at least one block for non-zero percentages
    if (15 > temp)                  printf(BLUE);
    if (15 <= temp && 35 >= temp)   printf(GREEN);
    if (35 <= temp && 40 >= temp)   printf(YELLOW);
    if (40 <= temp)                 printf (RED);
    printf("Battery: [%.*s%.*s] %d%%\n", bars, "|||||||||||||||||||||||||", (25 - bars), "                         ", percentage);
    printf(RESET);
}

void print_screen(float voltage, int temp, int stat) {
    print_battery(voltage, temp);
    switch(stat) {
        case 0:
            printf("Discharging");
            break;
        case 1:
            printf("Charging");
            break;
        case 2:
            printf("Full");
            break;
        case 4:
            printf("Paused");
            break;
        default:
            printf("Unknown");
            break;
    }
    printf("\t\t%.2f    %d°C\n", voltage, temp);
}

int main(void) {
    printf("BMS_simulator 0.01\n\n");

    // //MACRO testcase
    // float voltage = VOLTAGENOW;
    // int temp = TEMPNOW;
    // int stat = 1;
    // print_screen(voltage, temp, stat);


    //loop testcase
    float voltage = 1;
    int temp = 1;
    int stat = 1;
    while(1){
        printf("\nvoltage : ");
        scanf("%f", &voltage);
        printf("temp :");
        scanf("%d", &temp);
        printf("stat : ");
        scanf("%d", &stat);
        print_screen(voltage, temp, stat);
    }


    return 0;
}