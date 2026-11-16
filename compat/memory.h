// compat/memory.h  —— POSIX 上没有 <memory.h>，转发到标准头
#pragma once
#ifndef _WIN32
#include <string.h>
#endif
