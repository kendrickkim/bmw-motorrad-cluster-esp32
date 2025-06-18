/*********************************************
 * Logger for ESP32-s3
 * Author: Kendrick Kim(kjkim@mobipintech.com)
 * Created: 2024-11-01
 * Modified: 2024-11-01
 */

#pragma once
enum
{
    DEBUG_NOTHING = 0,
    DEBUG_INFO = 1 << 0,
    DEBUG_DEBUG = 1 << 1,
    DEBUG_ERROR = 1 << 2,
    DEBUG_API = 1 << 3,
    DEBUG_MEMORY = 1 << 4,
    DEBUG_ALL = DEBUG_INFO | DEBUG_ERROR | DEBUG_DEBUG | DEBUG_API,
};

#define DPRINTF(...) dprintf(DEBUG_INFO, __VA_ARGS__)
#define D_INFO(...) dprintf(DEBUG_INFO, __VA_ARGS__)
#define D_DEBUG(...) dprintf(DEBUG_DEBUG, __VA_ARGS__)
#define D_ERROR(...) dprintf(DEBUG_ERROR, __VA_ARGS__)
#define D_API(...) dprintf(DEBUG_API, __VA_ARGS__)

void set_debug_level(int level);
int dprintf(int level, const char *format, ...);