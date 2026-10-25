#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void* doom_nasua_allocate_memory(uint32_t size);
extern void doom_nasua_deallocate_memory(void* memory);
extern void doom_nasua_log(const char* text);
extern void doom_nasua_game_quit(void);
extern void doom_nasua_game_error(const char* message);

typedef struct doom_allocation
{
    size_t size;
    struct doom_allocation* previous;
    struct doom_allocation* next;
} doom_allocation;

static doom_allocation* allocation_head;
static void* runtime_exit_buffer[5];
static int runtime_exit_active;

__attribute__((visibility("hidden"))) void doom_relocate_image(
    void* image_base, const uint32_t* relocations, const void* image_end)
{
    const uintptr_t base = (uintptr_t)image_base;
    const uintptr_t image_size = (uintptr_t)image_end - base;
    const uint32_t relocation_count = relocations[0];
    if (relocation_count > 4096)
    {
        return;
    }

    for (uint32_t index = 0; index < relocation_count; ++index)
    {
        const uint32_t offset = relocations[index + 1];
        if (offset > image_size || sizeof(uint64_t) > image_size - offset)
        {
            continue;
        }

        uint64_t* address = (uint64_t*)(base + offset);
        *address += base;
    }
}

FILE* stdin;
FILE* stdout;
FILE* stderr;

void* memcpy(void* destination, const void* source, size_t length)
{
    unsigned char* dst = destination;
    const unsigned char* src = source;
    for (size_t i = 0; i < length; ++i)
    {
        dst[i] = src[i];
    }
    return destination;
}

void* memmove(void* destination, const void* source, size_t length)
{
    unsigned char* dst = destination;
    const unsigned char* src = source;
    if (dst < src)
    {
        for (size_t i = 0; i < length; ++i)
        {
            dst[i] = src[i];
        }
    }
    else if (dst > src)
    {
        while (length != 0)
        {
            --length;
            dst[length] = src[length];
        }
    }
    return destination;
}

void* memset(void* destination, int value, size_t length)
{
    unsigned char* dst = destination;
    for (size_t i = 0; i < length; ++i)
    {
        dst[i] = (unsigned char)value;
    }
    return destination;
}

int memcmp(const void* first, const void* second, size_t length)
{
    const unsigned char* a = first;
    const unsigned char* b = second;
    for (size_t i = 0; i < length; ++i)
    {
        if (a[i] != b[i])
        {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

size_t strlen(const char* text)
{
    size_t length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}

int strcmp(const char* first, const char* second)
{
    while (*first != '\0' && *first == *second)
    {
        ++first;
        ++second;
    }
    return (unsigned char)*first - (unsigned char)*second;
}

int strncmp(const char* first, const char* second, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char a = (unsigned char)first[i];
        unsigned char b = (unsigned char)second[i];
        if (a != b || a == '\0')
        {
            return a - b;
        }
    }
    return 0;
}

int strcasecmp(const char* first, const char* second)
{
    while (*first != '\0' && *second != '\0')
    {
        unsigned char a = (unsigned char)*first++;
        unsigned char b = (unsigned char)*second++;
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 'a' - 'A');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 'a' - 'A');
        if (a != b) return a - b;
    }
    return (unsigned char)*first - (unsigned char)*second;
}

int strncasecmp(const char* first, const char* second, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char a = (unsigned char)first[i];
        unsigned char b = (unsigned char)second[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 'a' - 'A');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 'a' - 'A');
        if (a != b || a == '\0') return a - b;
    }
    return 0;
}

char* strchr(const char* text, int character)
{
    while (*text != '\0')
    {
        if (*text == (char)character) return (char*)text;
        ++text;
    }
    return character == '\0' ? (char*)text : NULL;
}

char* strrchr(const char* text, int character)
{
    const char* result = NULL;
    do
    {
        if (*text == (char)character) result = text;
    } while (*text++ != '\0');
    return (char*)result;
}

char* strstr(const char* text, const char* needle)
{
    if (*needle == '\0') return (char*)text;
    for (; *text != '\0'; ++text)
    {
        if (*text == *needle && strncmp(text, needle, strlen(needle)) == 0)
        {
            return (char*)text;
        }
    }
    return NULL;
}

