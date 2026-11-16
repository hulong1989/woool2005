// compat/malloc.h  —— POSIX 上没有 <malloc.h>，转发到标准头
#pragma once
#ifndef _WIN32
#include <stdlib.h>
#endif
