#include <stdint.h>

#ifndef STRINGS_H
#define STRINGS_H


enum strtrim_mode_t {
        STRLIB_MODE_ALL = 0x00,
        STRLIB_MODE_RIGHT = 0x01,
        STRLIB_MODE_LEFT = 0x02,
        STRLIB_MODE_BOTH = 0x03
};

char * strcat(char *dest, const char *src);
char* itoa(int num, char* str, int base);

void int_to_ascii(int n, char str[]);
void reverse(char s[]);
int strlen(const char s[]);
void backspace(char s[]);
void append(char s[], char n);
int k_n_r_strcmp(char s1[], char s2[]);
bool strcmp(const char* str1, const char* str2);

void strcpy(char* dest, const char* src);
void removeLastChar(char *str);
void addCharToString(char *str, char c);

void uint64_to_hex_string(uint64_t num, char *str);
void uint16_to_string(uint16_t num, char *str);

char* charToStr(char character);

void trim(char * const a);

#endif
