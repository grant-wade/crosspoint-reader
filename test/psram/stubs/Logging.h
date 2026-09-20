#pragma once

inline void psramTestLog(const char*, const char*, ...) {}
#define LOG_ERR(...) psramTestLog(__VA_ARGS__)
