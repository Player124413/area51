//==============================================================================
//
//  test_dfs.cpp
//
//  Builds a synthetic but byte-accurate XDFS archive (same layout as
//  Apps/dfsTool/dfs_Build.cpp produces) and checks the reader against it:
//  header parsing, name building, lookups, reads that span data splits, chunk
//  checksums and the failure paths for damaged archives.
//
//==============================================================================

#include "test_framework.hpp"

#include "a51/dfs_archive.hpp"
#include "a51/crc16.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

using namespace a51;

//==============================================================================
//  Helpers
//==============================================================================

static std::string g_Dir;

static std::string path_of( const char* pName )
{
    return g_Dir + "/" + pName;
}

static bool write_file( const std::string& Path, const void* pData, size_t Size )
{
    FILE* pFile = fopen( Path.c_str(), "wb" );
    if( !pFile ) return false;
    bool Ok = ( fwrite( pData, 1, Size, pFile ) == Size );
    fclose( pFile );
    return Ok;
}

static std::vector< u8 > make_pattern( size_t Size, u8 Seed )
{
    std::vector< u8 > Data( Size );
    for( size_t i = 0; i < Size; i++ )
        Data[i] = (u8)( ( i * 7u + Seed ) & 0xFF );
    return Data;
}

//------------------------------------------------------------------------------
//  The archive we build:
//
//      file 0  AUDIO\MUSIC.BIN     40000 bytes  at data offset        0
//      file 1  STRINGS\STRINGS.TXT 20000 bytes  at data offset    65536
//      file 2  BOOT.CFG              100 bytes  at data offset    85536
//
//      test.000  65536 bytes (file 0 + zero padding to a chunk boundary)
//      test.001  20100 bytes (file 1 + file 2)
//
//  Chunk checksums therefore only exist for test.000 (two 32768 byte chunks).
//------------------------------------------------------------------------------

struct BuiltArchive
{
    std::vector< u8 > Music;
    std::vector< u8 > Strings;
    std::vector< u8 > Boot;
    std::vector< u8 > Split0;
    std::vector< u8 > Split1;
};

