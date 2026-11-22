#ifndef STRING_H
#define STRING_H

#include "types.h"

#include <stdarg.h>

void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
int   memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* str);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
char*  strcat(char* dest, const char* src);
char*  strchr(const char* s, int c);

int    sprintf(char* buf, const char* fmt, ...);
int    snprintf(char* buf, size_t size, const char* fmt, ...);
int    vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

void   itoa(int value, char* str, int base);
void   utoa(uint32_t value, char* str, int base);

#endif
