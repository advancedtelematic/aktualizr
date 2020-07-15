#include <cstdint>
#include <cstdlib>

extern "C" {
// NOLINTNEXTLINE(cppcoreguidelines-no-malloc, hicpp-no-malloc)
uint8_t* allocate(size_t size) { return static_cast<uint8_t*>(malloc((sizeof(uint8_t)) * size)); }

// NOLINTNEXTLINE(cppcoreguidelines-no-malloc, hicpp-no-malloc)
void free_allocated(uint8_t* data) { free(data); }
}
