#ifndef ALL_HEADERS_H
#define ALL_HEADERS_H

/*================================================================
all_headers.h
==================================================================*/

//standard headers
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>

//MACORS for color printing
#define BLACK  "\033[30m" 
#define BLUE    "\033[34m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define RESET   "\033[0m"

#define YELLOW_BG   "\033[43m"
#define BLUE_BG "\033[44m"
#define HIGHLIGHT "\033[33m\033[41m"


//MACORS for setting cursor position
#define CLEAR_SCREEN "\033[1J"
#define CURSOR_HIDE "\033[?25l"
#define CURSOR_SHOW "\033[?25h"
#define SET_CURSOR_UL "\033[H"
#define CURSOR_UP "\033[1A"

//userdefined headers
#include "dbc.h"


#endif // ALL_HEADERS_H