#!/bin/bash
set -e

gcc -nostdlib -o logic logic.c


echo "Build complete. Output file: logic"