#!/bin/bash

# Check if an argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <filename_without_extension>"
    exit 1
fi

# Get the filename from the first argument
filename=$1

# Assemble the source file
as -o "${filename}.o" "${filename}.s"

# Link the object file
ld -o "${filename}" "${filename}.o"
