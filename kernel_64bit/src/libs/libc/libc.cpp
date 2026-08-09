#include <stdint.h>
#include <cstddef>

void strcpy(char* dest, const char* src)
{
    while (*src)
    {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

int strcmp(const char* s1, const char* s2) 
{
    while (*s1 && (*s1 == *s2)) 
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* itoa(int64_t value, char* str)
{
    char* p = str;
    bool negative = false;

    if (value == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    if (value < 0)
    {
        negative = true;
        uint64_t abs_value = -static_cast<uint64_t>(value);
        
        while (abs_value)
        {
            *p++ = '0' + (abs_value % 10);
            abs_value /= 10;
        }
    }
    else
    {
        uint64_t abs_value = value;
        while (abs_value)
        {
            *p++ = '0' + (abs_value % 10);
            abs_value /= 10;
        }
    }

    if (negative)
    {
        *p++ = '-';
    }

    *p = '\0';

    char* start = str;
    char* end = p - 1;

    while (start < end)
    {
        char t = *start;
        *start = *end;
        *end = t;
        start++;
        end--;
    }

    return str;
}

uint32_t parse_digits(const char* str, uint8_t count) 
{
    uint32_t result = 0;

    for (uint8_t i = 0; i < count; i++) 
    {
        if (str[i] < '0' || str[i] > '9') 
        {
            return result;
        }
        
        result = result * 10 + (str[i] - '0');
    }

    return result;
}

bool strncmp(const char *s1, const char *s2, size_t n) 
{
    for (size_t i = 0; i < n; i++) 
    {
        if (s1[i] != s2[i]) return false;
        if (s1[i] == '\0') return true;
    }
    return true;
}

const char* strchr(const char *str, char c) 
{
    while (*str) 
    {
        if (*str == c) return str;
        str++;
    }
    return nullptr;
}

const char* strstr(const char *haystack, const char *needle) 
{
    if (!*needle) 
    {
        return haystack;
    }

    while (*haystack) 
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) 
        {
            h++;
            n++;
        }
        if (!*n) 
        {
            return haystack;
        }
        haystack++;
    }

    return nullptr;
}

uint8_t hex_char_to_val(char c) 
{
    if (c >= '0' && c <= '9') 
    {
        return c - '0';
    }

    if (c >= 'A' && c <= 'F') 
    {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') 
    {
        return c - 'a' + 10;
    }

    return 0;
}

uint32_t parse_hex_color(const char *str) 
{
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) 
    {
        str += 2;
    }
    
    if (*str == '"') 
    {
        str++;
    }

    uint32_t result = 0;
    for (int i = 0; i < 8; i++) 
    {
        char c = str[i];
        if (c == '\0' || c == '"' || c == ' ') 
        {
            break;
        }
        
        result <<= 4;
        result |= hex_char_to_val(c);
    }

    return result;
}

int parse_number(const char* str) 
{
    int num = 0;

    while (*str >= '0' && *str <= '9') 
    {
        num = num * 10 + (*str - '0');
        str++;
    }

    return num;
}

void to_hex8(uint8_t value, char* out) 
{
    static const char hex[] = "0123456789ABCDEF";

    out[0] = '0';
    out[1] = 'x';
    out[2] = hex[(value >> 4) & 0xF];
    out[3] = hex[value & 0xF];
    out[4] = '\0';
}

size_t strlen(const char* str)
{
    size_t length = 0;

    while (str[length] != '\0')
    {
        length++;
    }

    return length;
}

char* strcat(char* destination, const char* source)
{
    char* ptr = destination;

    while (*ptr != '\0')
    {
        ptr++;
    }

    while (*source != '\0')
    {
        *ptr = *source;
        ptr++;
        source++;
    }

    *ptr = '\0';

    return destination;
}

int atoi(const char* str) 
{
    int res = 0;
    int sign = 1;
    int i = 0;

    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r' || str[i] == '\v' || str[i] == '\f') 
    {
        i++;
    }

    if (str[i] == '-') 
    {
        sign = -1;
        i++;
    } else if (str[i] == '+') 
    {
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9') 
    {
        res = res * 10 + (str[i] - '0');
        i++;
    }

    return sign * res;
}

uint32_t parse_hex(const char* str) {
    if (!str)
        return 0;

    // 1. Najpierw pomiń spacje i cudzysłowy początkowe
    while (*str == ' ' || *str == '"')
        str++;

    // 2. Dopiero teraz sprawdź i pomiń prefiks 0x / 0X
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
        str += 2;

    uint32_t value = 0;

    // 3. Przetwarzaj znaki
    while (*str)
    {
        char c = *str++;

        // Jeśli trafisz na kończący cudzysłów lub spację, przerwij
        if (c == '"' || c == ' ')
            break;

        uint8_t digit = 0;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            break; // Nieprawidłowy znak hex

        // Przesuń dotychczasową wartość i dodaj nową cyfrę
        value = (value << 4) | digit;
    }

    return value;
}

char* strtok(char* str, const char* delim) {
    static char* backup_ptr = nullptr;
    if (str != nullptr) {
        backup_ptr = str;
    }

    if (backup_ptr == nullptr || *backup_ptr == '\0') {
        return nullptr;
    }

    char* start = backup_ptr;
    
    // Szukamy znaku delimetera
    while (*backup_ptr != '\0') {
        bool is_delim = false;
        for (int i = 0; delim[i] != '\0'; i++) {
            if (*backup_ptr == delim[i]) {
                is_delim = true;
                break;
            }
        }

        if (is_delim) {
            *backup_ptr = '\0'; // Zamieniamy delimiter na koniec stringa
            backup_ptr++;
            return start;
        }
        backup_ptr++;
    }

    return start;
}

char* strrchr(const char* str, int c) {
    const char* last = nullptr;
    while (*str) {
        if (*str == (char)c) last = str;
        str++;
    }
    return (char*)last;
}

bool find_in_string(const char* str, const char* target) 
{
    if (!str || !target) return false;
    for (std::size_t i = 0; str[i] != '\0'; ++i) 
    {
        std::size_t j = 0;
        while (target[j] != '\0' && str[i + j] == target[j]) 
        {
            j++;
        }
        if (target[j] == '\0') 
        {
            return true;
        }
    }
    return false;
}

void memclear(void* ptr, size_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for(size_t i = 0; i < size; i++) p[i] = 0;
}
