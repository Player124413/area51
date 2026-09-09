//==============================================================================
//
//  dfs_archive.hpp
//
//  Reader for the Area 51 "XDFS" disc file system (.dfs + .000/.001/... data
//  splits).  The on-disk layout is the one produced by Apps/dfsTool and read by
//  xCore/Entropy/IOManager/io_dfs.cpp:
//
//      offset 0   : dfs_header (48 bytes, little endian)
//                     u32 Magic          = 'XDFS' (0x58444653)
//                     s32 Version        = 3
//                     u32 Checksum       (CRC16 of header + metadata, low 16 bits)
//                     s32 SectorSize
//                     u32 SplitSize      (max size of one .000/.001 split)
//                     s32 nFiles
//                     s32 nSubFiles      (number of .000, .001, ... data files)
//                     s32 StringsLength
//                     u32 pSubFileTable  (offset from start of header)
//                     u32 pFiles         (offset from start of header)
//                     u32 pChecksums     (offset from start of header, 0 = none)
//                     u32 pStrings       (offset from start of header)
//      pSubFileTable : nSubFiles * { u32 Offset, u32 ChecksumIndex }
//      pFiles        : nFiles    * { u32 PathOff, u32 Name1Off, u32 Name2Off,
//                                    u32 ExtOff,  u32 DataOffset, u32 Length }
//      pChecksums    : u16[] chunk checksums
//      pStrings      : concatenation of NUL terminated strings
//
//  Full file name = Path + Name1 + Name2 + Ext  (see dfs_BuildFileName).
//
//  The reader never loads file data into memory - it pread()s straight out of
//  the split files, which keeps the memory footprint flat no matter how big the
//  disc image is.  pread() also makes a single open archive safe to read from
//  several worker threads.
//
//==============================================================================

#ifndef A51_DFS_ARCHIVE_HPP
#define A51_DFS_ARCHIVE_HPP

#include "a51_types.hpp"

namespace a51 {

//------------------------------------------------------------------------------
//  On disk structures (packed, little endian - kept here for reference and for
//  the static asserts at the bottom of dfs_archive.cpp).
//------------------------------------------------------------------------------

struct dfs_file_raw
{
    u32 PathNameOffset;
    u32 FileNameOffset1;
    u32 FileNameOffset2;
    u32 ExtNameOffset;
    u32 DataOffset;
    u32 Length;
};

struct dfs_subfile_raw
{
    u32 Offset;
    u32 ChecksumIndex;
};

A51_STATIC_ASSERT( sizeof( dfs_file_raw )    == 24, "dfs_file_raw must be 24 bytes"   );
A51_STATIC_ASSERT( sizeof( dfs_subfile_raw ) ==  8, "dfs_subfile_raw must be 8 bytes" );

#define A51_DFS_HEADER_SIZE   48
#define A51_DFS_MAGIC         0x58444653u     // 'XDFS' as an MSVC multi char constant
#define A51_DFS_MAGIC_BYTES   0x53464458u     // the same value written as a byte string
#define A51_DFS_VERSION       3
#define A51_DFS_MAX_FILES     ( 1 << 21 )     // sanity limits - a corrupt header must
#define A51_DFS_MAX_SUBFILES  ( 1 << 14 )     // never be able to make us allocate GBs
#define A51_DFS_MAX_STRINGS   ( 1 << 26 )
#define A51_DFS_MAX_PATH_LEN  512

//------------------------------------------------------------------------------
//  A single file inside the archive (resolved, ready to use).
//------------------------------------------------------------------------------

struct DfsFileInfo
{
    char    Path[ A51_DFS_MAX_PATH_LEN ];
    u32     DataOffset;     // offset in the concatenated .000/.001/... stream
    u32     Length;
    u32     Hash;           // case insensitive hash, for fast lookup
};

//------------------------------------------------------------------------------
//  One .000/.001/... data split.
//------------------------------------------------------------------------------

struct DfsSubFile
{
    int     Fd;             // open file descriptor, -1 when missing
    u64     GlobalStart;    // offset of this split inside the logical stream
    u64     Length;         // size of the split on disk
    u32     ChecksumIndex;  // index into the chunk checksum table
    char    Path[ A51_DFS_MAX_PATH_LEN ];
};

#define A51_DFS_HASH_SLOTS 4096

//------------------------------------------------------------------------------
//  The archive itself.
//------------------------------------------------------------------------------

class DfsArchive
{
public:
                     DfsArchive();
                    ~DfsArchive();

