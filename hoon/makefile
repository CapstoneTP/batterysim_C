# =====================================================================
# makefile for BMS_simulator
# =====================================================================

CC = gcc
CFLAGS = -Wall -Wextra -g -I -lpthread -lm

TARGET = bms_simulator

SRCS = main.c dbc.c

OBJS = $(SRCS:.c=.o)
$(info OBJS=$(OBJS))

HEADERS = all_headers.h dbc.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)