char* strncpy(char* destination, const char* source, size_t length)
{
    size_t i = 0;
    for (; i < length && source[i] != '\0'; ++i)
    {
        destination[i] = source[i];
    }
    for (; i < length; ++i)
    {
        destination[i] = '\0';
    }
    return destination;
}

char* strdup(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy != NULL) memcpy(copy, text, length);
    return copy;
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

double fabs(double value)
{
    return value < 0 ? -value : value;
}

long strtol(const char* text, char** end, int base)
{
    const char* original = text;
    while (*text == ' ' || *text == '\t' || *text == '\n') ++text;
    int negative = *text == '-';
    if (negative || *text == '+') ++text;
    if (base == 0)
    {
        base = text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ? 16 :
               text[0] == '0' ? 8 : 10;
    }
    if (base == 16 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;

    unsigned long value = 0;
    const char* digits_start = text;
    while (*text != '\0')
    {
        unsigned char c = (unsigned char)*text;
        int digit = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'z' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'Z' ? c - 'A' + 10 : base;
        if (digit >= base) break;
        value = value * (unsigned int)base + (unsigned int)digit;
        ++text;
    }
    if (end != NULL) *end = (char*)(text == digits_start ? original : text);
    return negative ? -(long)value : (long)value;
}

double strtod(const char* text, char** end)
{
    while (*text == ' ' || *text == '\t' || *text == '\n') ++text;
    int negative = *text == '-';
    if (negative || *text == '+') ++text;

    double value = 0;
    int digits = 0;
    while (*text >= '0' && *text <= '9')
    {
        value = value * 10.0 + (*text++ - '0');
        ++digits;
    }
    if (*text == '.')
    {
        ++text;
        double scale = 0.1;
        while (*text >= '0' && *text <= '9')
        {
            value += (*text++ - '0') * scale;
            scale *= 0.1;
            ++digits;
        }
    }
    if (*text == 'e' || *text == 'E')
    {
        ++text;
        int exp_negative = *text == '-';
        if (exp_negative || *text == '+') ++text;
        int exponent = 0;
        while (*text >= '0' && *text <= '9')
        {
            if (exponent < 308) exponent = exponent * 10 + (*text - '0');
            ++text;
        }
        double multiplier = 1.0;
        for (int i = 0; i < exponent; ++i) multiplier *= 10.0;
        value = exp_negative ? value / multiplier : value * multiplier;
    }
    if (end != NULL) *end = (char*)text;
    return digits == 0 ? 0.0 : negative ? -value : value;
}

double atof(const char* text)
{
    return strtod(text, NULL);
}

int sscanf(const char* input, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int assigned = 0;

    while (*format != '\0' && *input != '\0')
    {
        if (*format == ' ' || *format == '\t' || *format == '\n')
        {
            while (*format == ' ' || *format == '\t' || *format == '\n') ++format;
            while (*input == ' ' || *input == '\t' || *input == '\n') ++input;
            continue;
        }
        if (*format != '%')
        {
            if (*format++ != *input++) break;
            continue;
        }

        ++format;
        if (*format == '%')
        {
            if (*input++ != '%') break;
            ++format;
            continue;
        }

        int base = 10;
        char conversion = *format++;
        if (conversion == 'x' || conversion == 'X') base = 16;
        else if (conversion == 'o') base = 8;
        else if (conversion == 'i') base = 0;
        else if (conversion != 'd' && conversion != 'u') break;

        while (*input == ' ' || *input == '\t' || *input == '\n') ++input;
        char* end = NULL;
        long value = strtol(input, &end, base);
        if (end == input) break;

        if (conversion == 'u' || conversion == 'x' || conversion == 'X' || conversion == 'o')
        {
            unsigned int* output = va_arg(args, unsigned int*);
            if (output != NULL) *output = (unsigned int)value;
        }
        else
        {
            int* output = va_arg(args, int*);
            if (output != NULL) *output = (int)value;
        }
        input = end;
        ++assigned;
    }

    va_end(args);
    return assigned;
}

static int32_t uppercase_table[384];
static const int32_t* uppercase_values;
static const int32_t** uppercase_values_pointer = &uppercase_values;

const int32_t** __ctype_toupper_loc(void)
{
    if (uppercase_values == NULL)
    {
        for (int value = -128; value < 256; ++value)
        {
            int upper = value;
            if (value >= 'a' && value <= 'z') upper = value - 'a' + 'A';
            uppercase_table[value + 128] = upper;
        }
        uppercase_values = &uppercase_table[128];
    }
    return uppercase_values_pointer;
}

int putc(int character, FILE* stream)
{
    (void)stream;
    return putchar(character);
}

int remove(const char* path)
{
    (void)path;
    return -1;
}

int rename(const char* old_path, const char* new_path)
{
    (void)old_path;
    (void)new_path;
    return -1;
}

int system(const char* command)
{
    (void)command;
    return -1;
}

void* malloc(size_t size)
{
    if (size > UINT32_MAX - sizeof(doom_allocation))
    {
        return NULL;
    }
    doom_allocation* block = doom_nasua_allocate_memory((uint32_t)(sizeof(*block) + size));
    if (block == NULL)
    {
        return NULL;
    }
    block->size = size;
    block->previous = NULL;
    block->next = allocation_head;
    if (allocation_head != NULL)
    {
        allocation_head->previous = block;
    }
    allocation_head = block;
    return block + 1;
}

void free(void* memory)
{
    if (memory != NULL)
    {
        doom_allocation* block = (doom_allocation*)memory - 1;
        if (block->previous != NULL)
        {
            block->previous->next = block->next;
        }
        else
        {
            allocation_head = block->next;
        }
        if (block->next != NULL)
        {
            block->next->previous = block->previous;
        }
        doom_nasua_deallocate_memory(block);
    }
}

void doom_runtime_cleanup(void)
{
    while (allocation_head != NULL)
    {
        doom_allocation* block = allocation_head;
        allocation_head = block->next;
        doom_nasua_deallocate_memory(block);
    }
}

int doom_runtime_protected_call(void (*function)(void*), void* context)
{
    if (function == NULL || runtime_exit_active)
    {
        return -1;
    }

    runtime_exit_active = 1;
    int exit_requested = __builtin_setjmp(runtime_exit_buffer);
    if (exit_requested == 0)
    {
        function(context);
    }
    runtime_exit_active = 0;
    return exit_requested;
}

void exit(int status)
{
    if (status != 0)
    {
        doom_nasua_game_error("Doom exited with an error.");
    }
    doom_nasua_game_quit();
    if (runtime_exit_active)
    {
        __builtin_longjmp(runtime_exit_buffer, 1);
    }

    for (;;)
    {
    }
}

void* calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size)
    {
        return NULL;
    }
    size_t bytes = count * size;
    void* result = malloc(bytes);
    if (result != NULL)
    {
        memset(result, 0, bytes);
    }
    return result;
}

