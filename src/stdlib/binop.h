/*
Header for binop.c, used for some binary operations as part of SpecOS.
Copyright (C) 2024 Jake Steinburger under the MIT license. See the github repo for more info.
*/

#include <stdint.h>

#ifndef BINOP_H
#define BINOP_H

uint32_t combine32bit(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4); 
uint64_t combine64bit(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, uint8_t byte5, uint8_t byte6, uint8_t byte7, uint8_t byte8);

int getBit(unsigned char num, int x);
 
#endif
