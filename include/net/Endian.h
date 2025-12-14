#pragma once 

#include <cstdint>
#include <endian.h>
namespace Sock
{

inline auto HostToNetwork64(uint64_t host64)
    -> uint64_t
{
  return htobe64(host64);
}

inline auto hostToNetwork32(uint32_t host32)
    -> uint32_t
{
  return htobe32(host32);
}

inline auto HostToNetwork16(uint16_t host16)
    -> uint16_t
{
  return htobe16(host16);
}

inline auto NetworkToHost64(uint64_t net64)
    -> uint64_t
{
  return be64toh(net64);
}

inline auto networkToHost32(uint32_t net32) -> uint32_t
{
  return be32toh(net32);
}

inline auto networkToHost16(uint16_t net16) -> uint16_t
{
  return be16toh(net16);
}

}  // namespace Sock