void* realloc(void* memory, size_t size)
{
    if (memory == NULL)
    {
        return malloc(size);
    }
    if (size == 0)
    {
        free(memory);
        return NULL;
    }

    doom_allocation* old_block = (doom_allocation*)memory - 1;
    void* result = malloc(size);
    if (result != NULL)
    {
        size_t copy_size = old_block->size < size ? old_block->size : size;
        memcpy(result, memory, copy_size);
        free(memory);
    }
    return result;
}

static void append_char(char* buffer, size_t capacity, size_t* length, char value)
{
    if (*length + 1 < capacity)
    {
        buffer[*length] = value;
    }
    ++*length;
}

static void append_repeat(char* buffer, size_t capacity, size_t* length,
                          char value, size_t count)
{
    while (count-- != 0)
    {
        append_char(buffer, capacity, length, value);
    }
}

static void append_integer(char* buffer, size_t capacity, size_t* length,
                           uint64_t value, unsigned int base, int uppercase,
                           int negative, int width, int precision,
                           int left_align, int zero_pad, int show_sign,
                           int space_sign, int alternate, int pointer)
{
    char digits[32];
    size_t digit_count = 0;
    do
    {
        unsigned int digit = (unsigned int)(value % base);
        digits[digit_count++] = (char)(digit < 10 ? '0' + digit :
                                       (uppercase ? 'A' : 'a') + digit - 10);
        value /= base;
    } while (value != 0 && digit_count < sizeof(digits));

    if (precision == 0 && digit_count == 1 && digits[0] == '0')
    {
        digit_count = 0;
    }

    char sign = negative ? '-' : show_sign ? '+' : space_sign ? ' ' : '\0';
    char prefix[2];
    size_t prefix_length = 0;
    if (pointer || (alternate && base == 16 && digit_count != 0))
    {
        prefix[0] = '0';
        prefix[1] = uppercase ? 'X' : 'x';
        prefix_length = 2;
    }
    else if (alternate && base == 8 &&
             (digit_count == 0 || digits[digit_count - 1] != '0'))
    {
        prefix[0] = '0';
        prefix_length = 1;
    }

    size_t zero_count = precision > (int)digit_count
        ? (size_t)precision - digit_count
        : 0;
    size_t content_length = digit_count + zero_count + prefix_length +
                            (sign != '\0' ? 1 : 0);
    size_t padding = width > (int)content_length
        ? (size_t)width - content_length
        : 0;

    if (!left_align && !(zero_pad && precision < 0))
    {
        append_repeat(buffer, capacity, length, ' ', padding);
    }
    if (sign != '\0')
    {
        append_char(buffer, capacity, length, sign);
    }
    for (size_t index = 0; index < prefix_length; ++index)
    {
        append_char(buffer, capacity, length, prefix[index]);
    }
    if (!left_align && zero_pad && precision < 0)
    {
        append_repeat(buffer, capacity, length, '0', padding);
    }
    append_repeat(buffer, capacity, length, '0', zero_count);
    while (digit_count != 0)
    {
        append_char(buffer, capacity, length, digits[--digit_count]);
    }
    if (left_align)
    {
        append_repeat(buffer, capacity, length, ' ', padding);
    }
}

