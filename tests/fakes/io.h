#pragma once

#include <cstdio>
#include <unistd.h>

// Host stand-ins only. These declarations do not check the XDK CRT ABI.
int _commit(int descriptor);
inline int _fileno(FILE* file) { return fileno(file); }
