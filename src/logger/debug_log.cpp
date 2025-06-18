#include <Arduino.h>
#include "common.h"
#include "debug_log.h"

int debug_level = DEBUG_NOTHING;

void set_debug_level(int level)
{
    debug_level = level;
}

int dprintf(int level, const char *format, ...)
{
    if (!(debug_level & level))
        return 0;

    char loc_buf[64];
    char *temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if (len < 0)
    {
        va_end(arg);
        return 0;
    };
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            va_end(arg);
            return 0;
        }
        len = vsnprintf(temp, len + 1, format, arg);
    }
    va_end(arg);

    // use time stamp;
    time_t raw_time = get_timestamp() + get_time_zone_offset() * 3600;

    struct tm *struct_time;
    struct_time = localtime(&raw_time);

    char level_char = 'N';
    switch (level)
    {
    case DEBUG_INFO:
        level_char = 'I';
        break;
    case DEBUG_ERROR:
        level_char = 'E';
        break;
    case DEBUG_DEBUG:
        level_char = 'D';
        break;
    case DEBUG_API:
        level_char = 'A';
        break;
    }

    String debug_string;

    if (1 || raw_time > 5000)
    {
        debug_string = StringFormat("[%4d%02d%02d %02d%02d%02d][%c] - %s",
                                    struct_time->tm_year + 1900, struct_time->tm_mon + 1, struct_time->tm_mday,
                                    struct_time->tm_hour, struct_time->tm_min, struct_time->tm_sec, level_char, temp);
    }
    else
        debug_string = StringFormat("%s", temp);

    if (debug_level & DEBUG_MEMORY)
        Serial.printf("[%07dB] ", ESP.getFreeHeap());
    if (debug_string.endsWith("\n"))
        Serial.print(debug_string);
    else
        Serial.println(debug_string);

    if (temp != loc_buf)
    {
        free(temp);
    }
    return len;
}
