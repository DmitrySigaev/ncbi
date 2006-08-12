/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Vladimir Ivanov
 *           Anton Lavrentiev
 *
 * File Description:
 *   Tar archive API.
 *
 *   Supports subset of POSIX.1-1988 (ustar) format.
 *   Old GNU (POSIX 1003.1) and V7 formats are also partially supported.
 *   New archives are created using POSIX (genuine ustar) format, using
 *   GNU extensions for long names/links only when unavoidable.
 *   Can handle no exotics like sparse files, devices, etc,
 *   but just regular files, directories, and symbolic links.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/compress/tar.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CTar can be defined on MS-Windows and UNIX platforms only!"
#endif

#include <memory>

#ifdef NCBI_OS_MSWIN
#  include <io.h>
typedef unsigned int mode_t;
typedef short        uid_t;
typedef short        gid_t;
#endif


BEGIN_NCBI_SCOPE


// Convert a number to an octal string padded to the left
// with [leading] zeros ('0') and having _no_ trailing '\0'.
static bool s_NumToOctal(unsigned long value, char* ptr, size_t len)
{
    _ASSERT(len > 0);
    do {
        ptr[--len] = '0' + char(value & 7);
        value >>= 3;
    } while (len);
    return !value ? true : false;
}


// Convert an octal number (possibly preceded by spaces) to numeric form.
// Stop either at the end of the field or at first '\0' (if any).
static bool s_OctalToNum(unsigned long& value, const char* ptr, size_t size)
{
    _ASSERT(ptr  &&  size > 0);
    size_t i = *ptr ? 0 : 1;
    while (i < size  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            break;
        i++;
    }
    value = 0;
    bool retval = false;
    while (i < size  &&  ptr[i] >= '0'  &&  ptr[i] <= '7') {
        retval = true;
        value <<= 3;
        value  |= ptr[i++] - '0';
    }
    while (i < size  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            return false;
        i++;
    }
    return retval;
}


static mode_t s_TarToMode(TTarMode value)
{
    mode_t mode = (
#ifdef S_ISUID
                   (value & fTarSetUID   ? S_ISUID  : 0) |
#endif
#ifdef S_ISGID
                   (value & fTarSetGID   ? S_ISGID  : 0) |
#endif
#ifdef S_ISVTX
                   (value & fTarSticky   ? S_ISVTX  : 0) |
#endif
#if   defined(S_IRUSR)
                   (value & fTarURead    ? S_IRUSR  : 0) |
#elif defined(S_IREAD)
                   (value & fTarURead    ? S_IREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & fTarUWrite   ? S_IWUSR  : 0) |
#elif defined(S_IWRITE)
                   (value & fTarUWrite   ? S_IWRITE : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & fTarUExecute ? S_IXUSR  : 0) |
#elif defined(S_IEXEC)
                   (value & fTarUExecute ? S_IEXEC  : 0) |
#endif
#ifdef S_IRGRP
                   (value & fTarGRead    ? S_IRGRP  : 0) |
#endif
#ifdef S_IWGRP
                   (value & fTarGWrite   ? S_IWGRP  : 0) |
#endif
#ifdef S_IXGRP
                   (value & fTarGExecute ? S_IXGRP  : 0) |
#endif
#ifdef S_IROTH
                   (value & fTarORead    ? S_IROTH  : 0) |
#endif
#ifdef S_IWOTH
                   (value & fTarOWrite   ? S_IWOTH  : 0) |
#endif
#ifdef S_IXOTH
                   (value & fTarOExecute ? S_IXOTH  : 0) |
#endif
                   0);
    return mode;
}


static TTarMode s_ModeToTar(mode_t mode)
{
    /* Foresee that the mode can be extracted on a different platform */
    TTarMode value = (
#ifdef S_ISUID
                      (mode & S_ISUID  ? fTarSetUID   : 0) |
#endif
#ifdef S_ISGID
                      (mode & S_ISGID  ? fTarSetGID   : 0) |
#endif
#ifdef S_ISVTX
                      (mode & S_ISVTX  ? fTarSticky   : 0) |
#endif
#if   defined(S_IRUSR)
                      (mode & S_IRUSR  ? fTarURead    : 0) |
#elif defined(S_IREAD)
                      (mode & S_IREAD  ? fTarURead    : 0) |
#endif
#if   defined(S_IWUSR)
                      (mode & S_IWUSR  ? fTarUWrite   : 0) |
#elif defined(S_IWRITE)
                      (mode & S_IWRITE ? fTarUWrite   : 0) |
#endif
#if   defined(S_IXUSR)
                      (mode & S_IXUSR  ? fTarUExecute : 0) |
#elif defined(S_IEXEC)
                      (mode & S_IEXEC  ? fTarUExecute : 0) |
#endif
#if   defined(S_IRGRP)
                      (mode & S_IRGRP  ? fTarGRead    : 0) |
#elif defined(S_IREAD)
                      /* emulate read permission when file is readable */
                      (mode & S_IREAD  ? fTarGRead    : 0) |
#endif
#ifdef S_IWGRP
                      (mode & S_IWGRP  ? fTarGWrite   : 0) |
#endif
#ifdef S_IXGRP
                      (mode & S_IXGRP  ? fTarGExecute : 0) |
#endif
#if   defined(S_IROTH)
                      (mode & S_IROTH  ? fTarORead    : 0) |
#elif defined(S_IREAD)
                      /* emulate read permission when file is readable */
                      (mode & S_IREAD  ? fTarORead    : 0) |
#endif
#ifdef S_IWOTH
                      (mode & S_IWOTH  ? fTarOWrite   : 0) |
#endif
#ifdef S_IXOTH
                      (mode & S_IXOTH  ? fTarOExecute : 0) |
#endif
                      0);
    return value;
}


static size_t s_Length(const char* ptr, size_t size)
{
    const char* pos = (const char*) memchr(ptr, '\0', size);
    return pos ? (size_t)(pos - ptr) : size;
}


//////////////////////////////////////////////////////////////////////////////
//
// Constants / macros / typedefs
//

/// Tar block size
static const size_t kBlockSize = 512;

/// Round up to the nearest multiple of kBlockSize
#define ALIGN_SIZE(size) ((((size) + kBlockSize-1) / kBlockSize) * kBlockSize)


/// POSIX "ustar" tar archive member header
struct SHeader {        // byte offset
    char name[100];     //   0
    char mode[8];       // 100
    char uid[8];        // 108
    char gid[8];        // 116
    char size[12];      // 124
    char mtime[12];     // 136
    char checksum[8];   // 148
    char typeflag;      // 156
    char linkname[100]; // 157
    char magic[6];      // 257
    char version[2];    // 263
    char uname[32];     // 265
    char gname[32];     // 297
    char devmajor[8];   // 329
    char devminor[8];   // 337
    char prefix[155];   // 345 (not valid with old GNU format)
};                      // 500


/// Block as a header.
union TBlock {
    char    buffer[kBlockSize];
    SHeader header;
};


