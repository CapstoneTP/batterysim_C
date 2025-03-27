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

//MACORS for color printing
#define BLUE    "\033[34m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define RESET   "\033[0m"

//MACORS for setting cursor position
#define CLEAR_SCREEN "\x1b[1J"
#define CURSOR_HIDE "\x1b[?25l"
#define CURSOR_SHOW "\x1b[?25h"
#define SET_CURSOR_UL "\x1b[H"
#define CURSOR_UP "\x1b[1A]"
#define START_SCREEN printf(CURSOR_HIDE, CLEAR_SCREEN, SET_CURSOR_UL);

//userdefined headers
#include "dbc.h"


#endif // ALL_HEADERS_H