#pragma once

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <string>
#include <vector>
#include <algorithm>

#if defined(DEBUG)
#	define LOG_DEBUG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#else
#	define LOG_DEBUG(fmt, ...)
#endif