static void s_TarChecksum(TBlock* block, bool isgnu = false)
{
    SHeader* h = &block->header;
    size_t len = sizeof(h->checksum) - (isgnu ? 2 : 1);

    // Compute the checksum
    memset(h->checksum, ' ', sizeof(h->checksum));
    unsigned long checksum = 0;
    const unsigned char* p = (const unsigned char*) block->buffer;
    for (size_t i = 0; i < sizeof(block->buffer); i++) {
        checksum += *p++;
    }
    // ustar:       '\0'-terminated checksum
    // GNU special: 6 digits, then '\0', then a space [already in place]
    if (!s_NumToOctal(checksum, h->checksum, len)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store checksum");
    }
    h->checksum[len] = '\0';
}



//////////////////////////////////////////////////////////////////////////////
//
// CTarEntryInfo
//

unsigned int CTarEntryInfo::GetMode(void) const
{
    // Raw tar mode gets returned here (as kept in the info)
    return (TTarMode)(m_Stat.st_mode & 07777);
}


void CTarEntryInfo::GetMode(CDirEntry::TMode*            usr_mode,
                            CDirEntry::TMode*            grp_mode,
                            CDirEntry::TMode*            oth_mode,
                            CDirEntry::TSpecialModeBits* special_bits) const
{
    TTarMode mode = GetMode();

    // User
    if (usr_mode) {
        *usr_mode = ((mode & fTarURead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarUWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarUExecute ? CDirEntry::fExecute : 0));
    }

    // Group
    if (grp_mode) {
        *grp_mode = ((mode & fTarGRead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarGWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarGExecute ? CDirEntry::fExecute : 0));
    }

    // Others
    if (oth_mode) {
        *oth_mode = ((mode & fTarORead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarOWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarOExecute ? CDirEntry::fExecute : 0));
    }

    // Special bits
    if (special_bits) {
        *special_bits = ((mode & fTarSetUID ? CDirEntry::fSetUID  : 0) |
                         (mode & fTarSetGID ? CDirEntry::fSetGID  : 0) |
                         (mode & fTarSticky ? CDirEntry::fSticky  : 0));
    }

    return;
}


static string s_ModeAsString(const CTarEntryInfo& info)
{
    // Take raw tar mode
    TTarMode mode = info.GetMode();

    string usr("---");
    string grp("---");
    string oth("---");
    if (mode & fTarURead) {
        usr[0] = 'r';
    }
    if (mode & fTarUWrite) {
        usr[1] = 'w';
    }
    if (mode & fTarUExecute) {
        usr[2] = mode & fTarSetUID ? 's' : 'x';
    } else if (mode & fTarSetUID) {
        usr[2] = 'S';
    }
    if (mode & fTarGRead) {
        grp[0] = 'r';
    }
    if (mode & fTarGWrite) {
        grp[1] = 'w';
    }
    if (mode & fTarGExecute) {
        grp[2] = mode & fTarSetGID ? 's' : 'x';
    } else if (mode & fTarSetGID) {
        grp[2] = 'S';
    }
    if (mode & fTarORead) {
        oth[0] = 'r';
    }
    if (mode & fTarOWrite) {
        oth[1] = 'w';
    }
    if (mode & fTarOExecute) {
        oth[2] = mode & fTarSticky ? 't' : 'x';
    } else if (mode & fTarSticky) {
        oth[2] = 'T';
    }

    char type;
    switch (info.GetType()) {
    case CTarEntryInfo::eFile:
        type = '-';
        break;
    case CTarEntryInfo::eLink:
        type = 'l';
        break;
    case CTarEntryInfo::eDir:
        type = 'd';
        break;
    default:
        type = '?';
        break;
    }

    return type + usr + grp + oth;
}


static string s_UserGroupAsString(const CTarEntryInfo& info)
{
    string user(info.GetUserName());
    if (user.empty()) {
        NStr::UIntToString(user, info.GetUserId());
    }
    string group(info.GetGroupName());
    if (group.empty()) {
        NStr::UIntToString(group, info.GetGroupId());
    }
    return user + "/" + group;
}


ostream& operator << (ostream& os, const CTarEntryInfo& info)
{
    CTime mtime(info.GetModificationTime());
    os << s_ModeAsString(info)                            << ' '
       << setw(17) << s_UserGroupAsString(info)           << ' '
       << setw(10) << NStr::UInt8ToString(info.GetSize()) << ' '
       << mtime.ToLocalTime().AsString("Y-M-D h:m:s")     << ' '
       << info.GetName();
    if (info.GetType() == CTarEntryInfo::eLink) {
        os << " -> " << info.GetLinkName();
    }
    return os;
}


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& file_name, size_t blocking_factor)
    : m_FileName(file_name),
      m_FileStream(new CNcbiFstream),
      m_OpenMode(eNone),
      m_Stream(0),
      m_BufferSize(blocking_factor * kBlockSize),
      m_BufferPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase),
      m_IsModified(false)
{
    x_Init();
}


CTar::CTar(CNcbiIos& stream, size_t blocking_factor)
    : m_FileName(kEmptyStr),
      m_FileStream(0),
      m_OpenMode(eNone),
      m_Stream(&stream),
      m_BufferSize(blocking_factor * kBlockSize),
      m_BufferPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase),
      m_IsModified(false)
{
    x_Init();
}


CTar::~CTar()
{
    // Close stream(s)
    Close();
    delete m_FileStream;

    // Delete owned file name masks
    UnsetMask();

    // Delete buffer
    delete[] m_BufPtr;
}


void CTar::x_Init(void)
{
    size_t pagesize = (size_t) GetVirtualMemoryPageSize();
    if (!pagesize) {
        pagesize = 4096; // reasonable default
    }
    size_t pagemask = pagesize - 1;
    // Assume that the page size is a power of 2
    _ASSERT((pagesize & pagemask) == 0);
    m_BufPtr = new char[m_BufferSize + pagemask];
    // Make m_Buffer page-aligned
    m_Buffer = m_BufPtr +
        ((((size_t) m_BufPtr + pagemask) & ~pagemask) - (size_t) m_BufPtr);
}


void CTar::x_Flush(void)
{
    if (!m_Stream  ||  !m_OpenMode  ||  !m_IsModified) {
        return;
    }
    _ASSERT(m_OpenMode == eRW);
    _ASSERT(m_BufferSize >= m_BufferPos);
    size_t pad = m_BufferSize - m_BufferPos;
    // Assure proper blocking factor and pad the archive as necessary
    memset(m_Buffer + m_BufferPos, 0, pad);
    x_WriteArchive(pad);
    _ASSERT(m_BufferPos == 0);
    if (pad < kBlockSize  ||  pad - (pad % kBlockSize) < (kBlockSize << 1)) {
        // Write EOT (two zero blocks), if have not already done so by padding
        memset(m_Buffer, 0, m_BufferSize);
        x_WriteArchive(m_BufferSize);
        _ASSERT(m_BufferPos == 0);
        if (m_BufferSize == kBlockSize) {
            x_WriteArchive(kBlockSize);
            _ASSERT(m_BufferPos == 0);
        }
    }
    if (m_Stream->rdbuf()->PUBSYNC() != 0) {
        NCBI_THROW(CTarException, eWrite, "Archive flush error");
    }
    m_IsModified = false;
}


