/*
Some binary operation functions written for SpecOS.
Copyright (C) 2024 Jake Steinburger under the MIT license.
See the github repo for more information.
*/

#include <stdint.h>

#include <stdlib/binop.h>

uint32_t combine32bit(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return ((uint32_t)byte1 << 24) |
        ((uint32_t)byte2 << 16) |
        ((uint32_t)byte3 << 8)  |
        (uint32_t)byte4;
}

uint64_t combine64bit(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, uint8_t byte5, uint8_t byte6, uint8_t byte7, uint8_t byte8) {
    return 
        ((uint64_t)byte1 << 56) |
        ((uint64_t)byte2 << 48) |
        ((uint64_t)byte3 << 40) |
        ((uint64_t)byte4 << 32) |
        ((uint64_t)byte5 << 24) |
        ((uint64_t)byte6 << 16) |
        ((uint64_t)byte7 << 8)  |
        (uint64_t)byte8;
}

int getBit(unsigned char num, int x) {
    // Shift 1 x positions to the right and perform bitwise AND with num
    return (num >> x) & 1;
}
