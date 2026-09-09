//==============================================================================
//
//  crc16.hpp
//
//  The CCITT-style CRC16 used by the Area 51 .dfs archives for its chunk
//  checksum table.  The table is copied verbatim from the original source
//  (Apps/dfsTool/dfs_Build.cpp, xCore/Entropy/IOManager/io_device.cpp) so the
//  values match the shipped game data exactly.
//
//==============================================================================

#ifndef A51_CRC16_HPP
#define A51_CRC16_HPP

#include "a51_types.hpp"

namespace a51 {

// Table from the original engine (poly 0x1021, no reflection).
extern const u16 g_Crc16Table[256];

// Streaming helper - mirrors crc16ApplyByte() from dfs_Build.cpp.
inline u16 crc16_apply_byte( u8 v, u16 crc )
{
    return (u16)( ( crc << 8 ) ^ g_Crc16Table[( ( crc >> 8 ) ^ v ) & 255] );
}

// CRC16 over a block of memory.
u16 crc16_buffer( const void* pData, s32 Length, u16 Seed = 0 );

} // namespace a51

#endif // A51_CRC16_HPP
//==============================================================================