void CTar::x_Close(void)
{
    if (!m_OpenMode)
        return;
    if (m_FileStream  &&  m_FileStream->is_open()) {
        m_FileStream->close();
        m_Stream = 0;
    }
    m_OpenMode  = eNone;
    m_BufferPos = 0;
}


auto_ptr<CTar::TEntries> CTar::x_Open(EAction action)
{
    _ASSERT(action);
    // We can only open a named file here, and if an external stream
    // is being used as an archive, it must be explicitly repositioned by
    // user's code (outside of this class) before each archive operation.
    if (!m_FileStream) {
        if (m_IsModified  &&  action != eAppend) {
            ERR_POST(Warning << string("Pending changes may be discarded"
                                       " upon in-stream archive reopen"));
            m_IsModified = false;
            m_BufferPos = 0;
        }
        if (!m_Stream  ||  !m_Stream->good()  ||  !m_Stream->rdbuf()) {
            m_OpenMode = eNone;
            NCBI_THROW(CTarException, eOpen,
                       "Cannot open archive from bad IO stream");
        } else {
            m_OpenMode = EOpenMode((int) action & eRW);
        }
    } else {
        EOpenMode mode = EOpenMode((int) action & eRW);
        _ASSERT(mode != eNone);
        if (mode != eWO  &&  action != eAppend) {
            x_Flush();
        }
        if (mode == eWO  ||  m_OpenMode < mode) {
            x_Close();
            switch (mode) {
            case eWO:
                /// WO access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::out    |
                                   IOS_BASE::binary | IOS_BASE::trunc);
                break;
            case eRO:
                /// RO access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     |
                                   IOS_BASE::binary);
                break;
            case eRW:
                /// RW access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     |  IOS_BASE::out  |
                                   IOS_BASE::binary);
                break;
            default:
                _TROUBLE;
                break;
            }
            if (!m_FileStream->good()) {
                NCBI_THROW(CTarException, eOpen,
                           "Cannot open archive '" + m_FileName + '\'');
            }
            m_Stream = m_FileStream;
        }

        if (m_OpenMode) {
            _ASSERT(action != eCreate);
            if (action != eAppend) {
                m_BufferPos = 0;
                m_FileStream->seekg(0, IOS_BASE::beg);
            }
        } else {
            if (action == eAppend  &&  !m_IsModified) {
                // There may be an extra and unnecessary archive scanning
                // if Append() follows Update() that caused no modifications;
                // but there is no way to distinguish this, currently :-/
                // Also, this sequence should be a real rarity in practice.
                // Position at logical EOF
                x_ReadAndProcess(action);
            }
            m_OpenMode = mode;
        }
    }
    _ASSERT(m_Stream);
    _ASSERT(m_Stream->rdbuf());

    if (action == eList  ||  action == eUpdate  ||  action == eTest) {
        return x_ReadAndProcess(action, action != eUpdate);
    } else {
        return auto_ptr<TEntries>(0);
    }
}


auto_ptr<CTar::TEntries> CTar::Extract(void)
{
    x_Open(eExtract);

    // Extract
    auto_ptr<TEntries> entries = x_ReadAndProcess(eExtract);

    // Restore attributes of "delayed" directory entries
    ITERATE(TEntries, i, *entries.get()) {
        if (i->GetType() == CTarEntryInfo::eDir) {
            if (m_Flags & fPreserveAll) {
                x_RestoreAttrs(*i);
            }
        }
    }

    return entries;
}


Uint8 CTar::EstimateArchiveSize(const TFiles& files)
{
    Uint8 result = 0;

    ITERATE(TFiles, it, files) {
        // Compose file name for relative names
        string file;
        if (CDirEntry::IsAbsolutePath(it->first)  ||  m_BaseDir.empty()) {
            file = it->first;
        } else {
            file = CDirEntry::ConcatPath(m_BaseDir, it->first);
        }
        string name = x_ToArchiveName(file);

        result += kBlockSize/*header*/ + ALIGN_SIZE(it->second);
        size_t size = name.length();
        if (size > sizeof(((SHeader*) 0)->name)) {
            result += kBlockSize/*header*/ + ALIGN_SIZE(size);
        }
    }
    if (result) {
        result += kBlockSize << 1; // EOT
        Uint8 incomplete = result % m_BufferSize;
        if (incomplete) {
            result += m_BufferSize - incomplete;
        }
    }

    return result;
}


// Return a pointer to buffer, which is always block-aligned,
// and reflect the number of bytes available via the parameter.
const char* CTar::x_ReadArchive(size_t& n)
{
    _ASSERT(n != 0);
    _ASSERT(m_BufferPos % kBlockSize == 0);
    size_t nread;
    if (!m_BufferPos) {
        nread = (size_t) m_Stream->rdbuf()->sgetn(m_Buffer, m_BufferSize);
        if (!nread) {
            return 0/*EOF*/;
        }
        size_t gap = m_BufferSize - nread;
        memset(m_Buffer + nread, 0, gap);
    } else {
        nread = m_BufferSize - m_BufferPos;
    }
    if (n > nread) {
        n = nread;
    }
    size_t xpos = m_BufferPos;
    m_BufferPos += ALIGN_SIZE(n);
    m_BufferPos %= m_BufferSize;
    return m_Buffer + xpos;
}


// All partial internal (i.e. in-buffer) block writes are _not_ block-aligned.
void CTar::x_WriteArchive(size_t nwrite, const char* src)
{
    if (!nwrite) {
        return;
    }
    do {
        size_t avail = m_BufferSize - m_BufferPos;
        _ASSERT(avail != 0);
        if (avail > nwrite) {
            avail = nwrite;
        }
        size_t advance = avail;
        if (src) {
            memcpy(m_Buffer + m_BufferPos, src, avail);
            size_t pad = ALIGN_SIZE(avail) - avail;
            memset(m_Buffer + m_BufferPos + avail, 0, pad);
            advance += pad;
        }
        m_BufferPos += advance;
        if (m_BufferPos == m_BufferSize) {
            if ((size_t) m_Stream->rdbuf()->sputn(m_Buffer, m_BufferSize)
                != m_BufferSize) {
                NCBI_THROW(CTarException, eWrite, "Archive write error");
            }
            m_BufferPos = 0;
        }
        nwrite -= avail;
    } while (nwrite);
    m_IsModified = true;
}


