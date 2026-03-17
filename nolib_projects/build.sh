#!/bin/bash
set -e

gcc -nostdlib -no-pie -fno-stack-protector -Wl,-e,_start -o logic logic.c
echo "Build complete. Output file: logic"