#include <stdint.h>
#include <stdlib.h>
extern "C" {
uint8_t* allocate(size_t size) { return static_cast<uint8_t*>(malloc((sizeof(uint8_t)) * size)); }

void free_allocated(uint8_t* data) { free(data); }
}
