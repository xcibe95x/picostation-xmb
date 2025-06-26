#include "delay.h"

void delayMicroseconds(int time) {
    // Calculate the approximate number of CPU cycles that need to be burned,
    // assuming a 33.8688 MHz clock (1 us = 33.8688 = ~33.875 = 271 / 8 cycles).
    // The loop consists of a branch and a decrement, thus each iteration will
    // burn 2 cycles.
    time = ((time * 271) + 4) / 8;

    __asm__ volatile(
        // The .set noreorder directive will prevent the assembler from trying
        // to "hide" the branch instruction's delay slot by shuffling nearby
        // instructions. .set push and .set pop are used to save and restore the
        // assembler's settings respectively, ensuring the noreorder flag will
        // not affect any other code.
        ".set push\n"
        ".set noreorder\n"
        "bgtz  %0, .\n"
        "addiu %0, -2\n"
        ".set pop\n"
        : "+r"(time)
    );
}