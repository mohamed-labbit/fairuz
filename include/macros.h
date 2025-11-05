#pragma once

#include <limits>


#define BUFFER_END u'\0'
#define TABWIDTH 8
#define MAX_ALLOWED_INDENT TABWIDTH
#define DEFAULT_CAPACITY 4096
#define DEFAULT_BLOCK_SIZE 1024
#define MAX_BLOCK_SIZE std::numeric_limits<std::size_t>::max() / 2