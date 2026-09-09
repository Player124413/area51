//==============================================================================
//
//  dfs_archive.cpp - implementation of the XDFS reader.
//
//==============================================================================

#include "dfs_archive.hpp"
#include "crc16.hpp"
#include "log.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

namespace a51 {

//==============================================================================
//  Local helpers
//==============================================================================

static s64 file_length_fd( int Fd )
{
    if( Fd < 0 ) return -1;
    off_t Len = ::lseek( Fd, 0, SEEK_END );
    if( Len < 0 ) return -1;
    ::lseek( Fd, 0, SEEK_SET );
    return (s64)Len;
}

// Read exactly Length bytes at Offset.  Handles short reads and EINTR.
static s32 pread_full( int Fd, u64 Offset, void* pDst, s32 Length )
{
    u8*  p      = (u8*)pDst;
    s32  Done   = 0;

    while( Done < Length )
    {
        ssize_t n = ::pread( Fd, p + Done, (size_t)( Length - Done ), (off_t)( Offset + (u64)Done ) );
        if( n < 0 )
        {
            if( errno == EINTR ) continue;
            break;
        }
        if( n == 0 ) break;       // end of file
        Done += (s32)n;
    }
    return Done;
}

//==============================================================================
//  Construction / destruction
//==============================================================================

DfsArchive::DfsArchive()
{
    m_IsOpen          = false;
    m_Path[0]         = 0;
    m_Error[0]        = 0;
    m_Version         = 0;
    m_SectorSize      = 0;
    m_SplitSize       = 0;
    m_FileCount       = 0;
    m_SubFileCount    = 0;
    m_StringsLength   = 0;
    m_HeaderChecksum  = 0;
    m_pStrings        = nullptr;
    m_pFiles          = nullptr;
    m_pSubFiles       = nullptr;
    m_pChecksums      = nullptr;
    m_ChecksumCount   = 0;
    m_DataSize        = 0;
    m_pHashSlots      = nullptr;
}

//------------------------------------------------------------------------------

DfsArchive::~DfsArchive()
{
    Close();
}

//------------------------------------------------------------------------------

void DfsArchive::SetError( const char* pFmt, ... )
{
    va_list Args;
    va_start( Args, pFmt );
    vsnprintf( m_Error, sizeof( m_Error ), pFmt, Args );
    va_end( Args );
}

//------------------------------------------------------------------------------

void DfsArchive::Close( void )
{
    if( m_pSubFiles )
    {
        for( s32 i = 0; i < m_SubFileCount; i++ )
            if( m_pSubFiles[i].Fd >= 0 ) ::close( m_pSubFiles[i].Fd );
        free( m_pSubFiles );
        m_pSubFiles = nullptr;
    }

    free( m_pStrings   ); m_pStrings   = nullptr;
    free( m_pFiles     ); m_pFiles     = nullptr;
    free( m_pChecksums ); m_pChecksums = nullptr;
    free( m_pHashSlots ); m_pHashSlots = nullptr;

    m_IsOpen        = false;
    m_FileCount     = 0;
    m_SubFileCount  = 0;
    m_ChecksumCount = 0;
    m_DataSize      = 0;
}

//==============================================================================
//  Hashing (same DJB2 variant the engine's dictionary uses, but case
//  insensitive so "Audio\Music.dfs" and "audio/music.dfs" collide).
//==============================================================================

u32 DfsArchive::HashPath( const char* pPath ) const
{
    u32 Hash = 5381;
    for( const char* p = pPath; *p; p++ )
    {
        char c = *p;
        if( c == '\\' ) c = '/';
        Hash = ( Hash * 33 ) ^ (u32)a51_tolower( c );
    }
    return Hash;
}

//------------------------------------------------------------------------------

void DfsArchive::BuildHash( void )
{
    m_pHashSlots = (s32*)malloc( sizeof( s32 ) * A51_DFS_HASH_SLOTS );
    if( !m_pHashSlots ) return;

    for( s32 i = 0; i < A51_DFS_HASH_SLOTS; i++ )
        m_pHashSlots[i] = 0;

    for( s32 i = 0; i < m_FileCount; i++ )
    {
        u32 Slot = m_pFiles[i].Hash % A51_DFS_HASH_SLOTS;
        s32 Guard = 0;
        // First entry wins - later duplicates are simply not indexed.
        while( m_pHashSlots[Slot] != 0 && Guard < A51_DFS_HASH_SLOTS )
        {
            Slot = ( Slot + 1 ) % A51_DFS_HASH_SLOTS;
            Guard++;
        }
        if( m_pHashSlots[Slot] == 0 )
            m_pHashSlots[Slot] = i + 1;
    }
}

//==============================================================================
//  Open
//==============================================================================

bool DfsArchive::Open( const char* pPath )
{
    Close();

    if( !pPath || !pPath[0] )
    {
        SetError( "no path given" );
        return false;
    }

    snprintf( m_Path, sizeof( m_Path ), "%s", pPath );

    //--------------------------------------------------------------------------
    //  Read the header.
    //--------------------------------------------------------------------------
    int Fd = ::open( m_Path, O_RDONLY );
    if( Fd < 0 )
    {
        SetError( "cannot open '%s' (errno %d)", m_Path, errno );
        return false;
    }

    u8 Header[ A51_DFS_HEADER_SIZE ];
    if( pread_full( Fd, 0, Header, A51_DFS_HEADER_SIZE ) != A51_DFS_HEADER_SIZE )
    {
        SetError( "file too small to hold a DFS header" );
        ::close( Fd );
        return false;
    }

    u32 Magic       = a51_read_le32( Header + 0  );
    s32 Version     = a51_read_le32s( Header + 4  );
    u32 Checksum    = a51_read_le32( Header + 8  );
    s32 SectorSize  = a51_read_le32s( Header + 12 );
    u32 SplitSize   = a51_read_le32( Header + 16 );
    s32 nFiles      = a51_read_le32s( Header + 20 );
    s32 nSubFiles   = a51_read_le32s( Header + 24 );
    s32 StringsLen  = a51_read_le32s( Header + 28 );
    u32 OffSubTable = a51_read_le32( Header + 32 );
    u32 OffFiles    = a51_read_le32( Header + 36 );
    u32 OffChecks   = a51_read_le32( Header + 40 );
    u32 OffStrings  = a51_read_le32( Header + 44 );

    // The magic is written as the MSVC multi character constant 'XDFS'
    // (0x58444653).  Some third party packers write the four ASCII characters
    // instead, which reads back as 0x53464458 - accept both.
    if( Magic != A51_DFS_MAGIC && Magic != A51_DFS_MAGIC_BYTES )
    {
        SetError( "not a DFS archive (magic 0x%08X)", Magic );
        ::close( Fd );
        return false;
    }

    if( Version != A51_DFS_VERSION )
        A51_LOGW( "dfs '%s': version %d (expected %d), trying anyway", m_Path, Version, A51_DFS_VERSION );

    if( nFiles < 0 || nFiles > A51_DFS_MAX_FILES )
    {
        SetError( "corrupt header: nFiles = %d", nFiles );
        ::close( Fd );
        return false;
    }
    if( nSubFiles < 1 || nSubFiles > A51_DFS_MAX_SUBFILES )
    {
        SetError( "corrupt header: nSubFiles = %d", nSubFiles );
        ::close( Fd );
        return false;
    }
    if( StringsLen < 0 || StringsLen > A51_DFS_MAX_STRINGS )
    {
        SetError( "corrupt header: StringsLength = %d", StringsLen );
        ::close( Fd );
        return false;
    }
    if( OffFiles < A51_DFS_HEADER_SIZE || OffStrings < A51_DFS_HEADER_SIZE )
    {
        SetError( "corrupt header: table offsets point inside the header" );
        ::close( Fd );
        return false;
    }

    m_Version        = Version;
    m_SectorSize     = SectorSize;
    m_SplitSize      = SplitSize;
    m_HeaderChecksum = Checksum;
    m_StringsLength  = StringsLen;

    //--------------------------------------------------------------------------
    //  Sub file table.
    //--------------------------------------------------------------------------
    m_pSubFiles = (DfsSubFile*)calloc( (size_t)nSubFiles, sizeof( DfsSubFile ) );
    if( !m_pSubFiles )
    {
        SetError( "out of memory" );
        ::close( Fd );
        return false;
    }
    m_SubFileCount = nSubFiles;

    u32* pSubTable = nullptr;
    if( OffSubTable >= A51_DFS_HEADER_SIZE )
    {
        pSubTable = (u32*)malloc( sizeof( u32 ) * 2 * (size_t)nSubFiles );
        if( pSubTable &&
            pread_full( Fd, OffSubTable, pSubTable, (s32)( sizeof( u32 ) * 2 * (size_t)nSubFiles ) )
                != (s32)( sizeof( u32 ) * 2 * (size_t)nSubFiles ) )
        {
            free( pSubTable );
            pSubTable = nullptr;
        }
    }

    //--------------------------------------------------------------------------
    //  Data splits:  <name>.000, <name>.001, ...
    //--------------------------------------------------------------------------
    {
        char Base[ A51_DFS_MAX_PATH_LEN ];
        snprintf( Base, sizeof( Base ), "%s", m_Path );

        // Strip the extension.
        char* pDot = nullptr;
        for( char* p = Base; *p; p++ )
            if( *p == '.' ) pDot = p;
        char* pSlash = nullptr;
        for( char* p = Base; *p; p++ )
            if( *p == '/' || *p == '\\' ) pSlash = p;
        if( pDot && ( !pSlash || pDot > pSlash ) ) *pDot = 0;

        // The split names are "<base>.000" - make sure they still fit.
        if( strlen( Base ) > A51_DFS_MAX_PATH_LEN - 8 )
        {
            SetError( "path too long" );
            free( pSubTable );
            ::close( Fd );
            Close();
            return false;
        }

        u64 Cumulative = 0;
        s32 Missing    = 0;

        for( s32 i = 0; i < nSubFiles; i++ )
        {
            DfsSubFile& Sub = m_pSubFiles[i];
            Sub.Fd = -1;
            snprintf( Sub.Path, sizeof( Sub.Path ), "%.480s.%03d", Base, i );
            Sub.GlobalStart   = Cumulative;
            Sub.ChecksumIndex = pSubTable ? a51_read_le32( pSubTable + i * 2 + 1 ) : 0;

            Sub.Fd = ::open( Sub.Path, O_RDONLY );
            if( Sub.Fd >= 0 )
            {
                s64 Len = file_length_fd( Sub.Fd );
                if( Len < 0 ) Len = 0;
                Sub.Length = (u64)Len;
                Cumulative += Sub.Length;
            }
            else
            {
                Sub.Length = 0;
                Missing++;
                // Without the split on disk we cannot know its size, so fall
                // back on the archive's own split size to keep the following
                // splits at plausible offsets.
                u64 Guess = ( m_SplitSize > 0 ) ? m_SplitSize : 0;
                if( pSubTable && i + 1 < nSubFiles )
                {
                    u64 Next = a51_read_le32( pSubTable + i * 2 );
                    if( Next > Cumulative ) Guess = Next - Cumulative;
                }
                Cumulative += Guess;
            }
        }

        // The table stores, for split i, the logical offset at which the *next*
        // split begins (see Apps/dfsTool/dfs_Build.cpp).  Use it to correct the
        // offsets whenever it agrees with what is on disk, which keeps partial
        // dumps (missing splits) readable.
        if( pSubTable && nSubFiles > 1 )
        {
            u64 Expected = 0;
            bool Plausible = true;
            for( s32 i = 0; i + 1 < nSubFiles; i++ )
            {
                u64 Start = a51_read_le32( pSubTable + i * 2 );
                if( Start < Expected ) { Plausible = false; break; }
                Expected = Start;
            }
            if( Plausible )
            {
                for( s32 i = 1; i < nSubFiles; i++ )
                {
                    u64 Start = a51_read_le32( pSubTable + ( i - 1 ) * 2 );
                    // Only override when the split before it is missing, or when
                    // both agree - never fight against real file sizes.
                    if( m_pSubFiles[i-1].Fd < 0 )
                        m_pSubFiles[i].GlobalStart = Start;
                }
            }
        }

        m_DataSize = Cumulative;

        if( Missing == nSubFiles )
        {
            SetError( "no data splits found (expected '%s.000')", Base );
            free( pSubTable );
            ::close( Fd );
            Close();
            return false;
        }
        if( Missing > 0 )
            A51_LOGW( "dfs '%s': %d of %d data splits missing", m_Path, Missing, nSubFiles );
    }

    //--------------------------------------------------------------------------
    //  File table.
    //--------------------------------------------------------------------------
    m_FileCount = nFiles;
    m_pFiles = (DfsFileInfo*)calloc( (size_t)a51_max( nFiles, 1 ), sizeof( DfsFileInfo ) );
    if( !m_pFiles )
    {
        SetError( "out of memory" );
        free( pSubTable );
        ::close( Fd );
        Close();
        return false;
    }

    // The four string offsets of every entry, kept until the string table has
    // been loaded (4 * u32 per file).
    u32* pOffsets = nullptr;
    if( nFiles > 0 )
    {
        pOffsets = (u32*)malloc( sizeof( u32 ) * 4 * (size_t)nFiles );
        if( !pOffsets )
        {
            SetError( "out of memory" );
            free( pSubTable );
            ::close( Fd );
            Close();
            return false;
        }

        const s32 EntrySize  = (s32)sizeof( dfs_file_raw );       // 24 bytes
        const s32 SliceCount = 512;                               // read in slices
        u8        Buf[ 512 * 24 ];

        for( s32 Base = 0; Base < nFiles; Base += SliceCount )
        {
            s32 Count = a51_min( SliceCount, nFiles - Base );
            s32 Got   = pread_full( Fd,
                                    OffFiles + (u64)Base * (u64)EntrySize,
                                    Buf,
                                    EntrySize * Count );
            if( Got != EntrySize * Count )
            {
                SetError( "file table truncated (wanted %d bytes, got %d)", EntrySize * Count, Got );
                free( pOffsets );
                free( pSubTable );
                ::close( Fd );
                Close();
                return false;
            }

            for( s32 k = 0; k < Count; k++ )
            {
                const u8*    p    = Buf + (size_t)k * (size_t)EntrySize;
                DfsFileInfo& Info = m_pFiles[ Base + k ];

                Info.DataOffset = a51_read_le32( p + 16 );
                Info.Length     = a51_read_le32( p + 20 );
                Info.Path[0]    = 0;
                Info.Hash       = 0;

                u32* pOff = pOffsets + (size_t)( Base + k ) * 4;
                pOff[0] = a51_read_le32( p + 0  );    // path
                pOff[1] = a51_read_le32( p + 4  );    // name part 1
                pOff[2] = a51_read_le32( p + 8  );    // name part 2
                pOff[3] = a51_read_le32( p + 12 );    // extension
            }
        }
    }

    //--------------------------------------------------------------------------
    //  String table.
    //--------------------------------------------------------------------------
    if( !LoadStringTable( Fd, OffStrings, (u32)StringsLen ) )
    {
        free( pOffsets );
        free( pSubTable );
        ::close( Fd );
        Close();
        return false;
    }

    // Full name = Path + Name1 + Name2 + Ext  (see dfs_BuildFileName()).
    for( s32 i = 0; i < m_FileCount; i++ )
    {
        const u32* pOff = pOffsets + (size_t)i * 4;
        char*      pOut = m_pFiles[i].Path;
        s32        Left = (s32)sizeof( m_pFiles[i].Path ) - 1;

        pOut[0] = 0;
        for( s32 k = 0; k < 4; k++ )
        {
            const char* p = Str( pOff[k] );
            while( *p && Left > 0 ) { *pOut++ = *p++; Left--; }
        }
        *pOut = 0;

        a51_normalise_path( m_pFiles[i].Path );
        m_pFiles[i].Hash = HashPath( m_pFiles[i].Path );
    }
    free( pOffsets );

    BuildHash();

    //--------------------------------------------------------------------------
    //  Checksum table (optional).
    //--------------------------------------------------------------------------
    if( OffChecks >= A51_DFS_HEADER_SIZE )
    {
        // Number of chunks = 32768 byte blocks over the whole data stream.
        u64 Chunks = ( m_DataSize + 32767 ) / 32768;
        if( Chunks == 0 ) Chunks = 1;
        if( Chunks > 4 * 1024 * 1024 ) Chunks = 4 * 1024 * 1024;

        m_pChecksums = (u16*)malloc( sizeof( u16 ) * (size_t)Chunks );
        if( m_pChecksums )
        {
            s32 Got = pread_full( Fd, OffChecks, m_pChecksums, (s32)( sizeof( u16 ) * (size_t)Chunks ) );
            m_ChecksumCount = Got / (s32)sizeof( u16 );
            if( m_ChecksumCount <= 0 )
            {
                free( m_pChecksums );
                m_pChecksums    = nullptr;
                m_ChecksumCount = 0;
            }
        }
    }

    free( pSubTable );
    ::close( Fd );

    m_IsOpen = true;
    A51_LOGI( "dfs '%s' mounted: %d files, %d splits, %llu bytes",
              m_Path, m_FileCount, m_SubFileCount, (unsigned long long)m_DataSize );
    return true;
}

//==============================================================================
//  String table
//==============================================================================

const char* DfsArchive::Str( u32 Offset ) const
{
    if( !m_pStrings ) return "";
    if( Offset >= (u32)m_StringsLength ) return "";
    // Guarantee NUL termination even for a damaged table.
    m_pStrings[ m_StringsLength - 1 ] = 0;
    return m_pStrings + Offset;
}

//------------------------------------------------------------------------------

bool DfsArchive::LoadStringTable( int Fd, u32 Offset, u32 Length )
{
    if( Length == 0 )
    {
        m_pStrings      = (char*)calloc( 1, 1 );
        m_StringsLength = 1;
        return m_pStrings != nullptr;
    }

    m_pStrings = (char*)malloc( (size_t)Length + 1 );
    if( !m_pStrings )
    {
        SetError( "out of memory for the string table" );
        return false;
    }

    s32 Got = pread_full( Fd, Offset, m_pStrings, (s32)Length );
    if( Got != (s32)Length )
    {
        SetError( "string table truncated (%d of %u bytes)", Got, Length );
        free( m_pStrings );
        m_pStrings = nullptr;
        return false;
    }
    m_pStrings[ Length ] = 0;
    m_StringsLength      = (s32)Length;
    return true;
}

//==============================================================================
//  Queries
//==============================================================================

const DfsFileInfo& DfsArchive::GetFileInfo( s32 iFile ) const
{
    static DfsFileInfo s_Empty = {};
    if( !m_pFiles || iFile < 0 || iFile >= m_FileCount ) return s_Empty;
    return m_pFiles[ iFile ];
}

//------------------------------------------------------------------------------

bool DfsArchive::GetFileName( s32 iFile, char* pOut, s32 OutSize ) const
{
    if( iFile < 0 || iFile >= m_FileCount || !pOut || OutSize <= 0 ) return false;
    snprintf( pOut, (size_t)OutSize, "%s", m_pFiles[iFile].Path );
    return true;
}

//------------------------------------------------------------------------------

s32 DfsArchive::FindFile( const char* pPath ) const
{
    if( !pPath || !m_pFiles || !m_pHashSlots ) return -1;

    char Normal[ A51_DFS_MAX_PATH_LEN ];
    snprintf( Normal, sizeof( Normal ), "%s", pPath );
    a51_normalise_path( Normal );

    u32 Hash  = HashPath( Normal );
    u32 Slot  = Hash % A51_DFS_HASH_SLOTS;

    for( s32 Guard = 0; Guard < A51_DFS_HASH_SLOTS; Guard++ )
    {
        s32 Index = m_pHashSlots[ Slot ];
        if( Index == 0 ) return -1;
        if( m_pFiles[ Index - 1 ].Hash == Hash &&
            a51_strieq( m_pFiles[ Index - 1 ].Path, Normal ) )
            return Index - 1;
        Slot = ( Slot + 1 ) % A51_DFS_HASH_SLOTS;
    }
    return -1;
}

//------------------------------------------------------------------------------

u32 DfsArchive::GetFileLength( const char* pPath ) const
{
    s32 i = FindFile( pPath );
    return ( i >= 0 ) ? m_pFiles[i].Length : 0;
}

//==============================================================================
//  Reading
//==============================================================================

s32 DfsArchive::ReadRaw( u64 GlobalOffset, void* pDst, s32 Length ) const
{
    if( Length <= 0 || !pDst ) return 0;

    u8* p    = (u8*)pDst;
    s32 Done = 0;

    while( Done < Length )
    {
        // Find the split that contains the current position.
        s32 iSub = -1;
        for( s32 i = 0; i < m_SubFileCount; i++ )
        {
            const DfsSubFile& Sub = m_pSubFiles[i];
            if( Sub.Fd < 0 || Sub.Length == 0 ) continue;
            u64 Pos = GlobalOffset + (u64)Done;
            if( Pos >= Sub.GlobalStart && Pos < Sub.GlobalStart + Sub.Length )
            {
                iSub = i;
                break;
            }
        }
        if( iSub < 0 ) break;

        const DfsSubFile& Sub   = m_pSubFiles[ iSub ];
        u64 Local               = ( GlobalOffset + (u64)Done ) - Sub.GlobalStart;
        u64 Avail               = Sub.Length - Local;
        s32 Want                = a51_min( (s32)Avail, Length - Done );

        s32 Got = pread_full( Sub.Fd, Local, p + Done, Want );
        if( Got <= 0 ) break;
        Done += Got;
        if( Got < Want ) break;
    }

    return Done;
}

//------------------------------------------------------------------------------

s32 DfsArchive::Read( s32 iFile, u32 OffsetInFile, void* pDst, s32 Length ) const
{
    if( iFile < 0 || iFile >= m_FileCount ) return 0;

    const DfsFileInfo& Info = m_pFiles[ iFile ];
    if( OffsetInFile >= Info.Length ) return 0;

    u64 Remaining = (u64)Info.Length - OffsetInFile;
    s32 Want      = a51_min( (s32)Remaining, Length );

    return ReadRaw( (u64)Info.DataOffset + OffsetInFile, pDst, Want );
}

//------------------------------------------------------------------------------

s32 DfsArchive::ReadByName( const char* pPath, u32 OffsetInFile, void* pDst, s32 Length ) const
{
    return Read( FindFile( pPath ), OffsetInFile, pDst, Length );
}

//==============================================================================
//  Checksum verification
//==============================================================================

u16 DfsArchive::GetChunkChecksum( s32 iChunk ) const
{
    if( !m_pChecksums || iChunk < 0 || iChunk >= m_ChecksumCount ) return 0;
    return m_pChecksums[ iChunk ];
}

//------------------------------------------------------------------------------

bool DfsArchive::VerifyChecksums( s32 MaxChunks, s32* pBad, s32* pChecked ) const
{
    if( pBad )     *pBad     = 0;
    if( pChecked ) *pChecked = 0;

    if( !m_pChecksums || m_ChecksumCount == 0 ) return false;

    const s32 ChunkSize = 32768;
    u8* pBuf = (u8*)malloc( (size_t)ChunkSize );
    if( !pBuf ) return false;

    s32 Bad     = 0;
    s32 Checked = 0;

    for( s32 iSub = 0; iSub < m_SubFileCount; iSub++ )
    {
        const DfsSubFile& Sub = m_pSubFiles[ iSub ];
        if( Sub.Fd < 0 || Sub.Length == 0 ) continue;

        // Only whole chunks: the engine pads the tail of the last chunk with
        // whatever was left in its buffer, so a partial chunk cannot be
        // compared reliably.
        s32 nChunks = (s32)( Sub.Length / (u64)ChunkSize );

        for( s32 c = 0; c < nChunks; c++ )
        {
            if( MaxChunks > 0 && Checked >= MaxChunks ) goto done;

            s32 Index = (s32)Sub.ChecksumIndex + c;
            if( Index >= m_ChecksumCount ) break;

            s32 Got = pread_full( Sub.Fd, (u64)c * ChunkSize, pBuf, ChunkSize );
            if( Got != ChunkSize ) break;

            u16 Expected = m_pChecksums[ Index ];
            u16 Actual   = crc16_buffer( pBuf, ChunkSize, 0 );

            Checked++;
            if( Expected != Actual ) Bad++;
        }
    }

done:
    free( pBuf );
    if( pBad )     *pBad     = Bad;
    if( pChecked ) *pChecked = Checked;
    return true;
}

//==============================================================================
//  Folder scanning
//==============================================================================

static bool ends_with_i( const char* pStr, const char* pSuffix )
{
    size_t ls = strlen( pStr );
    size_t lf = strlen( pSuffix );
    if( lf > ls ) return false;
    return a51_strieq( pStr + ( ls - lf ), pSuffix );
}

//------------------------------------------------------------------------------

static void scan_recursive( const char* pDir, s32 Depth, s32 MaxDepth,
                            DfsSummary* pOut, s32 MaxOut, s32& nOut )
{
    if( Depth > MaxDepth || nOut >= MaxOut ) return;

    DIR* pDirHandle = ::opendir( pDir );
    if( !pDirHandle ) return;

    struct dirent* pEnt;
    while( ( pEnt = ::readdir( pDirHandle ) ) != nullptr && nOut < MaxOut )
    {
        if( pEnt->d_name[0] == '.' &&
            ( pEnt->d_name[1] == 0 || ( pEnt->d_name[1] == '.' && pEnt->d_name[2] == 0 ) ) )
            continue;

        char Full[ A51_DFS_MAX_PATH_LEN ];
        snprintf( Full, sizeof( Full ), "%s/%s", pDir, pEnt->d_name );

        struct stat St;
        if( ::stat( Full, &St ) != 0 ) continue;

        if( S_ISDIR( St.st_mode ) )
        {
            scan_recursive( Full, Depth + 1, MaxDepth, pOut, MaxOut, nOut );
        }
        else if( S_ISREG( St.st_mode ) && ends_with_i( pEnt->d_name, ".dfs" ) )
        {
            DfsSummary& S = pOut[ nOut++ ];
            memset( &S, 0, sizeof( S ) );
            snprintf( S.Path, sizeof( S.Path ), "%s", Full );
            S.DfsSize = (u64)St.st_size;

            DfsArchive Archive;
            if( Archive.Open( Full ) )
            {
                S.Valid        = true;
                S.FileCount    = Archive.GetFileCount();
                S.SubFileCount = Archive.GetSubFileCount();
                S.Version      = Archive.GetVersion();
                S.TotalSize    = S.DfsSize + Archive.GetDataSize();
            }
            else
            {
                S.Valid = false;
                snprintf( S.Error, sizeof( S.Error ), "%s", Archive.GetError() );
                S.TotalSize = S.DfsSize;
            }
        }
    }

    ::closedir( pDirHandle );
}

//------------------------------------------------------------------------------

s32 dfs_scan_folder( const char* pDir, s32 MaxDepth, DfsSummary* pOut, s32 MaxOut )
{
    if( !pDir || !pOut || MaxOut <= 0 ) return 0;
    s32 nOut = 0;
    scan_recursive( pDir, 0, MaxDepth, pOut, MaxOut, nOut );
    return nOut;
}

//------------------------------------------------------------------------------

static u64 folder_size_recursive( const char* pDir, s32 Depth, s32 MaxDepth )
{
    if( Depth > MaxDepth ) return 0;

    u64  Total = 0;
    DIR* pDirHandle = ::opendir( pDir );
    if( !pDirHandle ) return 0;

    struct dirent* pEnt;
    while( ( pEnt = ::readdir( pDirHandle ) ) != nullptr )
    {
        if( pEnt->d_name[0] == '.' &&
            ( pEnt->d_name[1] == 0 || ( pEnt->d_name[1] == '.' && pEnt->d_name[2] == 0 ) ) )
            continue;

        char Full[ A51_DFS_MAX_PATH_LEN ];
        snprintf( Full, sizeof( Full ), "%s/%s", pDir, pEnt->d_name );

        struct stat St;
        if( ::stat( Full, &St ) != 0 ) continue;

        if( S_ISDIR( St.st_mode ) )      Total += folder_size_recursive( Full, Depth + 1, MaxDepth );
        else if( S_ISREG( St.st_mode ) ) Total += (u64)St.st_size;
    }

    ::closedir( pDirHandle );
    return Total;
}

//------------------------------------------------------------------------------

u64 dfs_folder_size( const char* pDir, s32 MaxDepth )
{
    if( !pDir ) return 0;
    return folder_size_recursive( pDir, 0, MaxDepth );
}

} // namespace a51
//==============================================================================
