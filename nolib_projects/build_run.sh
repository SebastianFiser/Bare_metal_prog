#!/bin/bash
FILE="text.txt"

set -e

gcc -nostdlib -o logic logic.c


echo "Build complete. Output file: logic"

./logic $FILE