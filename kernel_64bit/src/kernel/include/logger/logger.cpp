#include "logger.hpp"
#include <stdint.h>

#include "system/drivers/video/driver.hpp"
#include "system/drivers/rtc/driver.h"

#include "libs/libc/libc.h"


#define LOG_BUFFER_SIZE 128


LogLevel parse_log_level(const char* level)
{
    if(strncmp(level, "INFO", 4))
        return INFO;

    if(strncmp(level, "WARN", 4))
        return WARN;

    if(strncmp(level, "ERROR", 5))
        return ERROR;

    if(strncmp(level, "DEBUG", 5))
        return DEBUG;


    return INFO;
}

struct LogEntry
{
    LogLevel level;
    RtcTime time;
    char subsystem[16];
    char message[128];
};


static LogEntry logs[LOG_BUFFER_SIZE];
static uint32_t log_index = 0;
static uint32_t log_count = 0;


void logger_init() 
{
    log_index = 0;
    log_count = 0;
}


void log(LogLevel level, const char* subsystem, const char* message)
{
    uint32_t id = log_index % LOG_BUFFER_SIZE;

    logs[id].time = get_rtc_time();
    logs[id].level = level;

    int i = 0;
    while(subsystem[i] && i < 15) 
    {
        logs[id].subsystem[i] = subsystem[i];
        i++;
    }

    logs[id].subsystem[i] = 0;

    i = 0;
    while(message[i] && i < 127) 
    {
        logs[id].message[i] = message[i];
        i++;
    }

    logs[id].message[i] = 0;

    log_index++;

    if(log_count < LOG_BUFFER_SIZE) 
    {
        log_count++;
    }
}

static void print_time(RtcTime time) 
{
    print("[");

    if(time.hour < 10) 
    {
        print("0");
    }

    print_num8(time.hour);
    print(":");


    if(time.minute < 10) 
    {
        print("0");
    }

    print_num8(time.minute);
    print(":");


    if(time.second < 10) 
    {
        print("0");
    }

    print_num8(time.second);
    print("]");
}

static void print_log_entry(LogEntry* entry) 
{
    print_time(entry->time);
    print("[");

    switch(entry->level) 
    {
        case INFO:
            print("INFO");
            break;

        case WARN:
            print("WARN");
            break;

        case ERROR:
            print("ERROR");
            break;

        case DEBUG:
            print("DEBUG");
            break;
    }

    print("] ");
    print(entry->subsystem);
    print(": ");
    print(entry->message);
    print("\n");
}

void print_logs_level(LogLevel level) 
{
    uint32_t start = 0;

    if(log_index >= LOG_BUFFER_SIZE) 
    {
        start = log_index % LOG_BUFFER_SIZE;
    }

    for(uint32_t i = 0; i < log_count; i++) 
    {
        uint32_t id = (start + i) % LOG_BUFFER_SIZE;

        if(logs[id].level == level) 
        {
            print_log_entry(&logs[id]);
        }
    }
}

static bool string_equal(const char* a, const char* b) 
{
    while(*a && *b) 
    {
        if(*a != *b) 
        {
            return false;
        }

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}

void print_logs_subsystem(const char* subsystem) 
{
    uint32_t start = 0;


    if(log_index >= LOG_BUFFER_SIZE) 
    {
        start = log_index % LOG_BUFFER_SIZE;
    }

    for(uint32_t i = 0; i < log_count; i++) 
    {
        uint32_t id = (start + i) % LOG_BUFFER_SIZE;

        if(string_equal(logs[id].subsystem, subsystem)) 
        {
            print_log_entry(&logs[id]);
        }
    }
}

void clear_logs() 
{
    log_index = 0;
    log_count = 0;

    for(uint32_t i = 0; i < LOG_BUFFER_SIZE; i++) 
    {
        logs[i].message[0] = 0;
        logs[i].subsystem[0] = 0;
    }
}

void print_logs() 
{
    uint32_t start = 0;

    if(log_index >= LOG_BUFFER_SIZE) 
    {
        start = log_index % LOG_BUFFER_SIZE;
    }

    for(uint32_t i = 0; i < log_count; i++)
    {
        uint32_t id = (start + i) % LOG_BUFFER_SIZE;

        print_log_entry(&logs[id]);
    }
}