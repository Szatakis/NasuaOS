#pragma once
#include <stddef.h>
#include <stdint.h>
#include <cstddef>

void strcpy(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
bool strncmp(const char *s1, const char *s2, size_t n);
const char* strchr(const char *str, char c);
const char* strstr(const char *haystack, const char *needle);
char* strcat(char* destination, const char* source);

int atoi(const char* str);
char* itoa(int64_t value, char* str);

uint32_t parse_digits(const char* str, uint8_t count);
uint8_t hex_char_to_val(char c);
uint32_t parse_hex_color(const char *str);
int parse_number(const char* str);
void to_hex8(uint8_t value, char* out);
void to_hex64(uint64_t value, char* out);
uint32_t parse_hex(const char* str);
char* strtok(char* str, const char* delim);
char* strrchr(const char* str, int c);
bool find_in_string(const char* str, const char* target);
void memclear(void* ptr, size_t size);
bool is_empty_or_whitespace(const char* str);
void uint64_to_string(uint64_t value, char* buffer, size_t size);