CTar::EStatus CTar::x_ReadEntryInfo(CTarEntryInfo& info)
{
    // Read block
    const TBlock* block;
    size_t nread = kBlockSize;
    _ASSERT(sizeof(block->buffer) == nread);
    if (!(block = (const TBlock*) x_ReadArchive(nread))) {
        return eEOF;
    }
    if (nread != kBlockSize) {
        NCBI_THROW(CTarException, eRead, "Unexpected EOF in archive");
    }
    const SHeader* h = &block->header;

    bool ustar = false, oldgnu = false;
    // Check header format
    if (memcmp(h->magic, "ustar", 6) == 0) {
        ustar = true;
    } else if (memcmp(h->magic, "ustar  ", 8) == 0) {
        // NB: Here, the magic protruded into the adjacent version field
        oldgnu = true;
    } else if (memcmp(h->magic, "\0\0\0\0\0", 6) != 0) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat,
                   "Unknown archive format");
    }

    // Get checksum from header
    unsigned long value;
    if (!s_OctalToNum(value, h->checksum, sizeof(h->checksum))) {
        // We must allow all zero bytes here in case of pad/zero blocks
        for (size_t i = 0; i < sizeof(block->buffer); i++) {
            if (block->buffer[i]) {
                NCBI_THROW(CTarException,eUnsupportedTarFormat,"Bad checksum");
            }
        }
        return eZeroBlock;
    }
    int checksum = (int)value;

    // Compute both signed and unsigned checksums (for compatibility)
    int ssum = 0;
    unsigned int usum = 0;
    const char* p = block->buffer;
    for (size_t i = 0; i < sizeof(block->buffer); i++)  {
        ssum +=                 *p;
        usum += (unsigned char)(*p);
        p++;
    }
    p = h->checksum;
    for (size_t j = 0; j < sizeof(h->checksum); j++) {
        ssum -=                 *p  - ' ';
        usum -= (unsigned char)(*p) - ' ';
        p++;
    }

    // Compare checksum(s)
    if (checksum != ssum   &&  (unsigned int) checksum != usum) {
        NCBI_THROW(CTarException, eChecksum, "Header checksum failed");
    }

    // Set info members now

    // Name
    if (ustar/*implies "&&  !oldgnu"*/  &&  h->prefix[0]) {
        info.m_Name =
            CDirEntry::ConcatPath(string(h->prefix,
                                         s_Length(h->prefix,
                                                  sizeof(h->prefix))),
                                  string(h->name,
                                         s_Length(h->name,
                                                  sizeof(h->name))));
    } else {
        info.m_Name.assign(h->name, s_Length(h->name, sizeof(h->name)));
    }

    // Mode
    if (!s_OctalToNum(value, h->mode, sizeof(h->mode))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad entry mode");
    }
    info.m_Stat.st_mode = (mode_t) value;

    // User Id
    if (!s_OctalToNum(value, h->uid, sizeof(h->uid))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad user ID");
    }
    info.m_Stat.st_uid = (uid_t) value;

    // Group Id
    if (!s_OctalToNum(value, h->gid, sizeof(h->gid))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad group ID");
    }
    info.m_Stat.st_gid = (gid_t) value;

    // Size
    if (!s_OctalToNum(value, h->size, sizeof(h->size))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad entry size");
    }
    info.m_Stat.st_size = value;

    // Modification time
    if (!s_OctalToNum(value, h->mtime, sizeof(h->mtime))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat,
                   "Bad modification time");
    }
    info.m_Stat.st_mtime = value;

    // Entry type
    switch (h->typeflag) {
    case '\0':
    case '0':
        if (ustar  ||  oldgnu) {
            info.m_Type = CTarEntryInfo::eFile;
        } else {
            size_t name_size = s_Length(h->name, sizeof(h->name));
            if (!name_size  ||  h->name[name_size - 1] != '/') {
                info.m_Type = CTarEntryInfo::eFile;
            } else {
                info.m_Type = CTarEntryInfo::eDir;
                info.m_Stat.st_size = 0;
            }
        }
        break;
    case '2':
        info.m_Type = CTarEntryInfo::eLink;
        info.m_LinkName.assign(h->linkname,
                               s_Length(h->linkname, sizeof(h->linkname)));
        info.m_Stat.st_size = 0;
        break;
    case '5':
        if (ustar  ||  oldgnu) {
            info.m_Type = CTarEntryInfo::eDir;
            info.m_Stat.st_size = 0;
            break;
        }
        /*FALLTHRU*/
    case 'K':
    case 'L':
        if (oldgnu) {
            info.m_Type = h->typeflag == 'K'
                ? CTarEntryInfo::eGNULongLink
                : CTarEntryInfo::eGNULongName;
            // Read the long name in
            info.m_Name.erase();
            size_t size = (streamsize) info.GetSize();
            while (size) {
                nread = size;
                const char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    NCBI_THROW(CTarException, eRead,
                               "Unexpected EOF while reading long name");
                }
                info.m_Name.append(xbuf, nread);
                size -= nread;
            }
            info.m_Stat.st_size = 0;
            // NB: Input buffers may be trashed, so return right here
            return eSuccess;
        }
        /*FALLTHRU*/
    case '1':
        if (h->typeflag == '1') {
            // Fixup although we don't handle hard links
            info.m_Stat.st_size = 0;
        }
        /*FALLTHRU*/
    default:
        info.m_Type = CTarEntryInfo::eUnknown;
        break;
    }

    if (ustar  ||  oldgnu) {
        // User name
        info.m_UserName.assign(h->uname, s_Length(h->uname, sizeof(h->uname)));

        // Group name
        info.m_GroupName.assign(h->gname, s_Length(h->gname,sizeof(h->gname)));
    }

    if (oldgnu) {
        // Name prefix cannot be used because there are times, and other stuff
        if (!s_OctalToNum(value, h->prefix,      12)) {
            NCBI_THROW(CTarException, eUnsupportedTarFormat,
                       "Bad last access time");
        }
        info.m_Stat.st_atime = value;
        if (!s_OctalToNum(value, h->prefix + 12, 12)) {
            NCBI_THROW(CTarException, eUnsupportedTarFormat,
                       "Bad creation time");
        }
        info.m_Stat.st_ctime = value;
    }

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& name, const CTarEntryInfo& info)
{
    // Prepare block info
    TBlock block;
    _ASSERT(sizeof(block.buffer) == kBlockSize);
    memset(block.buffer, 0, sizeof(block.buffer));
    SHeader* h = &block.header;

    CTarEntryInfo::EType type = info.GetType();

    // Name(s) ('\0'-terminated if fit, otherwise not)
    if (!x_PackName(h, info, false)) {
        NCBI_THROW(CTarException, eNameTooLong,
                   "Name '" + info.GetName() + "' too long in"
                   " entry '" + name + '\'');
    }
    if (type == CTarEntryInfo::eLink  &&  !x_PackName(h, info, true)) {
        NCBI_THROW(CTarException, eNameTooLong,
                   "Link '" + info.GetLinkName() + "' too long in"
                   " entry '" + name + '\'');
    }

    /* NOTE:  Although some sources on the Internet indicate that
     * all but size, mtime and version numeric fields are '\0'-terminated,
     * we could not confirm that with existing tar programs, all of
     * which used either '\0' or ' '-terminated values in both size
     * and mtime fields.  For ustar archive we have found a document
     * that definitively tells that _all_ numeric fields are '\0'-terminated,
     * and can keep maximum sizeof(field)-1 octal digits.  We follow it here.
     * However, GNU and ustar checksums seem to be different indeed,
     * so we don't use a trailing space for ustar, but for GNU only.
     */

    // Mode
    if (!s_NumToOctal(info.GetMode(), h->mode, sizeof(h->mode) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store file mode");
    }

    // User ID
    if (!s_NumToOctal(info.GetUserId(), h->uid, sizeof(h->uid) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store user ID");
    }

    // Group ID
    if (!s_NumToOctal(info.GetGroupId(), h->gid, sizeof(h->gid) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store group ID");
    }

    // Size
    if (!s_NumToOctal((unsigned long)
                      (type == CTarEntryInfo::eFile ? info.GetSize() : 0),
                      h->size, sizeof(h->size) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store file size");
    }

    // Modification time
    if (!s_NumToOctal((unsigned long) info.GetModificationTime(),
                      h->mtime, sizeof(h->mtime) - 1)){
        NCBI_THROW(CTarException, eMemory, "Cannot store modification time");
    }

    // Type (GNU extension for SymLink)
    switch (type) {
    case CDirEntry::eFile:
        h->typeflag = '0';
        break;
    case CDirEntry::eLink:
        h->typeflag = '2';
        break;
    case CDirEntry::eDir:
        h->typeflag = '5';
        break;
    default:
        NCBI_THROW(CTarException, eUnsupportedEntryType,
                   "Don't know how to store entry type #" +
                   NStr::IntToString(int(type)) + " in archive");
        break;
    }

    // User and group names
    const string& usr = info.GetUserName();
    size_t len = usr.length();
    if (len < sizeof(h->uname)) {
        memcpy(h->uname, usr.c_str(), len);
    }
    const string& grp = info.GetGroupName();
    len = grp.length();
    if (len < sizeof(h->gname)) {
        memcpy(h->gname, grp.c_str(), len);
    }

    // Magic
    strcpy(h->magic,   "ustar");
    // Version (EXCEPTION:  not '\0' terminated)
    memcpy(h->version, "00", 2);

    s_TarChecksum(&block);

    // Write header
    x_WriteArchive(sizeof(block.buffer), block.buffer);
}


bool CTar::x_PackName(SHeader* h, const CTarEntryInfo& info, bool link)
{
    char*      storage = link ? h->linkname         : h->name;
    size_t        size = link ? sizeof(h->linkname) : sizeof(h->name);
    const string& name = link ? info.GetLinkName()  : info.GetName();
    size_t         len = name.length();
    const char*    src = name.c_str();

    if (len <= size) {
        // Name fits!
        memcpy(storage, src, len);
        return true;
    }

    if (!link  &&  len <= sizeof(h->prefix) + 1 + sizeof(h->name)) {
        // Try to split the long name into a prefix and a short name (POSIX)
        size_t i = len;
        if (i > sizeof(h->prefix)) {
            i = sizeof(h->prefix);
        }
        while (i > 0) {
            if (CDirEntry::IsPathSeparator(src[--i])) {
                break;
            }
        }
        if (i  &&  len - i <= sizeof(h->name) + 1) {
            memcpy(h->prefix, src,         i);
            memcpy(h->name,   src + i + 1, len - i - 1);
            return true;
        }
    }

    // Still, store the initial part in the original header
    memcpy(storage, src, size);

    // Prepare extended block header with the long name info (old GNU style)
    _ASSERT(m_BufferPos % kBlockSize == 0  &&  m_BufferPos < m_BufferSize);
    TBlock* block = (TBlock*)(m_Buffer + m_BufferPos);
    _ASSERT(sizeof(block->buffer) == kBlockSize);
    memset(block->buffer, 0, sizeof(block->buffer));
    h = &block->header;

    // See above for comments about header filling
    strcpy(h->name, "././@LongLink");
    s_NumToOctal(0,        h->mode,  sizeof(h->mode) - 1);
    s_NumToOctal(0,        h->uid,   sizeof(h->uid)  - 1);
    s_NumToOctal(0,        h->gid,   sizeof(h->gid)  - 1);
    if (!s_NumToOctal(len, h->size,  sizeof(h->size) - 1)) {
        return false;
    }
    s_NumToOctal(0,        h->mtime, sizeof(h->mtime)- 1);
    h->typeflag = link ? 'K' : 'L';

    // NB: Old GNU magic protrudes into adjacent version field
    memcpy(h->magic, "ustar  ", 8); // 2 spaces and '\0'-terminated

    s_TarChecksum(block, true);

    // Write the header
    x_WriteArchive(sizeof(block->buffer));

    // Store the full name in the extended block
    AutoPtr< char, ArrayDeleter<char> > buf_ptr(new char[len]);
    storage = buf_ptr.get();
    memcpy(storage, src, len);

    // Write the extended block (will be aligned as necessary)
    x_WriteArchive(len, storage);

    return true;
}


void CTar::x_Backspace(EAction action, size_t blocks)
{
    if (!blocks  ||  (action != eAppend  &&  action != eUpdate)) {
        return;
    }
    if (!m_FileStream) {
        ERR_POST(Warning << "In-stream update results in gapped tar archive");
        return;
    }

    CT_POS_TYPE pos = m_FileStream->tellg();  // Current read position
    if (pos == (CT_POS_TYPE)(-1)) {
        NCBI_THROW(CTarException, eRead, "Archive backspace error");
    }
    size_t      gap = blocks * kBlockSize;    // Size of zero-filled area read
    CT_POS_TYPE rec = 0;                      // Record number (0-based)

    if (pos > (CT_POS_TYPE)(gap)) {
        // NB: pos > 0 here
        pos -= 1;
        rec = pos / m_BufferSize;
        if (!m_BufferPos)
            m_BufferPos = m_BufferSize;
        if (gap > m_BufferPos) {
            gap -= m_BufferPos;
            size_t n = gap / m_BufferSize;
            rec -= (CT_OFF_TYPE)(n);          // Backup this many records
            gap %= m_BufferSize;              // Gap size remaining
            m_BufferPos = 0;
            if (gap) {
                rec -= 1;                     // Compensation for partial rec.
                m_FileStream->seekg(rec * m_BufferSize);
                n = kBlockSize;
                x_ReadArchive(n);             // Refetch the record
                m_BufferPos = m_BufferSize - gap;
            }
        } else {
            m_BufferPos -= gap;               // Entirely within this buffer
        }
    } else {
        m_BufferPos = 0;                      // File had nothing but the gap
    }

    m_FileStream->seekp(rec * m_BufferSize);  // Always set put position here
}


auto_ptr<CTar::TEntries> CTar::x_ReadAndProcess(EAction action, bool use_mask)
{
    auto_ptr<TEntries> entries(new TEntries);

    string nextLongName, nextLongLink;
    size_t zeroblock_count = 0;

    for (;;) {
        // Next block is supposed to be a header
        CTarEntryInfo info;
        EStatus status = x_ReadEntryInfo(info);
        switch (status) {
        case eSuccess:
            // processed below
            break;
        case eZeroBlock:
            zeroblock_count++;
            if ((m_Flags & fIgnoreZeroBlocks)  ||  zeroblock_count < 2) {
                continue;
            }
            // Two zero blocks -> eEOF
            /*FALLTHRU*/
        case eEOF:
            x_Backspace(action, zeroblock_count);
            return entries;
        default:
            NCBI_THROW(CCoreException, eCore, "Unknown error");
            break;
        }
        zeroblock_count = 0;

        //
        // Process entry
        //
        switch (info.GetType()) {
        case CTarEntryInfo::eGNULongName:
            // Latch next long name here then just skip
            nextLongName = info.GetName();
            continue;
        case CTarEntryInfo::eGNULongLink:
            // Latch next long link here then just skip
            nextLongLink = info.GetName();
            continue;
        default:
            // Otherwise process the entry as usual
            break;
        }

        // Correct 'info' if long names have been previously defined
        if (!nextLongName.empty()) {
            info.m_Name.swap(nextLongName);
            nextLongName.erase();
        }
        if (!nextLongLink.empty()) {
            if (info.GetType() == CTarEntryInfo::eLink) {
                info.m_LinkName.swap(nextLongLink);
            }
            nextLongLink.erase();
        }

        // Match file name by set of masks
        bool match = true;
        if (action != eTest  &&  action != eAppend) {
            if (use_mask  &&  m_Mask) {
                match = m_Mask->Match(info.GetName(), m_MaskUseCase);
            }
            if (match) {
                entries->push_back(info);
            }
        }

        if (action == eExtract) {
            // Extract entry (if)
            x_ProcessEntry(info, match);
        } else {
            // Skip entry
            x_ProcessEntry(info);
        }
    }
    /*NOTREACHED*/
}


void CTar::x_ProcessEntry(const CTarEntryInfo& info, bool extract)
{
    // Remaining entry size in the archive
    streamsize size;
    
    if (extract) {
        size = x_ExtractEntry(info);
    } else {
        size = (streamsize) info.GetSize();
    }

    while (size) {
        size_t nskip = size;
        if (!x_ReadArchive(nskip)) {
            NCBI_THROW(CTarException, eRead, "Archive read error");
        }
        size -= nskip;
    }
}


streamsize CTar::x_ExtractEntry(const CTarEntryInfo& info)
{
    bool extract = true;

    CTarEntryInfo::EType type = info.GetType();
    streamsize           size = (streamsize) info.GetSize();

    // Destination for extraction
    auto_ptr<CDirEntry>
        dst(CDirEntry::CreateObject(CDirEntry::EType(type),
                                    CDir::ConcatPath(m_BaseDir,
                                                     info.GetName())));
    // Dereference sym.link if requested
    if (type != CTarEntryInfo::eLink  &&  (m_Flags & fFollowLinks)) {
        dst->DereferenceLink();
    }

    // Look if extract is necessary
    if (dst->Exists()) {
        // Can overwrite it?
        if (!(m_Flags & fOverwrite)) {
            // Entry already exists, and cannot be changed
            extract = false;
        } else { // The fOverwrite flag is set
            // Can update?
            if (((m_Flags & fUpdate) == fUpdate)  &&
                (type != CTarEntryInfo::eDir)) {
                // Update directories always, because archive can contain
                // other subtree of existing destination directory.
                time_t dst_time;
                // Check that destination is not older than the archive entry
                if (dst->GetTimeT(&dst_time)  &&
                    dst_time >= info.GetModificationTime()) {
                    extract = false;
                }
            }
            // Have equal types?
            if (extract  &&  (m_Flags & fEqualTypes)  &&
                CDirEntry::EType(type) != dst->GetType()) {
                extract = false;
            }
            // Need to backup destination entry?
            if (extract  &&  ((m_Flags & fBackup) == fBackup)) {
                try {
                    CDirEntry dst_tmp(*dst);
                    if (!dst_tmp.Backup(kEmptyStr,
                                        CDirEntry::eBackup_Rename)) {
                        NCBI_THROW(CTarException, eBackup,
                                "Cannot backup existing destination '" +
                                dst->GetPath() + '\'');
                    } else {
                        // The fOverwrite flag is set
                        dst->Remove();
                    }
                } catch (...) {
                    extract = false;
                }
            }
        } /* check on fOverwrite */
    }

    if (!extract) {
        // Full size of the entry to skip
        return size;
    }

    // Really extract the entry
    switch (info.GetType()) {
    case CTarEntryInfo::eFile:
        {{
            // Create base directory
            CDir dir(dst->GetDir());
            if (!dir.CreatePath()) {
                NCBI_THROW(CTarException, eCreate,
                           "Cannot create directory '" + dir.GetPath() + '\'');
            }
            // Create file
            ofstream file(dst->GetPath().c_str(),
                          IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
            if (!file) {
                NCBI_THROW(CTarException, eCreate,
                           "Cannot create file '" + dst->GetPath() + '\'');
            }

            while (size) {
                // Read from the archive
                size_t nread = size;
                const char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    NCBI_THROW(CTarException, eRead, "Cannot extract '"
                               + info.GetName() + "' from archive");
                }
                // Write file to disk
                if (!file.write(xbuf, nread)) {
                    NCBI_THROW(CTarException, eWrite,
                               "Error writing file '" + dst->GetPath() + '\'');
                }
                size -= nread;
            }

            _ASSERT(file.good());
            file.close();

            // Restore attributes
            if (m_Flags & fPreserveAll) {
                x_RestoreAttrs(info, dst.get());
            }
        }}
        break;

    case CTarEntryInfo::eDir:
        if (!CDir(dst->GetPath()).CreatePath()) {
            NCBI_THROW(CTarException, eCreate,
                       "Cannot create directory '" + dst->GetPath() + '\'');
        }
        // Attributes for directories must be set only when all
        // its files have been already extracted.
        _ASSERT(size == 0);
        break;

    case CTarEntryInfo::eLink:
        {{
            CSymLink symlink(dst->GetPath());
            if (!symlink.Create(info.GetLinkName())) {
                ERR_POST(Error << "Cannot create symlink '" + dst->GetPath()
                         + "' -> '" + info.GetLinkName() + '\'');
            }
            _ASSERT(size == 0);
        }}
        break;

    case CTarEntryInfo::eGNULongName:
    case CTarEntryInfo::eGNULongLink:
        // Long name/link -- already processed and should not be here
        _ASSERT(0);
        break;

    default:
        ERR_POST(Warning << "Skipping unsupported entry '" +
                 info.GetName() + "' of type " + NStr::IntToString(int(type)));
        break;
    }

    return size;
}


void CTar::x_RestoreAttrs(const CTarEntryInfo& info, CDirEntry* dst)
{
    auto_ptr<CDirEntry> dst_ptr;
    if (!dst) {
        dst = CDirEntry::CreateObject(CDirEntry::EType(info.GetType()),
                                      CDir::ConcatPath(m_BaseDir,
                                                       info.GetName()));
        dst_ptr.reset(dst);
    }

    // Date/time.
    // Set the time before permissions because on some platforms
    // this setting can also affect file permissions.
    if (m_Flags & fPreserveTime) {
        time_t modification = info.GetModificationTime();
        time_t last_access = info.GetLastAccessTime();
        time_t creation = info.GetCreationTime();
        if ( !dst->SetTimeT(&modification,
                            last_access ? &last_access : 0,
                            creation    ? &creation    : 0) ) {
            NCBI_THROW(CTarException, eRestoreAttrs,
                       "Cannot restore date/time for '" +
                       info.GetName() + '\'');
        }
    }

    // Owner.
    // This must precede changing permissions because on some
    // systems chown() clears the set[ug]id bits for non-superusers
    // thus resulting in incorrect permissions.
    if (m_Flags & fPreserveOwner) {
        // 2-tier trial:  first using the names, then using numeric IDs.
        // Note that it is often impossible to restore the original owner
        // without super-user rights so no error checking is done here.
        if (!dst->SetOwner(info.GetUserName(),
                           info.GetGroupName(),
                           eIgnoreLinks)) {
            dst->SetOwner(NStr::IntToString(info.GetUserId()),
                          NStr::IntToString(info.GetGroupId()),
                          eIgnoreLinks);
        }
    }

    // Mode.
    // Set them last.
    if (m_Flags & fPreserveMode) {
        bool failed = false;
#ifdef NCBI_OS_UNIX
        // We cannot change permissions for sym.links because lchmod()
        // is not portable and is not implemented on majority of platforms.
        if (info.GetType() != CTarEntryInfo::eLink) {
            // Use raw mode here to restore most of bits
            mode_t mode = s_TarToMode(info.m_Stat.st_mode);
            if (chmod(dst->GetPath().c_str(), mode) != 0) {
                // May fail due to setuid/setgid bits -- strip'em and try again
                mode &= ~(S_ISUID | S_ISGID);
                failed = chmod(dst->GetPath().c_str(), mode) != 0;
            }
        }
#else
        CDirEntry::TMode user, group, other;
        CDirEntry::TSpecialModeBits special_bits;
        info.GetMode(&user, &group, &other, &special_bits);
        failed = !dst->SetMode(user, group, other, special_bits);
#endif /*NCBI_OS_UNIX*/
        if (failed) {
            NCBI_THROW(CTarException, eRestoreAttrs,
                       "Cannot restore file mode for '" +
                       info.GetName() + '\'');
        }
    }
}


string CTar::x_ToArchiveName(const string& path) const
{
    string retval = CDirEntry::NormalizePath(path, eIgnoreLinks);

    // Remove leading base dir from the path
    if (!m_BaseDir.empty()  &&  NStr::StartsWith(retval, m_BaseDir)) {
        retval.erase(0, m_BaseDir.length()/*separator too*/);
    }
    _ASSERT(!retval.empty());

    SIZE_TYPE pos = 0;

#ifdef NCBI_OS_MSWIN
    // Convert to Unix format with forward slashes
    retval = NStr::Replace(retval, "\\", "/");

    // Remove disk name if present
    if (isalpha((unsigned char) retval[0])  &&  retval.find(":") == 1) {
        pos = 2;
    }
#endif // NCBI_OS_MSWIN

    // Remove leading and trailing slashes
    while (pos < retval.length()  &&  retval[pos] == '/') {
        ++pos;
    }
    if (pos) {
        retval.erase(0, pos);
    }
    if (retval.length()) {
        pos = retval.length() - 1;
        while (pos >= 0  &&  retval[pos] == '/') {
            --pos;
        }
        retval.erase(pos + 1);
    }

    // Check on '..'
    if (retval == ".."  ||  NStr::StartsWith(retval, "../")  ||
        NStr::EndsWith(retval, "/..")  ||  retval.find("/../") != NPOS) {
        NCBI_THROW(CTarException,
                   eBadName, "Entry name contains '..' (parent) directory");
    }

    return retval;
}


auto_ptr<CTar::TEntries> CTar::x_Append(const string&   entry_name,
                                        const TEntries* toc)
{
    auto_ptr<TEntries> entries(new TEntries);

    // Compose entry name for relative names
    string name;
    if (CDirEntry::IsAbsolutePath(entry_name)  ||  m_BaseDir.empty()) {
        name = entry_name;
    } else {
        name = CDirEntry::ConcatPath(m_BaseDir, entry_name);
    }
    EFollowLinks follow_links = m_Flags & fFollowLinks
        ? eFollowLinks : eIgnoreLinks;

    // Get dir entry information
    CDir entry(name);
    CDirEntry::SStat st;
    if (!entry.Stat(&st, follow_links)) {
        NCBI_THROW(CTarException, eOpen,
                   "Cannot get status information on '" + entry_name + '\'');
    }
    CDirEntry::EType type = entry.GetType();

    // Create the entry info
    CTarEntryInfo info;
    string x_name(x_ToArchiveName(name));
    info.m_Name = type == CDirEntry::eDir ? x_name + '/' : x_name;
    if (info.GetName().empty()) {
        NCBI_THROW(CTarException, eBadName, "Empty entry name not allowed");
    }
    info.m_Type = CTarEntryInfo::EType(type);
    if (info.GetType() == CTarEntryInfo::eLink) {
        _ASSERT(!follow_links);
        info.m_LinkName = entry.LookupLink();
        if (info.GetLinkName().empty()) {
            NCBI_THROW(CTarException, eBadName, "Empty link name not allowed");
        }
    }
    entry.GetOwner(&info.m_UserName, &info.m_GroupName);
    info.m_Stat = st.orig;
    // Fixup for mode bits
    info.m_Stat.st_mode = (mode_t) s_ModeToTar(st.orig.st_mode);

    // Check if we need to update this entry in the archive
    bool update = true;
    if (toc) {
        bool found = false;

        // Start searching from the end of the list, to find
        // the most recently updated entry (if any) first
        REVERSE_ITERATE(CTar::TEntries, i, *toc) {
            if (info.GetName() == i->GetName()  &&
                (info.GetType() == i->GetType() || (m_Flags & fEqualTypes))) {
                found = true;
                if (!entry.IsNewer(i->GetModificationTime(),
                                   CDirEntry::eIfAbsent_Throw)) {
                    if (type != CDirEntry::eDir) {
                        /* same(or older) file, no update */
                        return entries;
                    }
                    /* same(or older) dir gets recursive treatment later */
                    update = false;
                }
                break;
            }
        }

        if (!found) {
            if (m_Flags & fUpdateExistingOnly) {
                return entries;
            }
            if (type == CDirEntry::eDir) {
                update = false;
            }
        }
    }

    // Append entry
    switch (type) {
    case CDirEntry::eLink:
        info.m_Stat.st_size = 0;
        /*FALLTHRU*/
    case CDirEntry::eFile:
        _ASSERT(update);
        x_AppendFile(name, info);
        entries->push_back(info);
        break;

    case CDirEntry::eDir:
        {{
            if (update) {
                x_WriteEntryInfo(name, info);
                entries->push_back(info);
            }
            // Append/Update all files from that directory
            CDir::TEntries dir = entry.GetEntries("*", CDir::eIgnoreRecursive);
            ITERATE(CDir::TEntries, i, dir) {
                auto_ptr<TEntries> e = x_Append((*i)->GetPath(), toc);
                entries->splice(entries->end(), *e.get());
            }
        }}
        break;

    case CDirEntry::eUnknown:
        NCBI_THROW(CTarException, eBadName, "Cannot find '" + name + '\'');
        break;

    default:
        ERR_POST(Warning << "Skipping unsupported source '" + name + '\'');
        break;
    }

    return entries;
}


// Works for both regular files and symbolic links
void CTar::x_AppendFile(const string& filename, const CTarEntryInfo& info)
{
    CNcbiIfstream ifs;

    if (info.GetType() == CTarEntryInfo::eFile) {
        // Open file
        ifs.open(filename.c_str(), IOS_BASE::binary | IOS_BASE::in);
        if (!ifs) {
            NCBI_THROW(CTarException, eOpen,
                       "Cannot open file '" + filename + '\'');
        }
    }

    // Write file header
    x_WriteEntryInfo(filename, info);
    streamsize size = (streamsize) info.GetSize();

    while (size) {
        // Write file contents
        size_t avail = m_BufferSize - m_BufferPos;
        if (avail > (size_t) size) {
            avail = (size_t) size;
        }
        // Read file
        if (!ifs.read(m_Buffer + m_BufferPos, avail)) {
            NCBI_THROW(CTarException, eRead,
                       "Error reading file '" + filename + '\'');
        }
        avail = (size_t) ifs.gcount();
        // Write buffer to the archive
        x_WriteArchive(avail);
        size -= avail;
    }

    // Write zeros to get the written size a multiple of kBlockSize
    size_t zero = ALIGN_SIZE(m_BufferPos) - m_BufferPos;
    memset(m_Buffer + m_BufferPos, 0, zero);
    x_WriteArchive(zero);
    _ASSERT(m_BufferPos % kBlockSize == 0);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.43  2006/08/12 07:01:28  lavr
 * BUGFIX: x_PackName() to correctly split long names
 * Added:  Restore original atime (and virtually ctime) for old GNU formats
 *
 * Revision 1.42  2006/07/13 15:11:06  lavr
 * Bug fixes in x_Backspace() [proper pos] and x_ToArchiveName() [.. handling]
 *
 * Revision 1.41  2006/04/20 18:51:07  ivanov
 * Get rid of warnings on 64-bit Sun Workshop compiler
 *
 * Revision 1.40  2006/03/03 18:25:06  lavr
 * Simplify/change/speed up x_Append()
 *
 * Revision 1.39  2006/03/03 13:19:41  ivanov
 * Fixed warning about uninitialized variable
 *
 * Revision 1.38  2006/02/16 19:51:55  lavr
 * Use "entry" instead of "file" in exception text
 *
 * Revision 1.37  2005/12/02 05:43:32  lavr
 * Few minor mods (comment fixes mostly)
 *
 * Revision 1.36  2005/07/20 16:18:33  ivanov
 * CTar::x_ExtractEntry() --
 * - do not backup entries if only single fOverwrite flag is set;
 * - allow merging entries from some archives containing the same directory
 *   structure.
 *
 * Revision 1.35  2005/07/18 17:58:37  lavr
 * x_Backspace(): use bullet-proof C-style casts with CT_ macros
 *
 * Revision 1.34  2005/07/18 17:50:14  ucko
 * x_Backspace: cast increments to CT_OFF_TYPE for compatibility with
 * WorkShop 5.5.
 *
 * Revision 1.33  2005/07/12 18:26:45  lavr
 * Fix archive flushing bug
 *
 * Revision 1.32  2005/07/12 11:19:03  ivanov
 * Added additional argument to used CDirEntry::IsNewer()
 *
 * Revision 1.31  2005/06/24 12:54:13  ivanov
 * Heed Workshop compiler warnings
 *
 * Revision 1.30  2005/06/23 18:00:14  lavr
 * Heed MSVC warning in x_Backspace()
 *
 * Revision 1.29  2005/06/23 14:57:39  rsmith
 * pos_type must be intialized with a value.
 * (fpos<> does not have a default ctor).
 *
 * Revision 1.28  2005/06/22 21:10:55  lavr
 * x_RestoreAttrs(): Take advantage of special bits (in file modes)
 *
 * Revision 1.27  2005/06/22 21:07:10  lavr
 * Avoid buffer modification (which may lead to data corruption) while reading
 *
 * Revision 1.26  2005/06/22 20:03:34  lavr
 * Proper append/update implementation; Major actions got return values
 *
 * Revision 1.25  2005/06/13 18:27:56  lavr
 * Use enums for mode; define special bits' manipulations; better ustar-ness
 *
 * Revision 1.24  2005/06/03 21:40:32  lavr
 * Add FIXME reminder for CDirEntry::NormalizePath()
 *
 * Revision 1.23  2005/06/03 17:04:29  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.22  2005/06/01 20:15:27  lavr
 * Do not use special bits yet on Windows in CDirEntry:: SetMode()
 *
 * Revision 1.21  2005/06/01 20:04:31  lavr
 * Use older eBackup_Rename enum (not newer one which is not yet in CVS)
 *
 * Revision 1.20  2005/06/01 19:58:58  lavr
 * Fix previous "fix" of getting page size
 * Move tar permission bits to the header; some cosmetics
 *
 * Revision 1.19  2005/05/31 14:09:40  ivanov
 * CTar::x_Init(): use GetVirtualMemoryPageSize().
 * Ger rid of compilation warnings on MSVC.
 *
 * Revision 1.18  2005/05/30 21:04:59  ucko
 * Use erase() rather than clear() on strings for compatibility with GCC 2.95.
 *
 * Revision 1.17  2005/05/30 15:28:22  lavr
 * Comments reviewed, proper blocking factor fully implemented, other patches
 *
 * Revision 1.16  2005/05/27 21:12:55  lavr
 * Revert to use of std::ios as a main I/O stream (instead of std::iostream)
 *
 * Revision 1.15  2005/05/27 14:14:27  lavr
 * Fix MS-Windows compilation problems and heed warnings
 *
 * Revision 1.14  2005/05/27 13:55:45  lavr
 * Major revamp/redesign/fix/improvement/extension of this API
 *
 * Revision 1.13  2005/05/05 13:41:42  ivanov
 * Added const to parameters in private methods
 *
 * Revision 1.12  2005/05/05 12:40:11  ivanov
 * Fixed GCC compilation warning
 *
 * Revision 1.11  2005/05/05 12:32:45  ivanov
 * + CTar::Update()
 *
 * Revision 1.10  2005/04/27 13:53:02  ivanov
 * Added support for (re)storing permissions/owner/times
 *
 * Revision 1.9  2005/02/03 14:01:21  rsmith
 * must initialize m_StreamPos since streampos may not have a default cnstr.
 *
 * Revision 1.8  2005/01/31 20:53:09  ucko
 * Use string::erase() rather than string::clear(), which GCC 2.95
 * continues not to support.
 *
 * Revision 1.7  2005/01/31 15:31:22  ivanov
 * Lines wrapped at 79th column
 *
 * Revision 1.6  2005/01/31 15:09:55  ivanov
 * Fixed include path to tar.hpp
 *
 * Revision 1.5  2005/01/31 14:28:51  ivanov
 * CTar:: rewritten Extract() methods using universal x_ReadAndProcess().
 * Added some private auxiliary methods to support Create(), Append(), List(),
 * and Test() methods.
 *
 * Revision 1.4  2004/12/14 17:55:58  ivanov
 * Added GNU tar long name support
 *
 * Revision 1.3  2004/12/07 15:29:40  ivanov
 * Fixed incorrect file size for extracted files
 *
 * Revision 1.2  2004/12/02 18:01:13  ivanov
 * Comment out unused seekpos()
 *
 * Revision 1.1  2004/12/02 17:49:16  ivanov
 * Initial draft revision
 *
 * ===========================================================================
 */
