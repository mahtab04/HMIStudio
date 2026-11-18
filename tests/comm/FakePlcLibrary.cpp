#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Deterministic libplctag fixture (Allen-Bradley). It never opens a socket.
// Reads return 1 (bit), 42 (int32) and 12.5 (float) until written.

namespace
{
int g_bit = 1;
std::int32_t g_int32 = 42;
float g_float = 12.5F;
} // namespace

EXPORT std::int32_t plc_tag_create(const char* attributes, int)
{
    return std::strstr(attributes, "gateway=") && std::strstr(attributes, "name=") ? 17 : -1;
}
EXPORT int plc_tag_destroy(std::int32_t)
{
    return 0;
}
EXPORT int plc_tag_read(std::int32_t, int)
{
    return 0;
}
EXPORT int plc_tag_write(std::int32_t, int)
{
    return 0;
}
EXPORT int plc_tag_status(std::int32_t)
{
    return 0;
}
EXPORT int plc_tag_get_bit(std::int32_t, int)
{
    return g_bit;
}
EXPORT std::int32_t plc_tag_get_int32(std::int32_t, int)
{
    return g_int32;
}
EXPORT float plc_tag_get_float32(std::int32_t, int)
{
    return g_float;
}
EXPORT int plc_tag_set_bit(std::int32_t, int, int value)
{
    g_bit = value;
    return 0;
}
EXPORT int plc_tag_set_int32(std::int32_t, int, std::int32_t value)
{
    g_int32 = value;
    return 0;
}
EXPORT int plc_tag_set_float32(std::int32_t, int, float value)
{
    g_float = value;
    return 0;
}
