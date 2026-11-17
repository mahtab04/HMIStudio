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
EXPORT int S7_CALL Cli_SetParam(std::uintptr_t, int, void*)
{
    return 0;
}
EXPORT int S7_CALL Cli_DBRead(std::uintptr_t, int db, int start, int count, void* buffer)
{
    if (db == 999)
        return 1234;
    const unsigned char data[] = {0x01, 0x00, 0x00, 0x2A, 0x41, 0x48, 0x00, 0x00};
    if (start < 0 || count < 0 || start + count > int(sizeof(data)))
        return 4321;
    std::memcpy(buffer, data + start, std::size_t(count));
    return 0;
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
EXPORT int plc_tag_get_bit(std::int32_t, int)
{
    return 1;
}
EXPORT std::int16_t plc_tag_get_int16(std::int32_t, int)
{
    return -123;
}
EXPORT std::int32_t plc_tag_get_int32(std::int32_t, int)
{
    return 42;
}
EXPORT float plc_tag_get_float32(std::int32_t, int)
{
    return 12.5F;
}