static bool build_archive( const std::string& BaseName, BuiltArchive& Out, u32 Magic = A51_DFS_MAGIC )
{
    Out.Music   = make_pattern( 40000, 3 );
    Out.Strings = make_pattern( 20000, 91 );
    Out.Boot    = make_pattern(   100, 17 );

    // Data splits.
    Out.Split0.assign( 65536, 0 );
    memcpy( &Out.Split0[0], &Out.Music[0], Out.Music.size() );

    Out.Split1.clear();
    Out.Split1.insert( Out.Split1.end(), Out.Strings.begin(), Out.Strings.end() );
    Out.Split1.insert( Out.Split1.end(), Out.Boot.begin(),    Out.Boot.end() );

    if( !write_file( BaseName + ".000", &Out.Split0[0], Out.Split0.size() ) ) return false;
    if( !write_file( BaseName + ".001", &Out.Split1[0], Out.Split1.size() ) ) return false;

    // Chunk checksums of split 0.
    u16 Crc0 = crc16_buffer( &Out.Split0[0],     32768, 0 );
    u16 Crc1 = crc16_buffer( &Out.Split0[32768], 32768, 0 );

    // String dictionary: plain concatenation of NUL terminated strings.
    std::vector< char > Dict;
    struct { const char* pStr; u32 Offset; } Entries[8];
    const char* pWords[8] = { "", "AUDIO\\", "MUSIC", ".BIN", "STRINGS\\", "STRINGS", ".TXT", "BOOT" };
    for( int i = 0; i < 8; i++ )
    {
        Entries[i].Offset = (u32)Dict.size();
        size_t Len = strlen( pWords[i] );
        Dict.insert( Dict.end(), pWords[i], pWords[i] + Len );
        Dict.push_back( 0 );
    }

    // Metadata.
    std::vector< u8 > Meta;
    const u32 OffSubTable = 48;
    const u32 OffFiles    = OffSubTable + 2 * 8;
    const u32 OffChecks   = OffFiles + 3 * 24;
    const u32 OffStrings  = OffChecks + 2 * 2;

    auto PutU32 = [&]( u32 V )
    {
        u8 B[4];
        a51_write_le32( B, V );
        Meta.insert( Meta.end(), B, B + 4 );
    };
    auto PutU16 = [&]( u16 V )
    {
        u8 B[2];
        a51_write_le16( B, V );
        Meta.insert( Meta.end(), B, B + 2 );
    };

    // Sub file table: { offset of the *next* split, checksum base }.
    PutU32( 65536 ); PutU32( 0 );
    PutU32( 85636 ); PutU32( 2 );

    // File table.
    struct Entry { u32 Path, N1, N2, Ext, Offset, Length; };
    Entry Files[3] =
    {
        { Entries[1].Offset, Entries[2].Offset, Entries[0].Offset, Entries[3].Offset,     0, 40000 },
        { Entries[4].Offset, Entries[5].Offset, Entries[0].Offset, Entries[6].Offset, 65536, 20000 },
        { Entries[0].Offset, Entries[7].Offset, Entries[0].Offset, Entries[3].Offset, 85536,   100 },
    };
    for( int i = 0; i < 3; i++ )
    {
        PutU32( Files[i].Path   );
        PutU32( Files[i].N1     );
        PutU32( Files[i].N2     );
        PutU32( Files[i].Ext    );
        PutU32( Files[i].Offset );
        PutU32( Files[i].Length );
    }

    PutU16( Crc0 );
    PutU16( Crc1 );

    // Header.
    u8 Header[ A51_DFS_HEADER_SIZE ];
    memset( Header, 0, sizeof( Header ) );
    a51_write_le32( Header + 0,  Magic );
    a51_write_le32( Header + 4,  A51_DFS_VERSION );
    a51_write_le32( Header + 8,  0 );                 // checksum, fixed below
    a51_write_le32( Header + 12, 2048 );              // sector size
    a51_write_le32( Header + 16, 49152 );             // split size
    a51_write_le32( Header + 20, 3 );                 // nFiles
    a51_write_le32( Header + 24, 2 );                 // nSubFiles
    a51_write_le32( Header + 28, (u32)Dict.size() );  // strings length
    a51_write_le32( Header + 32, OffSubTable );
    a51_write_le32( Header + 36, OffFiles );
    a51_write_le32( Header + 40, OffChecks );
    a51_write_le32( Header + 44, OffStrings );

    // Checksum over header + metadata, exactly like dfs_Build.cpp does.
    u16 Sum = 0;
    for( int i = 0; i < A51_DFS_HEADER_SIZE; i++ ) Sum = crc16_apply_byte( Header[i], Sum );
    for( size_t i = 0; i < Meta.size(); i++ )      Sum = crc16_apply_byte( Meta[i],   Sum );
    a51_write_le32( Header + 8, (u32)Sum );

    // .dfs = header + metadata + padding to a 2k boundary.
    std::vector< u8 > Dfs;
    Dfs.insert( Dfs.end(), Header, Header + A51_DFS_HEADER_SIZE );
    Dfs.insert( Dfs.end(), Meta.begin(), Meta.end() );
    Dfs.insert( Dfs.end(), Dict.begin(), Dict.end() );
    while( Dfs.size() % 2048 != 0 ) Dfs.push_back( 0 );

    return write_file( BaseName + ".dfs", &Dfs[0], Dfs.size() );
}

//==============================================================================
//  Tests
//==============================================================================

TEST( dfs_open_and_enumerate )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "test" ), B ) );

    DfsArchive A;
    CHECK( A.Open( path_of( "test.dfs" ).c_str() ) );
    if( !A.IsOpen() )
    {
        ::test::fail( __FILE__, __LINE__, std::string( "open failed: " ) + A.GetError() );
        return;
    }

    CHECK_EQ_INT( A.GetVersion(),      3 );
    CHECK_EQ_INT( A.GetFileCount(),    3 );
    CHECK_EQ_INT( A.GetSubFileCount(), 2 );
    CHECK_EQ_INT( A.GetSectorSize(),   2048 );
    CHECK_EQ_INT( A.GetSplitSize(),    49152 );
    CHECK_EQ_INT( (long long)A.GetDataSize(), 65536 + 20100 );

    char Name[512];
    CHECK( A.GetFileName( 0, Name, sizeof( Name ) ) ); CHECK_STR( Name, "AUDIO/MUSIC.BIN" );
    CHECK( A.GetFileName( 1, Name, sizeof( Name ) ) ); CHECK_STR( Name, "STRINGS/STRINGS.TXT" );
    CHECK( A.GetFileName( 2, Name, sizeof( Name ) ) ); CHECK_STR( Name, "BOOT.BIN" );

    // Lengths straight from the table.
    CHECK_EQ_INT( A.GetFileInfo( 0 ).Length, 40000 );
    CHECK_EQ_INT( A.GetFileInfo( 1 ).Length, 20000 );
    CHECK_EQ_INT( A.GetFileInfo( 2 ).Length,   100 );
}

