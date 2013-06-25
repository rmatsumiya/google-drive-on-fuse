#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "str.h"
char *new_str(const char *val) {
    size_t l = strlen(val);
    char *res = (char *)malloc(l + 1);
    strncpy(res, val, l + 1);
    return res;
}

char *str_cat(const char *a, const char *b) {
    char *res = (char *)malloc(strlen(a) + strlen(b) + 1);
    strncpy(res, a, strlen(a) + 1);
//    fprintf(stderr, "%s\n", res);
    strncpy(res + strlen(a), b, strlen(b) + 1);
    return res;
}

char *str_split(const char *a, const char b, int *start) {
    char *res;
    int size = 0;
    *start = 0;
    while(a[*start] == b && *start < strlen(a)) {
        ++(*start);
    }
    do {
        ++size;
        if(*start + size >= strlen(a)) {
            break;
        }
    } while(a[*start + size] != b);
    res = malloc(size + 1);
    memset(res, 0, size + 1);
    strncpy(res, a + *start, size);
    return res;
}

char *str_split_rev(const char *a, const char b, int *pos) {
    char *res;
    *pos = strlen(a);
    while(a[--(*pos)] == b && *pos > 0);
    while(a[--(*pos)] != b && *pos >= 0);
    ++(*pos);
    res = (char *)malloc(*pos + 1);
    memset(res, 0, *pos + 1);
    strncpy(res, a, *pos);
    return res;
}
