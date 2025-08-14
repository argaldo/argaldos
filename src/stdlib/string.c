#include <stdbool.h>
#include <stdlib/string.h>

void addCharToString(char *str, char c) {
    // Iterate to find the end of the string
    int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    // Append the character
    str[i] = c;
    // Null-terminate the string
    str[i + 1] = '\0';
}

void uint64_to_hex_string(uint64_t num, char *str) {
    // Define a buffer large enough to hold the maximum uint64_t value in hex
    char buffer[17];  // Max length of uint64_t in hex is 16 digits, plus null terminator
    // Index to fill the buffer
    int index = 0;

    // Special case for zero
    if (num == 0) {
        buffer[index++] = '0';
    } else {
        // Extract hex digits from least significant to most significant
        while (num > 0) {
            uint8_t digit = num & 0xF;  // Get the least significant 4 bits (a hex digit)
            if (digit < 10) {
                buffer[index++] = '0' + digit;  // Convert digit to character
            } else {
                buffer[index++] = 'A' + (digit - 10);  // Convert digit to character
            }
            num >>= 4;  // Shift right by 4 bits to get the next hex digit
        }
    }
    // Null-terminate the buffer
    buffer[index] = '\0';
    // Reverse the buffer
    reverse(buffer);
    // Copy the reversed buffer to the output string (str)
    for (int i = 0; i <= index; ++i) {
        str[i] = buffer[i];
    }
}


void uint16_to_string(uint16_t num, char *str) {
    // Define a buffer large enough to hold the maximum uint16_t value in decimal
    char buffer[6];  // Max length of uint16_t in decimal is 5 digits, plus null terminator
    // Index to fill the buffer
    int index = 0;
    // Special case for zero
    if (num == 0) {
        buffer[index++] = '0';
    } else {
        // Extract digits from least significant to most significant
        while (num > 0) {
            buffer[index++] = '0' + (num % 10);  // Convert digit to character
            num /= 10;  // Move to the next digit
        }
    }
    // Null-terminate the buffer
    buffer[index] = '\0';
    // Reverse the buffer
    reverse(buffer);
    // Copy the reversed buffer to the output string (str)
    for (int i = 0; i <= index; ++i) {
        str[i] = buffer[i];
    } 
}

void removeLastChar(char *str) {
    for (int i = strlen(str); i > 0; i--) {
        if (str[i] != '\0') {
            str[i] = '\0';
            break;
        }
    }
}

void strcpy(char* dest, const char* src) {
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0'; // Null-terminate the destination string
}

/**
 * K&R implementation
 */
void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

/* K&R */
void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* K&R */
int strlen(const char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);
    s[len-1] = '\0';
}

/* K&R 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2 */
int k_n_r_strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

bool strcmp(const char* str1, const char* str2) {
    int str1len = strlen(str1);
    int str2len = strlen(str2);
    if (str1len != str2len) return false;
    for (int c = 0; c < str1len && c < str2len; c++) {
        if (str1[c] != str2[c]) return false;
    }
    return true;
}




char * strcat(char *dest, const char *src) {
   int i,j;
   for (i=0; dest[i] != '\0';i++)
      ;
   for (j = 0; src[j] != '\0'; j++)
      dest[i+j] = src[j];
   dest[i+j] = '\0';
   return dest;
}


// Implementation of itoa()
char* itoa(int num, char* str, int base) {
    int i = 0;
    bool isNegative = false;

    /* Handle 0 explicitly, otherwise empty string is
     * printed for 0 */
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled
    // only with base 10. Otherwise numbers are
    // considered unsigned.
    if (num < 0 && base == 10) {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse(str);

    return str;
}


char* charToStr(char character) {
    static char wholeStr[2];
    wholeStr[0] = character;
    wholeStr[1] = '\0';
    return wholeStr;
}

int isspace(int c) {
        if (c == 0x20) return (1);
        return (0);
}

void trim(char * const a) {
        char *p = a, *q = a;
        while (isspace(*q)) ++q;
        while (*q) *p++ = *q++;
        *p = '\0';
        while(p > a && isspace(*--p)) *p = '\0';
}