    // Open <pPath>.dfs plus all of its data splits.  Returns false and fills in
    // GetError() when the archive is unusable.
    bool             Open           ( const char* pPath );
    void             Close          ( void );
    bool             IsOpen         ( void ) const { return m_IsOpen; }

    const char*      GetPath        ( void ) const { return m_Path; }
    const char*      GetError       ( void ) const { return m_Error; }

    s32              GetFileCount   ( void ) const { return m_FileCount; }
    s32              GetSubFileCount( void ) const { return m_SubFileCount; }
    s32              GetVersion     ( void ) const { return m_Version; }
    s32              GetSectorSize  ( void ) const { return m_SectorSize; }
    u32              GetSplitSize   ( void ) const { return m_SplitSize; }
    u64              GetDataSize    ( void ) const { return m_DataSize; }
    s32              GetChecksumCount( void ) const { return m_ChecksumCount; }

    const DfsFileInfo& GetFileInfo  ( s32 iFile ) const;
    bool             GetFileName    ( s32 iFile, char* pOut, s32 OutSize ) const;
    s32              FindFile       ( const char* pPath ) const;   // -1 = not found
    u32              GetFileLength  ( const char* pPath ) const;

    // Read Length bytes of file iFile starting at OffsetInFile.
    // Returns the number of bytes actually read.
    s32              Read           ( s32 iFile, u32 OffsetInFile, void* pDst, s32 Length ) const;
    s32              ReadByName     ( const char* pPath, u32 OffsetInFile, void* pDst, s32 Length ) const;

    // Read a raw range of the logical data stream (used by the chunk verifier).
    s32              ReadRaw        ( u64 GlobalOffset, void* pDst, s32 Length ) const;

    // Verify chunk checksums.  pBad receives the number of chunks that do not
    // match.  Returns false when the archive carries no checksum table.
    bool             VerifyChecksums( s32 MaxChunks, s32* pBad, s32* pChecked ) const;

    u16              GetChunkChecksum( s32 iChunk ) const;

private:
                     DfsArchive( const DfsArchive& );
    DfsArchive&      operator = ( const DfsArchive& );

    void             SetError( const char* pFmt, ... );
    bool             LoadStringTable( int Fd, u32 Offset, u32 Length );
    const char*      Str( u32 Offset ) const;
    u32              HashPath( const char* pPath ) const;
    void             BuildHash  ( void );

private:
    bool             m_IsOpen;
    char             m_Path[ A51_DFS_MAX_PATH_LEN ];
    char             m_Error[ 256 ];

    s32              m_Version;
    s32              m_SectorSize;
    u32              m_SplitSize;
    s32              m_FileCount;
    s32              m_SubFileCount;
    s32              m_StringsLength;
    u32              m_HeaderChecksum;

    char*            m_pStrings;
    DfsFileInfo*     m_pFiles;
    DfsSubFile*      m_pSubFiles;
    u16*             m_pChecksums;
    s32              m_ChecksumCount;
    u64              m_DataSize;

    s32*             m_pHashSlots;      // index into m_pFiles + 1, 0 = empty
};

//------------------------------------------------------------------------------
//  Folder scanning helper used by the launcher to validate the game data that
//  the user dropped into the app.
//------------------------------------------------------------------------------

struct DfsSummary
{
    char    Path[ A51_DFS_MAX_PATH_LEN ];
    char    Error[ 256 ];
    u64     TotalSize;      // .dfs + all splits
    u64     DfsSize;
    s32     FileCount;
    s32     SubFileCount;
    s32     Version;
    bool    Valid;
};

// Walks pDir (recursively, up to MaxDepth) and reports every .dfs archive it
// finds.  pOut receives up to MaxOut summaries, returns the number written.
s32  dfs_scan_folder( const char* pDir, s32 MaxDepth, DfsSummary* pOut, s32 MaxOut );

// Total size in bytes of every file inside pDir.
u64  dfs_folder_size( const char* pDir, s32 MaxDepth );

} // namespace a51

#endif // A51_DFS_ARCHIVE_HPP
//==============================================================================
