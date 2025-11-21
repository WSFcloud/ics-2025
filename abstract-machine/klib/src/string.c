#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include <limits.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

#define SS (sizeof(size_t))
#define ALIGN (sizeof(size_t)-1)
#define ONES ((size_t)-1/UCHAR_MAX)
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

size_t strlen(const char *s) {
    const char *p = s;
    while (*p)
        p++;
    return p - s;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}

int strcmp(const char *l, const char *r) {
    for (; *l == *r && *l && *r; l++, r++)
        ;
    return *l - *r;
}

int strncmp(const char *l, const char *r, size_t n) {
    if (!n--)
        return 0;
    for (; *l && *r && n && *l == *r; l++, r++, n--)
        ;
    return *l - *r;
}

void *memset(void *dest, int c, size_t n) {
    unsigned char *s = dest;
    c = (unsigned char)c;
    for (; ((uintptr_t)s & ALIGN) && n; n--)
        *s++ = c;
    if (n) {
        size_t *w, k = ONES * c;
        for (w = (void *)s; n >= SS; n -= SS, w++)
            *w = k;
        for (s = (void *)w; n; n--, s++)
            *s = c;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    if (d == s)
        return d;
    if ((size_t)(d - s) < n) {
        while (n--)
            d[n] = s[n];
        return dest;
    }
    /* Assumes memcpy is overlap-safe when dest < src */
    return memcpy(d, s, n);
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (((uintptr_t)d & ALIGN) != ((uintptr_t)s & ALIGN))
        goto misaligned;

    for (; ((uintptr_t)d & ALIGN) && n; n--)
        *d++ = *s++;
    if (n) {
        size_t *wd = (void *)d;
        const size_t *ws = (const void *)s;

        for (; n >= SS; n -= SS)
            *wd++ = *ws++;
        d = (void *)wd;
        s = (const void *)ws;
    misaligned:
        for (; n; n--)
            *d++ = *s++;
    }
    return dest;
}

int memcmp(const void *vl, const void *vr, size_t n) {
    const unsigned char *l = vl, *r = vr;
    for (; n && *l == *r; n--, l++, r++)
        ;
    return n ? *l - *r : 0;
}

#endif