int vsnprintf(char* buffer, size_t capacity, const char* format, va_list args)
{
    size_t length = 0;
    if (format == NULL || (capacity != 0 && buffer == NULL))
    {
        return -1;
    }

    while (*format != '\0')
    {
        if (*format != '%')
        {
            append_char(buffer, capacity, &length, *format++);
            continue;
        }
        ++format;
        if (*format == '%')
        {
            append_char(buffer, capacity, &length, *format++);
            continue;
        }

        int left_align = 0;
        int zero_pad = 0;
        int show_sign = 0;
        int space_sign = 0;
        int alternate = 0;
        for (;;)
        {
            if (*format == '-')
            {
                left_align = 1;
            }
            else if (*format == '0')
            {
                zero_pad = 1;
            }
            else if (*format == '+')
            {
                show_sign = 1;
            }
            else if (*format == ' ')
            {
                space_sign = 1;
            }
            else if (*format == '#')
            {
                alternate = 1;
            }
            else
            {
                break;
            }
            ++format;
        }

        int width = 0;
        if (*format == '*')
        {
            width = va_arg(args, int);
            ++format;
            if (width < 0)
            {
                left_align = 1;
                width = -width;
            }
        }
        else
        {
            while (*format >= '0' && *format <= '9')
            {
                if (width < 100000)
                {
                    width = width * 10 + (*format - '0');
                }
                ++format;
            }
        }

        int precision = -1;
        if (*format == '.')
        {
            ++format;
            precision = 0;
            if (*format == '*')
            {
                precision = va_arg(args, int);
                ++format;
                if (precision < 0)
                {
                    precision = -1;
                }
            }
            else
            {
                while (*format >= '0' && *format <= '9')
                {
                    if (precision < 100000)
                    {
                        precision = precision * 10 + (*format - '0');
                    }
                    ++format;
                }
            }
        }

        int length_modifier = 0;
        if (*format == 'h')
        {
            length_modifier = 1;
            ++format;
            if (*format == 'h')
            {
                length_modifier = 2;
                ++format;
            }
        }
        else if (*format == 'l')
        {
            length_modifier = 4;
            ++format;
            if (*format == 'l')
            {
                length_modifier = 5;
                ++format;
            }
        }
        else if (*format == 'z' || *format == 't')
        {
            length_modifier = 6;
            ++format;
        }

        char conversion = *format == '\0' ? '\0' : *format++;
        if (conversion == 's')
        {
            const char* value = va_arg(args, const char*);
            if (value == NULL)
            {
                value = "(null)";
            }
            size_t string_length = 0;
            while (value[string_length] != '\0' &&
                   (precision < 0 || string_length < (size_t)precision))
            {
                ++string_length;
            }
            size_t padding = width > (int)string_length
                ? (size_t)width - string_length
                : 0;
            if (!left_align) append_repeat(buffer, capacity, &length, ' ', padding);
            for (size_t index = 0; index < string_length; ++index)
                append_char(buffer, capacity, &length, value[index]);
            if (left_align) append_repeat(buffer, capacity, &length, ' ', padding);
        }
        else if (conversion == 'c')
        {
            size_t padding = width > 1 ? (size_t)width - 1 : 0;
            if (!left_align) append_repeat(buffer, capacity, &length, ' ', padding);
            append_char(buffer, capacity, &length, (char)va_arg(args, int));
            if (left_align) append_repeat(buffer, capacity, &length, ' ', padding);
        }
        else if (conversion == 'd' || conversion == 'i')
        {
            int64_t value = length_modifier == 5 ? va_arg(args, long long) :
                            length_modifier == 4 ? va_arg(args, long) :
                            length_modifier == 6 ? va_arg(args, ptrdiff_t) :
                            va_arg(args, int);
            uint64_t magnitude = value < 0
                ? (uint64_t)(-(value + 1)) + 1
                : (uint64_t)value;
            append_integer(buffer, capacity, &length, magnitude, 10, 0,
                           value < 0, width, precision, left_align, zero_pad,
                           show_sign, space_sign, alternate, 0);
        }
        else if (conversion == 'u' || conversion == 'x' || conversion == 'X' ||
                 conversion == 'o' || conversion == 'p')
        {
            uint64_t value = conversion == 'p' ? (uintptr_t)va_arg(args, void*) :
                             length_modifier == 5 ? va_arg(args, unsigned long long) :
                             length_modifier == 4 ? va_arg(args, unsigned long) :
                             length_modifier == 6 ? va_arg(args, size_t) :
                             va_arg(args, unsigned int);
            unsigned int base = conversion == 'o' ? 8 :
                                (conversion == 'x' || conversion == 'X' || conversion == 'p') ? 16 : 10;
            append_integer(buffer, capacity, &length, value, base,
                           conversion == 'X', 0, width, precision,
                           left_align, zero_pad, 0, 0,
                           alternate, conversion == 'p');
        }
        else if (conversion != '\0')
        {
            append_char(buffer, capacity, &length, '%');
            append_char(buffer, capacity, &length, conversion);
        }
    }

    if (capacity != 0)
    {
        buffer[length < capacity ? length : capacity - 1] = '\0';
    }
    return length > INT32_MAX ? INT32_MAX : (int)length;
}

