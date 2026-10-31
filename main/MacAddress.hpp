#ifndef FIRMWARE_MACADDRESS_HPP
#define FIRMWARE_MACADDRESS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace wifi
{
    struct MacAddress
    {
        MacAddress() = default;
        explicit MacAddress(const uint8_t* mac);

        std::array<uint8_t, 6> address{};

        bool constexpr operator==(const MacAddress &other) const
        {
            return this->address == other.address;
        }
    };

    struct MacHash
    {
        std::size_t operator()(const MacAddress& mac) const;
    };
}

#endif //FIRMWARE_MACADDRESS_HPP