//------------------------------------------------------------------------------

TEST( dfs_lookup_is_case_insensitive )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "lookup" ), B ) );

    DfsArchive A;
    CHECK( A.Open( path_of( "lookup.dfs" ).c_str() ) );

    CHECK_EQ_INT( A.FindFile( "AUDIO/MUSIC.BIN" ),      0 );
    CHECK_EQ_INT( A.FindFile( "audio/music.bin" ),      0 );   // lower case
    CHECK_EQ_INT( A.FindFile( "AUDIO\\MUSIC.BIN" ),     0 );   // backslashes
    CHECK_EQ_INT( A.FindFile( "Strings/Strings.Txt" ),  1 );
    CHECK_EQ_INT( A.FindFile( "boot.bin" ),             2 );
    CHECK_EQ_INT( A.FindFile( "does/not/exist.bin" ),  -1 );
    CHECK_EQ_INT( A.FindFile( "" ),                    -1 );
    CHECK_EQ_INT( A.FindFile( nullptr ),               -1 );

    CHECK_EQ_INT( A.GetFileLength( "audio/music.bin" ), 40000 );
    CHECK_EQ_INT( A.GetFileLength( "missing.bin" ),         0 );
}

//------------------------------------------------------------------------------

TEST( dfs_read_whole_files )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "read" ), B ) );

    DfsArchive A;
    CHECK( A.Open( path_of( "read.dfs" ).c_str() ) );

    // File 0 lives entirely in split 0.
    std::vector< u8 > Buf( 40000, 0xAA );
    CHECK_EQ_INT( A.Read( 0, 0, &Buf[0], 40000 ), 40000 );
    CHECK( memcmp( &Buf[0], &B.Music[0], 40000 ) == 0 );

    // File 1 lives entirely in split 1.
    Buf.assign( 20000, 0xAA );
    CHECK_EQ_INT( A.Read( 1, 0, &Buf[0], 20000 ), 20000 );
    CHECK( memcmp( &Buf[0], &B.Strings[0], 20000 ) == 0 );

    // File 2 as well.
    Buf.assign( 100, 0xAA );
    CHECK_EQ_INT( A.Read( 2, 0, &Buf[0], 100 ), 100 );
    CHECK( memcmp( &Buf[0], &B.Boot[0], 100 ) == 0 );

    // By name.
    Buf.assign( 100, 0xAA );
    CHECK_EQ_INT( A.ReadByName( "BOOT.BIN", 0, &Buf[0], 100 ), 100 );
    CHECK( memcmp( &Buf[0], &B.Boot[0], 100 ) == 0 );
}

//------------------------------------------------------------------------------

TEST( dfs_read_partial_and_overlong )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "part" ), B ) );

    DfsArchive A;
    CHECK( A.Open( path_of( "part.dfs" ).c_str() ) );

    u8 Buf[ 512 ];

    // Middle of a file.
    CHECK_EQ_INT( A.Read( 0, 1000, Buf, 500 ), 500 );
    CHECK( memcmp( Buf, &B.Music[1000], 500 ) == 0 );

    // Asking for more than is left must clamp, not overrun.
    CHECK_EQ_INT( A.Read( 0, 39900, Buf, 500 ), 100 );
    CHECK( memcmp( Buf, &B.Music[39900], 100 ) == 0 );

    // Past the end reads nothing.
    CHECK_EQ_INT( A.Read( 0, 40000, Buf, 10 ), 0 );
    CHECK_EQ_INT( A.Read( 0, 99999, Buf, 10 ), 0 );

    // Bad index.
    CHECK_EQ_INT( A.Read( -1, 0, Buf, 10 ), 0 );
    CHECK_EQ_INT( A.Read( 99, 0, Buf, 10 ), 0 );
}