int snprintf(char* buffer, size_t capacity, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return result;
}

int printf(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (result > 0)
    {
        doom_nasua_log(buffer);
    }
    return result;
}

int fprintf(FILE* stream, const char* format, ...)
{
    (void)stream;
    char buffer[512];
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (result > 0)
    {
        doom_nasua_log(buffer);
    }
    return result;
}

int vfprintf(FILE* stream, const char* format, va_list args)
{
    (void)stream;
    char buffer[512];
    int result = vsnprintf(buffer, sizeof(buffer), format, args);
    if (result > 0)
    {
        doom_nasua_log(buffer);
    }
    return result;
}

int puts(const char* text)
{
    if (text == NULL)
    {
        return -1;
    }
    doom_nasua_log(text);
    doom_nasua_log("\n");
    return (int)strlen(text) + 1;
}

int putchar(int value)
{
    char text[2] = {(char)value, '\0'};
    doom_nasua_log(text);
    return value;
}

FILE* fopen(const char* path, const char* mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

int fclose(FILE* stream)
{
    (void)stream;
    return 0;
}

size_t fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    (void)buffer;
    (void)size;
    (void)count;
    (void)stream;
    return 0;
}

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    (void)buffer;
    (void)size;
    (void)count;
    (void)stream;
    return 0;
}

int fseek(FILE* stream, long offset, int origin)
{
    (void)stream;
    (void)offset;
    (void)origin;
    return -1;
}

long ftell(FILE* stream)
{
    (void)stream;
    return -1;
}

int fflush(FILE* stream)
{
    (void)stream;
    return 0;
}

int feof(FILE* stream)
{
    (void)stream;
    return 1;
}

int ferror(FILE* stream)
{
    (void)stream;
    return 1;
}
