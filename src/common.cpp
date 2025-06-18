#include "common.h"
#include <Arduino.h>

std::vector<String> splitString(const String &str, const String &delimiter)
{
    std::vector<String> result;
    int start = 0;
    int end = str.indexOf(delimiter);

    while (end != -1)
    {
        result.push_back(str.substring(start, end));
        start = end + delimiter.length();
        end = str.indexOf(delimiter, start);
    }
    String lastToken = str.substring(start, end);
    lastToken.trim();
    // Check if the last token is empty
    if (lastToken.length() > 0)
        result.push_back(lastToken);
    return result;
}

String button_to_string(bool button)
{
    return button ? "O" : "X";
}

uint32_t time_offset_sec = 0;
int time_zone_offset = 0;
void set_time_zone_offset(int offset)
{
    time_zone_offset = offset;
}

int get_time_zone_offset()
{
    return time_zone_offset;
}

void set_time_offset(uint32_t offset)
{
    time_offset_sec = offset;
}

time_t get_timestamp()
{
    time_t raw_time = time(NULL);
    raw_time += time_offset_sec;
    return raw_time;
}
String bool_to_string(bool value)
{
    return value ? "true" : "false";
}