#include "MacAddress.hpp"

#include <algorithm>


wifi::MacAddress::MacAddress(const uint8_t* mac)
{
    std::copy_n(mac, address.size(), address.begin());
}

std::size_t wifi::MacHash::operator()(const MacAddress& mac) const
{
    std::size_t hash = 0;

    for (int i = 0; i < 6; i++)
    {
        const uint8_t byte = mac.address[i];
        hash = hash * 31 + byte;
    }

    return hash;
}