//------------------------------------------------------------------------------

TEST( dfs_read_across_split_boundary )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "span" ), B ) );

    DfsArchive A;
    CHECK( A.Open( path_of( "span.dfs" ).c_str() ) );

    // Split 0 is 65536 bytes, so reading at 65000 crosses into split 1:
    // 536 bytes of end-of-split padding, then the start of file 1.
    const s32 Total = 1000;
    std::vector< u8 > Buf( Total, 0xAA );
    CHECK_EQ_INT( A.ReadRaw( 65000, &Buf[0], Total ), Total );

    CHECK( memcmp( &Buf[0], &B.Split0[65000], 536 ) == 0 );
    CHECK( memcmp( &Buf[536], &B.Split1[0], Total - 536 ) == 0 );
}

//------------------------------------------------------------------------------

TEST( dfs_checksum_verification )
{
    BuiltArchive B;
    const std::string Base = path_of( "crc" );
    CHECK( build_archive( Base, B ) );

    DfsArchive A;
    CHECK( A.Open( ( Base + ".dfs" ).c_str() ) );

    s32 Bad = -1, Checked = -1;
    CHECK( A.VerifyChecksums( 0, &Bad, &Checked ) );
    CHECK_EQ_INT( Checked, 2 );     // split 0 has two full 32k chunks
    CHECK_EQ_INT( Bad,     0 );

    // Corrupt one byte inside the first chunk and make sure it is noticed.
    {
        FILE* pFile = fopen( ( Base + ".000" ).c_str(), "r+b" );
        CHECK( pFile != nullptr );
        if( pFile )
        {
            u8 Byte = 0x00;
            fseek( pFile, 1234, SEEK_SET );
            fread( &Byte, 1, 1, pFile );
            Byte ^= 0xFF;
            fseek( pFile, 1234, SEEK_SET );
            fwrite( &Byte, 1, 1, pFile );
            fclose( pFile );
        }
    }

    DfsArchive A2;
    CHECK( A2.Open( ( Base + ".dfs" ).c_str() ) );
    Bad = -1; Checked = -1;
    CHECK( A2.VerifyChecksums( 0, &Bad, &Checked ) );
    CHECK_EQ_INT( Checked, 2 );
    CHECK_EQ_INT( Bad,     1 );

    // MaxChunks must be honoured.
    Bad = -1; Checked = -1;
    CHECK( A2.VerifyChecksums( 1, &Bad, &Checked ) );
    CHECK_EQ_INT( Checked, 1 );
}

//------------------------------------------------------------------------------

TEST( dfs_rejects_damaged_archives )
{
    BuiltArchive B;
    CHECK( build_archive( path_of( "bad" ), B ) );

    // Wrong magic.
    {
        const std::string Base = path_of( "badmagic" );
        CHECK( build_archive( Base, B, 0xDEADBEEF ) );

        DfsArchive A;
        CHECK( !A.Open( ( Base + ".dfs" ).c_str() ) );
        CHECK( strlen( A.GetError() ) > 0 );
    }

    // Header too small.
    {
        u8 Tiny[10] = { 0 };
        CHECK( write_file( path_of( "tiny.dfs" ), Tiny, sizeof( Tiny ) ) );

        DfsArchive A;
        CHECK( !A.Open( path_of( "tiny.dfs" ).c_str() ) );
    }

    // Missing data splits.
    {
        const std::string Base = path_of( "nosplits" );
        CHECK( build_archive( Base, B ) );
        ::remove( ( Base + ".000" ).c_str() );
        ::remove( ( Base + ".001" ).c_str() );

        DfsArchive A;
        CHECK( !A.Open( ( Base + ".dfs" ).c_str() ) );
        CHECK( strstr( A.GetError(), "no data splits" ) != nullptr );
    }

    // Non existent file.
    {
        DfsArchive A;
        CHECK( !A.Open( path_of( "nope.dfs" ).c_str() ) );
        CHECK( !A.Open( nullptr ) );
    }
}

//------------------------------------------------------------------------------

