#!/bin/bash
# r.sh

gcc -o test test.c dbc.c -lpthread

if [ $? -eq 0 ]; then
    echo -e "🍀🍀Build complete🍀🍀\n"
else
    echo -e "❌❌Build failed!❌❌\n"
    exit 1
fi
