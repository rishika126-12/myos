#include "string.h"
#include <stdarg.h>

void* memset(void* dest, int val, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    uint8_t v = (uint8_t)val;
    for (size_t i = 0; i < count; i++) {
        d[i] = v;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str && str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char*)s;
}

void itoa(int value, char* str, int base) {
    char* ptr;
    char* low;
    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }
    ptr = str;
    if (value < 0 && base == 10) {
        *ptr++ = '-';
    }
    low = ptr;
    unsigned int val = (value < 0 && base == 10) ? (unsigned int)(-(int64_t)value) : (unsigned int)value;
    do {
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[val % base];
        val /= base;
    } while (val);
    *ptr-- = '\0';
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

void utoa(uint32_t value, char* str, int base) {
    char* ptr = str;
    char* low;
    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }
    low = ptr;
    do {
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[value % base];
        value /= base;
    } while (value);
    *ptr-- = '\0';
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

static void format_number(char** buf_ptr, size_t* rem, uint32_t val, int base, bool sign, int width, char pad_char) {
    char num_buf[36];
    if (sign && (int32_t)val < 0) {
        itoa((int32_t)val, num_buf, base);
    } else {
        utoa(val, num_buf, base);
    }
    int len = (int)strlen(num_buf);
    while (width > len && *rem > 1) {
        *(*buf_ptr)++ = pad_char;
        (*rem)--;
        width--;
    }
    for (int i = 0; num_buf[i] && *rem > 1; i++) {
        *(*buf_ptr)++ = num_buf[i];
        (*rem)--;
    }
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    if (!buf || size == 0) return 0;
    char* p = buf;
    size_t rem = size;

    for (const char* f = fmt; *f && rem > 1; f++) {
        if (*f != '%') {
            *p++ = *f;
            rem--;
            continue;
        }
        f++;
        if (*f == '%') {
            *p++ = '%';
            rem--;
            continue;
        }

        char pad = ' ';
        if (*f == '0') {
            pad = '0';
            f++;
        }
        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        switch (*f) {
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s && rem > 1) {
                    *p++ = *s++;
                    rem--;
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                *p++ = c;
                rem--;
                break;
            }
            case 'd':
            case 'i': {
                int d = va_arg(args, int);
                format_number(&p, &rem, (uint32_t)d, 10, true, width, pad);
                break;
            }
            case 'u': {
                uint32_t u = va_arg(args, uint32_t);
                format_number(&p, &rem, u, 10, false, width, pad);
                break;
            }
            case 'x':
            case 'X': {
                uint32_t x = va_arg(args, uint32_t);
                format_number(&p, &rem, x, 16, false, width, pad);
                break;
            }
            case 'p': {
                uint32_t ptr_val = (uint32_t)va_arg(args, void*);
                if (rem > 3) {
                    *p++ = '0'; rem--;
                    *p++ = 'x'; rem--;
                }
                format_number(&p, &rem, ptr_val, 16, false, 8, '0');
                break;
            }
            default:
                *p++ = *f;
                rem--;
                break;
        }
    }
    *p = '\0';
    return (int)(p - buf);
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

int sprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return ret;
}