TEST( dfs_partial_dump_still_reads )
{
    // A dump with a missing split must not crash and must still answer queries
    // about the archive contents.
    BuiltArchive B;
    const std::string Base = path_of( "partial" );
    CHECK( build_archive( Base, B ) );
    ::remove( ( Base + ".001" ).c_str() );

    DfsArchive A;
    CHECK( A.Open( ( Base + ".dfs" ).c_str() ) );
    CHECK_EQ_INT( A.GetFileCount(), 3 );

    // File 0 is still readable.
    std::vector< u8 > Buf( 40000, 0xAA );
    CHECK_EQ_INT( A.Read( 0, 0, &Buf[0], 40000 ), 40000 );
    CHECK( memcmp( &Buf[0], &B.Music[0], 40000 ) == 0 );

    // File 1 is gone - it must read zero bytes, not garbage.
    Buf.assign( 20000, 0xAA );
    CHECK_EQ_INT( A.Read( 1, 0, &Buf[0], 20000 ), 0 );
}

//------------------------------------------------------------------------------

TEST( dfs_folder_scan )
{
    // Its own root so the archives created by the other tests do not interfere.
    std::string Root = g_Dir + "/scanroot";
    ::mkdir( Root.c_str(), 0755 );

    BuiltArchive B;
    CHECK( build_archive( Root + "/scanme", B ) );

    // A nested folder as well, to prove the walk is recursive.
    std::string Sub = Root + "/nested";
    ::mkdir( Sub.c_str(), 0755 );
    CHECK( build_archive( Sub + "/deep", B ) );

    DfsSummary Out[8];
    s32 n = dfs_scan_folder( Root.c_str(), 4, Out, 8 );
    CHECK_EQ_INT( n, 2 );

    s32 Valid = 0;
    for( s32 i = 0; i < n; i++ )
        if( Out[i].Valid && Out[i].FileCount == 3 ) Valid++;
    CHECK_EQ_INT( Valid, 2 );
    CHECK( Out[0].TotalSize > 65536 );
    CHECK_EQ_INT( Out[0].SubFileCount, 2 );

    // Depth 0 must not descend into "nested".
    CHECK_EQ_INT( dfs_scan_folder( Root.c_str(), 0, Out, 8 ), 1 );

    CHECK( dfs_folder_size( Root.c_str(), 4 ) > 65536 );
    CHECK_EQ_INT( dfs_scan_folder( nullptr, 4, Out, 8 ), 0 );
    CHECK_EQ_INT( dfs_scan_folder( Root.c_str(), 4, nullptr, 8 ), 0 );
    CHECK_EQ_INT( dfs_scan_folder( Root.c_str(), 4, Out, 0 ), 0 );
}

//------------------------------------------------------------------------------

TEST( dfs_crc16_table_matches_engine )
{
    // Spot checks against the table shipped in Apps/dfsTool/dfs_Build.cpp.
    CHECK_EQ_INT( g_Crc16Table[0],   0x0000 );
    CHECK_EQ_INT( g_Crc16Table[1],   0x1021 );
    CHECK_EQ_INT( g_Crc16Table[255], 0x1EF0 );

    // "123456789" with this polynomial and no reflection/reflection-out.
    const char* p = "123456789";
    CHECK_EQ_INT( crc16_buffer( p, 9, 0 ), 0x31C3 );

    // Seeded and split computation must agree with a single pass.
    u8 Data[ 256 ];
    for( int i = 0; i < 256; i++ ) Data[i] = (u8)( i * 3 );
    u16 Whole = crc16_buffer( Data, 256, 0 );
    u16 Split = crc16_buffer( Data + 100, 156, crc16_buffer( Data, 100, 0 ) );
    CHECK_EQ_INT( Whole, Split );
}

//==============================================================================

int main( void )
{
    char Template[] = "/tmp/a51dfsXXXXXX";
    const char* pDir = mkdtemp( Template );
    if( !pDir )
    {
        std::printf( "could not create a temp directory\n" );
        return 2;
    }
    g_Dir = pDir;

    std::printf( "Running DFS tests in %s\n\n", g_Dir.c_str() );
    int Result = ::test::run_all();

    // Clean up.
    std::string Cmd = "rm -rf '" + g_Dir + "'";
    if( system( Cmd.c_str() ) != 0 ) { /* best effort */ }

    return Result;
}
//==============================================================================
