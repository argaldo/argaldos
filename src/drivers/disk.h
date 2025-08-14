/* Header for hard disk driver (ATA PIO mode) as part of the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 */
#include <stdbool.h>

#ifndef DISK_H
#define DISK_H


char* readdisk(int32_t sect);
// Expose accessDisk for direct disk sector access
uint8_t* accessDisk(int32_t sect, bool isWrite, uint8_t data[512]);

void writedisk(int32_t sect, char* data);

bool identifyCompatibility();

bool identifyInitiate();

#endif
