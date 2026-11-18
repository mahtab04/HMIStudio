#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#define S7_CALL __stdcall
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#define S7_CALL
#endif

// Deterministic native ABI fixture. It never opens a socket.

// DB1 starts 01 00 00 2A | 41 48 00 00 (0x0100002A, then 12.5f); markers,
// inputs and outputs start at zero. Writes change them.
namespace
{
unsigned char g_db1[16] = {0x01, 0x00, 0x00, 0x2A, 0x41, 0x48, 0x00, 0x00};
unsigned char g_markers[16] = {};
unsigned char g_inputs[16] = {};
unsigned char g_outputs[16] = {};
int g_port = 102;
} // namespace

EXPORT std::uintptr_t S7_CALL Cli_Create()
{
    return 1;
}
EXPORT void S7_CALL Cli_Destroy(std::uintptr_t* client)
{
    *client = 0;
}
EXPORT int S7_CALL Cli_ConnectTo(std::uintptr_t client, const char*, int, int)
{
    return client == 1 ? 0 : 1;
}
EXPORT int S7_CALL Cli_SetParam(std::uintptr_t, int parameter, void* value)
{
    if (parameter == 2)
    {
        unsigned short port = 0;
        std::memcpy(&port, value, sizeof port);
        g_port = port;
    }
    return 0;
}
namespace
{

unsigned char* areaOf(int area, int db)
{
    switch (area)
    {
    case 0x81:
        return g_inputs;
    case 0x82:
        return g_outputs;
    case 0x83:
        return g_markers;
    case 0x84:
        return db == 1 ? g_db1 : nullptr;
    default:
        return nullptr;
    }
}
} // namespace

EXPORT int S7_CALL Cli_ReadArea(std::uintptr_t, int area, int db, int start, int amount,
                                int wordLength, void* buffer)
{
    if (db == 999)
        return 1234;
    unsigned char* data = areaOf(area, db);
    if (!data || wordLength != 0x02 || start < 0 || amount < 0 || start + amount > 16)
        return 4321;
    std::memcpy(buffer, data + start, std::size_t(amount));
    return 0;
}
EXPORT int S7_CALL Cli_WriteArea(std::uintptr_t, int area, int db, int start, int amount,
                                 int wordLength, void* buffer)
{
    unsigned char* data = areaOf(area, db);
    if (!data)
        return 4321;
    const auto* bytes = static_cast<const unsigned char*>(buffer);
    if (wordLength == 0x01) // one bit: start is byte * 8 + bit
    {
        if (amount != 1 || start < 0 || start / 8 >= 16)
            return 4321;
        const unsigned char mask = static_cast<unsigned char>(1U << (start % 8));
        data[start / 8] = bytes[0] ? (data[start / 8] | mask) : (data[start / 8] & ~mask);
        return 0;
    }
    if (wordLength != 0x02 || start < 0 || amount < 0 || start + amount > 16)
        return 4321;
    std::memcpy(data + start, bytes, std::size_t(amount));
    return 0;
}
// The remote port set before connecting (parameter 2), for tests.
EXPORT int fake_s7_port()
{
    return g_port;
}

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
EXPORT int plc_tag_status(std::int32_t)
{
    return 0;
}
namespace
{
int g_bit = 1;
std::int16_t g_int16 = -123;
std::int32_t g_int32 = 42;
float g_float = 12.5F;
int g_writes = 0;
} // namespace
EXPORT int plc_tag_write(std::int32_t, int)
{
    ++g_writes;
    return 0;
}
EXPORT int plc_tag_get_bit(std::int32_t, int)
{
    return g_bit;
}
EXPORT std::int16_t plc_tag_get_int16(std::int32_t, int)
{
    return g_int16;
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
EXPORT int plc_tag_set_int16(std::int32_t, int, std::int16_t value)
{
    g_int16 = value;
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
// How many plc_tag_write calls were made, for tests.
EXPORT int fake_plctag_writes()
{
    return g_writes;
}
