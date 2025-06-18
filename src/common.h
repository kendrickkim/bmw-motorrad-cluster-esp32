#pragma once
#include <Arduino.h>
#include <vector>
#include "logger/debug_log.h"

template <typename... Args>
String StringFormat(const String &format, Args... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size <= 0)
        printf("Error during String formatting\n");
    char *buf = new char[size + 1];
    snprintf(buf, size, format.c_str(), args...);
    String r(buf);
    delete[] buf;
    return r; // We don't want the '\0' inside
}

// getting string array from splited string
std::vector<String> splitString(const String &str, const String &delimiter);

String button_to_string(bool button);
void set_time_offset(uint32_t offset);
time_t get_timestamp();
void set_time_zone_offset(int offset);
int get_time_zone_offset();
String bool_to_string(bool value);