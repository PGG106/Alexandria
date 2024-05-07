#include <array>
#include <cstdint>

extern std::array<uint64_t, 8192> keys;
extern std::array<int, 8192> cuckooMoves;

constexpr auto H1(uint64_t key)
{
    return static_cast<std::size_t>(key & 0x1FFF);
}

constexpr auto H2(uint64_t key)
{
    return static_cast<std::size_t>((key >> 16) & 0x1FFF);
}
