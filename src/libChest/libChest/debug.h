#pragma once

#ifdef NDEBUG
constexpr static bool DEBUG = false;
#undef DEBUG
#else
constexpr static bool DEBUG = true;
#define DEBUG DEBUG
#endif
