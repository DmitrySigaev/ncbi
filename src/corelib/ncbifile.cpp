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
 * Author: Vladimir Ivanov
 *
 * File Description:   Files and directories accessory functions
 *
 */

#include <corelib/ncbifile.hpp>

#if !defined(NCBI_OS_MAC)
#  include <sys/types.h>
#  include <sys/stat.h>
#endif

#include <stdio.h>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <io.h>
#  include <direct.h>

#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <dirent.h>
#  include <pwd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  ifndef MAP_FAILED
#    define MAP_FAILED ((void *) -1)
#  endif

#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  endif

#elif defined(NCBI_OS_MAC)
#  include <fcntl.h>
#  include <corelib/ncbi_os_mac.hpp>
#  include <Script.h>
#  include <Gestalt.h>
#  include <Folders.h>

#endif  /* NCBI_OS_MSWIN, NCBI_OS_UNIX, NCBI_OS_MAC */


BEGIN_NCBI_SCOPE


// Path separators

#undef  DIR_SEPARATOR
#undef  DIR_SEPARATOR_ALT
#undef  DISK_SEPARATOR
#undef  ALL_SEPARATORS
#undef  ALL_OS_SEPARATORS

#define DIR_PARENT  ".."
#define DIR_CURRENT '.'
#define ALL_OS_SEPARATORS   ":/\\"

#if defined(NCBI_OS_MSWIN)
#  define DIR_SEPARATOR     '\\'
#  define DIR_SEPARATOR_ALT '/'
#  define DISK_SEPARATOR    ':'
#  define ALL_SEPARATORS    ":/\\"

#elif defined(NCBI_OS_UNIX)
#  define DIR_SEPARATOR     '/'
#  define ALL_SEPARATORS    "/"

#elif defined(NCBI_OS_MAC)
#  define DIR_SEPARATOR     ':'
#  define ALL_SEPARATORS    ":"
#  undef  DIR_PARENT
#  undef  DIR_CURRENT
#endif



//////////////////////////////////////////////////////////////////////////////
//
// Static functions
//

#if defined(NCBI_OS_MAC)
static const FSSpec sNullFSS = {0, 0, "\p"};

static bool operator== (const FSSpec& one, const FSSpec& other)
{
    return one.vRefNum == other.vRefNum
        && one.parID   == other.parID
        && PString(one.name) == PString(other.name);
}
#endif  /* NCBI_OS_MAC */


// Construct real entry mode from parts. Parameters can not have "fDefault" 
// value.
static CDirEntry::TMode s_ConstructMode(CDirEntry::TMode user_mode, 
                                        CDirEntry::TMode group_mode, 
                                        CDirEntry::TMode other_mode)
{
    CDirEntry::TMode mode = 0;
    mode |= (user_mode  << 6);
    mode |= (group_mode << 3);
    mode |= other_mode;
    return mode;
}


//////////////////////////////////////////////////////////////////////////////
//
// CDirEntry
//


CDirEntry::CDirEntry()
#if defined(NCBI_OS_MAC)
    : m_FSS(new FSSpec(sNullFSS))
#endif
{
}

#if defined(NCBI_OS_MAC)
CDirEntry::CDirEntry(const CDirEntry& other) : m_FSS(new FSSpec(*other.m_FSS))
{
    m_DefaultMode[eUser]  = other.m_DefaultMode[eUser];
    m_DefaultMode[eGroup] = other.m_DefaultMode[eGroup];
    m_DefaultMode[eOther] = other.m_DefaultMode[eOther];
}


CDirEntry&
CDirEntry::operator= (const CDirEntry& other)
{
    *m_FSS = *other.m_FSS;
    m_DefaultMode[eUser]  = other.m_DefaultMode[eUser];
    m_DefaultMode[eGroup] = other.m_DefaultMode[eGroup];
    m_DefaultMode[eOther] = other.m_DefaultMode[eOther];
    return *this;
}


CDirEntry::CDirEntry(const FSSpec& fss) : m_FSS(new FSSpec(fss))
{
    m_DefaultMode[eUser]  = m_DefaultModeGlobal[eFile][eUser];
    m_DefaultMode[eGroup] = m_DefaultModeGlobal[eFile][eGroup];
    m_DefaultMode[eOther] = m_DefaultModeGlobal[eFile][eOther];
}
#endif


CDirEntry::CDirEntry(const string& name)
#if defined(NCBI_OS_MAC)
    : m_FSS(new FSSpec(sNullFSS))
#endif
{
    Reset(name);
    m_DefaultMode[eUser]  = m_DefaultModeGlobal[eFile][eUser];
    m_DefaultMode[eGroup] = m_DefaultModeGlobal[eFile][eGroup];
    m_DefaultMode[eOther] = m_DefaultModeGlobal[eFile][eOther];
}


#if defined(NCBI_OS_MAC)
bool CDirEntry::operator== (const CDirEntry& other) const
{
    return *m_FSS == *other.m_FSS;
}

const FSSpec& CDirEntry::FSS() const
{
    return *m_FSS;
}
#endif



CDirEntry::TMode CDirEntry::m_DefaultModeGlobal[eUnknown][3] =
{
    // eFile
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eDir
    { CDirEntry::fDefaultDirUser, CDirEntry::fDefaultDirGroup, 
          CDirEntry::fDefaultDirOther },
    // ePipe
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eLink
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eSocket
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eDoor
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eBlockSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eCharSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther }
};



#if defined(NCBI_OS_MAC)
void CDirEntry::Reset(const string& path)
{
    OSErr err = MacPathname2FSSpec(path.c_str(), m_FSS);
    if (err != noErr  &&  err != fnfErr) {
        *m_FSS = sNullFSS;
    }
}

string CDirEntry::GetPath(void) const
{
    OSErr err;
    char* path;
    err = MacFSSpec2FullPathname(&FSS(), &path);
    if (err != noErr) {
        return kEmptyStr;
    }
    return string(path);
}
#endif


CDirEntry::~CDirEntry(void)
{
#if defined(NCBI_OS_MAC)
    delete m_FSS;
#endif
}


void CDirEntry::SplitPath(const string& path, string* dir,
                          string* base, string* ext)
{
    // Get file name
    size_t pos = path.find_last_of(ALL_SEPARATORS);
    string filename = (pos == NPOS) ? path : path.substr(pos+1);

    // Get dir
    if ( dir ) {
        *dir = (pos == NPOS) ? kEmptyStr : path.substr(0, pos+1);
    }
    // Split file name to base and extension
    pos = filename.rfind('.');
    if ( base ) {
        *base = (pos == NPOS) ? filename : filename.substr(0, pos);
    }
    if ( ext ) {
        *ext = (pos == NPOS) ? kEmptyStr : filename.substr(pos);
    }
}


string CDirEntry::MakePath(const string& dir, const string& base, 
                           const string& ext)
{
    string path;

    // Adding "dir" and file base
    path = AddTrailingPathSeparator(dir) + base;
    // Adding extension
    if ( ext.length()  &&  ext.at(0) != '.' ) {
        path += '.';
    }
    path += ext;
    // Return result
    return path;
}


char CDirEntry::GetPathSeparator(void) 
{
    return DIR_SEPARATOR;
}


bool CDirEntry::IsPathSeparator(const char c)
{
#if defined(DISK_SEPARATOR)
    if ( c == DISK_SEPARATOR ) {
        return true;
    }
#endif
#if defined(DIR_SEPARATOR_ALT)
    if ( c == DIR_SEPARATOR_ALT ) {
        return true;
    }
#endif
    return c == DIR_SEPARATOR;
}


string CDirEntry::AddTrailingPathSeparator(const string& path)
{
    size_t len = path.length();
#if defined(NCBI_OS_MAC)
    if ( !len ) {
        return string(1, GetPathSeparator());
    }
#endif
    if (len  &&  string(ALL_SEPARATORS).find(path.at(len-1)) == NPOS) {
        return path + GetPathSeparator();
    }
    return path;
}


bool CDirEntry::IsAbsolutePath(const string& path)
{
    if ( path.empty() )
        return false;
    char first = path[0];

#if defined(NCBI_OS_MAC)
    if ( path.find(DIR_SEPARATOR) != NPOS  &&  first != DIR_SEPARATOR )
        return true;
#else
    if ( IsPathSeparator(first) )
        return true;
#endif
#if defined(DISK_SEPARATOR)
    if ( path.find(DISK_SEPARATOR) != NPOS )
        return true;
#endif
    return false;
}


bool CDirEntry::IsAbsolutePathEx(const string& path)
{
    if ( path.empty() )
        return false;
    char first = path[0];

    // MAC or WIN absolute
    if ( path.find(':') != NPOS  &&  first != ':' )
        return true;

    // MAC relative path
    if ( first == ':' )
        return false;

    // UNIX or WIN absolute
    if ( first == '\\' || first == '/' )
        return true;

    // Else - relative
    return false;
}


string CDirEntry::ConvertToOSPath(const string& path)
{
    // Not process empty and absolute path
    if ( path.empty() || IsAbsolutePathEx(path) ) {
        return path;
    }
    // Now we have relative "path"
    string xpath = path;
    // Add trailing separator if path ends with DIR_PARENT or DIR_CURRENT
#if defined(DIR_PARENT)
    if ( NStr::EndsWith(xpath, DIR_PARENT) )  {
        xpath += DIR_SEPARATOR;
    }
#endif
#if defined(DIR_CURRENT)
    if ( NStr::EndsWith(xpath, string(1,DIR_CURRENT)) )  {
        xpath += DIR_SEPARATOR;
    }
#endif
    // Replace each path separator with the current OS separator character
    for (size_t i = 0; i < xpath.length(); i++) {
        char c = xpath[i];
        if ( c == '\\' || c == '/' || c == ':') {
            xpath[i] = DIR_SEPARATOR;
        }
    }
#if defined(NCBI_OS_MAC)
    // Fix current and parent refs in the path for MAC
    string search= "..:";
    size_t pos = 0;
    while ((pos = xpath.find(search)) != NPOS) {
        xpath.replace(pos, search.length(), ":");
        if ( xpath.substr(pos + 1, 2) == ".:" )  {
            xpath.erase(pos + 1, 2);
        } else {
            if ( xpath.substr(pos + 1, search.length()) != search )  {
                xpath.insert(pos,":");
            }
        }
    }
    xpath = NStr::Replace(xpath, ".:",  "");
    // Add leading ":" (criterion of relativity of the dir)
    if ( xpath[0] != ':' ) {
        xpath = ":" + xpath;
    }
#else
    // Fix current and parent refs in the path after conversion from MAC path
    // Replace all "::" to "/../"
#if defined(DIR_PARENT)
    string sep1 = string(1,DIR_SEPARATOR);
    string sep2 = string(2,DIR_SEPARATOR);
    string sep = sep1 + DIR_PARENT + sep1;
    while ( xpath.find(sep2) != NPOS ) {
        xpath = NStr::Replace(xpath, sep2, sep);
    }
#if !defined(NCBI_OS_MAC)
    // replace smth like "../xxx/../yyy/zzz" with "../yyy/zzz"
    {
        sep1 = DIR_SEPARATOR; sep1 += DIR_PARENT; sep1 += DIR_SEPARATOR;
        for (bool erased=true; erased;) {
            erased=false;
            size_t found = xpath.find(sep1);
            if (found != 0 && found != string::npos) {
                size_t start = xpath.rfind(DIR_SEPARATOR,found-1);
                if (start != 0 && start != string::npos && (found-start > 1)) {
                    xpath.erase(start,found+sep1.length()-1-start);
                    erased=true;
                }
            }
        }
    }
#endif // NCBI_OS_MAC
#endif // DIR_PARENT
    // Remove leading ":" in the relative path on non-MAC platforms 
    if ( xpath[0] == DIR_SEPARATOR ) {
        xpath.erase(0,1);
    }
#endif
    return xpath;
}


string CDirEntry::ConcatPath(const string& first, const string& second)
{
    // Prepare first part of path
    string path = AddTrailingPathSeparator(NStr::TruncateSpaces(first));
    // Remove leading separator in "second" part
    string part = NStr::TruncateSpaces(second);
    if ( part.length() > 0  &&  part[0] == DIR_SEPARATOR ) {
        part.erase(0,1);
    }
    // Add second part
    path += part;
    return path;
}


string CDirEntry::ConcatPathEx(const string& first, const string& second)
{
    // Prepare first part of path
    string path = NStr::TruncateSpaces(first);

    // Add trailing path separator to first part (OS independence)

    size_t pos = path.length();
#if defined(NCBI_OS_MAC)
    if ( !pos ) {
        path = string(1, GetPathSeparator());
    }
#endif
    if ( pos  &&  string(ALL_OS_SEPARATORS).find(path.at(pos-1)) == NPOS ) {
        // Find used path separator
        char sep = GetPathSeparator();
        size_t sep_pos = path.find_last_of(ALL_OS_SEPARATORS);
        if ( sep_pos != NPOS ) {
            sep = path.at(sep_pos);
        }
        path += sep;
    }
    // Remove leading separator in "second" part
    string part = NStr::TruncateSpaces(second);
    if ( part.length() > 0  &&
         string(ALL_OS_SEPARATORS).find(part[0]) != NPOS ) {
        part.erase(0,1);
    }
    // Add second part
    path += part;
    return path;
}


//  Match "name" against the filename "mask", returning TRUE if
//   it matches, FALSE if not.  
//
bool CDirEntry::MatchesMask(const char* name, const char* mask) 
{
    char c;

    for (;;) {
        // Analyze symbol in mask
        switch ( c = *mask++ ) {
        
        case '\0':
            return *name == '\0';

        case '?':
            if ( *name == '\0' ) {
                return false;
            }
            ++name;
            break;
		
        case '*':
            c = *mask;
            // Collapse multiple stars
            while ( c == '*' ) {
                c = *++mask;
            }
            if (c == '\0') {
                return true;
            }
            // General case, use recursion
            while ( *name ) {
                if ( MatchesMask(name, mask) ) {
                    return true;
                }
                ++name;
            }
            return false;
		
        default:
            // Compare nonpattern character in mask and name
            if ( c != *name++ ) {
                return false;
            }
            break;
        }
    }
    /*NOTREACHED*/
    return false;
}


bool CDirEntry::GetMode(TMode* user_mode, TMode* group_mode, TMode* other_mode)
    const
{
#if defined(NCBI_OS_MAC)
    FSSpec fss = FSS();
    OSErr err = FSpCheckObjectLock(&fss);
    if (err != noErr  &&  err != fLckdErr) {
        return false;
    }
    bool locked = (err == fLckdErr);
    TMode mode = fRead | (locked ? 0 : fWrite);
    // User
    if (user_mode) {
        *user_mode = mode;
    }
    // Group
    if (group_mode) {
        *group_mode = mode;
    }
    // Other
    if (other_mode) {
        *other_mode = mode;
    }
    return true;
#else
    struct stat st;
    if (stat(GetPath().c_str(), &st) != 0) {
        return false;
    }
    // Other
    if (other_mode) {
        *other_mode = st.st_mode & 0007;
    }
    st.st_mode >>= 3;
    // Group
    if (group_mode) {
        *group_mode = st.st_mode & 0007;
    }
    st.st_mode >>= 3;
    // User
    if (user_mode) {
        *user_mode = st.st_mode & 0007;
    }
    return true;
#endif
}


bool CDirEntry::SetMode(TMode user_mode, TMode group_mode, TMode other_mode)
    const
{
    if (user_mode == fDefault) {
        user_mode = m_DefaultMode[eUser];
    }
#if defined(NCBI_OS_MAC)
    bool wantLocked = (user_mode & fWrite) == 0;
    FSSpec fss = FSS();
    OSErr  err = FSpCheckObjectLock(&fss);
    if (err != noErr  &&  err != fLckdErr) {
        return false;
    }
    bool locked = (err == fLckdErr);
    if (locked == wantLocked) {
        return true;
    }
    err = wantLocked 
        ? ::FSpSetFLock(&fss) 
        : ::FSpRstFLock(&fss);
	
    return err == noErr;
#else
    if (group_mode == fDefault) {
        group_mode = m_DefaultMode[eGroup];
    }
    if (other_mode == fDefault) {
        other_mode = m_DefaultMode[eOther];
    }
    TMode mode = s_ConstructMode(user_mode, group_mode, other_mode);

    return chmod(GetPath().c_str(), mode) == 0;
#endif
}


void CDirEntry::SetDefaultModeGlobal(EType entry_type, TMode user_mode, 
                                     TMode group_mode, TMode other_mode)
{
    if (entry_type >= eUnknown ) {
        return;
    }
    if (entry_type == eDir ) {
        if ( user_mode == fDefault ) {
            user_mode = fDefaultDirUser;
        }
        if ( group_mode == fDefault ) {
            group_mode = fDefaultDirGroup;
        }
        if ( other_mode == fDefault ) {
            other_mode = fDefaultDirOther;
        }
    } else {
        if ( user_mode == fDefault ) {
            user_mode = fDefaultUser;
        }
        if ( group_mode == fDefault ) {
            group_mode = fDefaultGroup;
        }
        if ( other_mode == fDefault ) {
            other_mode = fDefaultOther;
        }
    }
    m_DefaultModeGlobal[entry_type][eUser]  = user_mode;
    m_DefaultModeGlobal[entry_type][eGroup] = group_mode;
    m_DefaultModeGlobal[entry_type][eOther] = other_mode;
}


void CDirEntry::SetDefaultMode(EType entry_type, TMode user_mode, 
                               TMode group_mode, TMode other_mode)
{
    if ( user_mode == fDefault ) {
        user_mode = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode == fDefault ) {
        group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode == fDefault ) {
        other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
    m_DefaultMode[eUser]  = user_mode;
    m_DefaultMode[eGroup] = group_mode;
    m_DefaultMode[eOther] = other_mode;
}


void CDirEntry::GetDefaultModeGlobal(EType  entry_type, TMode* user_mode,
                                     TMode* group_mode, TMode* other_mode)
{
    if ( user_mode ) {
        *user_mode = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
}


void CDirEntry::GetDefaultMode(TMode* user_mode, TMode* group_mode,
                               TMode* other_mode) const
{
    if ( user_mode ) {
        *user_mode = m_DefaultMode[eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultMode[eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultMode[eOther];
    }
}


bool CDirEntry::GetTime(CTime *modification,
                        CTime *creation, CTime *last_access) const
{
    struct stat st;
    if (stat(GetPath().c_str(), &st) != 0) {
        return false;
    }
    if (modification) {
        modification->SetTimeT(st.st_mtime);
    }
    if (creation) {
        creation->SetTimeT(st.st_ctime);
    }
    if (last_access) {
        last_access->SetTimeT(st.st_atime);
    }

    return true;
}

 
CDirEntry::EType CDirEntry::GetType(void) const
{
#if defined(NCBI_OS_MAC)
    OSErr   err;
    long    dirID;
    Boolean isDir;
    err = FSpGetDirectoryID(&FSS(), &dirID, &isDir);
    if ( err )
        return eUnknown;
    return isDir ? eDir : eFile;
#else
    struct stat st;
    if (stat(GetPath().c_str(), &st) != 0) {
        return eUnknown;
    }
    if ( (st.st_mode & S_IFREG)  == S_IFREG ) {
        return eFile;
    }
    if ( (st.st_mode & S_IFDIR)  == S_IFDIR ) {
        return eDir;
    }
    if ( (st.st_mode & S_IFCHR)  == S_IFCHR ) {
        return eCharSpecial;
    }

#  if defined(NCBI_OS_MSWIN)
    if ( (st.st_mode & _S_IFIFO) == _S_IFIFO ) {
        return ePipe;
    }

#  elif defined(NCBI_OS_UNIX)
    if ( (st.st_mode & S_IFIFO)  == S_IFIFO ) {
        return ePipe;
    }
    if ( (st.st_mode & S_IFLNK)  == S_IFLNK ) {
        return eLink;
    }
    if ( (st.st_mode & S_IFSOCK) == S_IFSOCK ) {
        return eSocket;
    }
#    ifdef S_IFDOOR /* only Solaris seems to have this */
    if ( (st.st_mode & S_IFDOOR) == S_IFDOOR ) {
        return eDoor;
    }
#    endif
    if ( (st.st_mode & S_IFBLK)  == S_IFBLK ) {
        return eBlockSpecial;
    }

#  endif

    return eUnknown;
#endif
}


bool CDirEntry::Rename(const string& newname)
{
#if defined(NCBI_OS_MAC)
    const int maxFilenameLength = 31;
    if (newname.length() > maxFilenameLength) return false;
    Str31 newNameStr;
    Pstrcpy(newNameStr, newname.c_str());
    OSErr err = FSpRename(&FSS(), newNameStr);
    if (err != noErr) return false;
#else
    if (rename(GetPath().c_str(), newname.c_str()) != 0) {
        return false;
    }
#endif
    Reset(newname);
    return true;
}


bool CDirEntry::Remove(EDirRemoveMode mode) const
{
#if defined(NCBI_OS_MAC)
    OSErr err = ::FSpDelete(&FSS());
    return err == noErr;
#else
    if ( IsDir() ) {
        if (mode == eOnlyEmpty) {
            return rmdir(GetPath().c_str()) == 0;
        } else {
            CDir dir(GetPath());
            return dir.Remove(eRecursive);
        }
    } else {
        return remove(GetPath().c_str()) == 0;
    }
#endif
}



//////////////////////////////////////////////////////////////////////////////
//
// CFile
//

CFile::CFile(const string& filename) : CParent(filename)
{ 
    // Set default mode
    SetDefaultMode(eFile, fDefault, fDefault, fDefault);
}


CFile::~CFile(void)
{ 
    return;
}


Int8 CFile::GetLength(void) const
{
#if defined(NCBI_OS_MAC)
    long dataSize, rsrcSize;
    OSErr err = FSpGetFileSize(&FSS(), &dataSize, &rsrcSize);
    if (err != noErr) {
        return -1;
    } else {
        return dataSize;
    }
#else
    struct stat buf;
    if ( stat(GetPath().c_str(), &buf) != 0 ) {
        return -1;
    }
    return buf.st_size;
#endif
}


string CFile::GetTmpName(void)
{
    char* filename = tmpnam(0);
    if ( !filename ) {
        return kEmptyStr;
    }
    return filename;
}


string CFile::GetTmpNameEx(const string& dir, const string& prefix)
{
#if defined(NCBI_COMPILER_MW_MSL)
    string filenamstr = AddTrailingPathSeparator(dir) + prefix + "XXXXXXXX";
    // we need our filename in memory that mktemp can write into.
    auto_ptr<char> filename(strdup(filenamstr.c_str()));
    mktemp(filename.get());
    string res(filename.get());
    return res;
#else
    char* filename = tempnam(dir.c_str(), prefix.c_str());
    if ( !filename ) {
        return kEmptyStr;
    }
    string res(filename);
    free(filename);
    return res;   
#endif
}


class CTmpStream : public fstream
{
public:
    CTmpStream(const char *s, IOS_BASE::openmode mode) : fstream(s, mode) 
    {
        m_FileName = s; 
    }
    virtual ~CTmpStream(void) 
    { 
        close();
        CFile(m_FileName).Remove();
    }
protected:
    string m_FileName;
};


fstream* CFile::CreateTmpFile(const string& filename, 
                              ETextBinary text_binary,
                              EAllowRead  allow_read)
{
    ios::openmode mode = ios::out | ios::trunc;
    if ( text_binary == eBinary ) {
        mode = mode | ios::binary;
    }
    if ( allow_read == eAllowRead ) {
        mode = mode | ios::in;
    }
    string tmpname = filename.empty() ? GetTmpName() : filename;
    fstream* stream = new CTmpStream(tmpname.c_str(), mode);
    return stream;
}


fstream* CFile::CreateTmpFileEx(const string& dir, const string& prefix,
                                ETextBinary text_binary, 
                                EAllowRead allow_read)
{
    return CreateTmpFile(GetTmpNameEx(dir, prefix), text_binary, allow_read);
}



//////////////////////////////////////////////////////////////////////////////
// CDir


CDir::CDir(void)
{
    return;
}


#if defined(NCBI_OS_MAC)
CDir::CDir(const FSSpec& fss) : CParent(fss)
{
    // Set default mode
    SetDefaultMode(eDir, fDefault, fDefault, fDefault);
}
#endif


CDir::CDir(const string& dirname) : CParent(dirname)
{
    // Set default mode
    SetDefaultMode(eDir, fDefault, fDefault, fDefault);
}


#if defined(NCBI_OS_UNIX)

static bool s_GetHomeByUID(string& home)
{
    // Get the info using user ID
    struct passwd *pwd;

    if ((pwd = getpwuid(getuid())) == 0) {
        return false;
    }
    home = pwd->pw_dir;
    return true;
}

static bool s_GetHomeByLOGIN(string& home)
{
    char* ptr = 0;
    // Get user name
    if ( !(ptr = getenv("USER")) ) {
        if ( !(ptr = getenv("LOGNAME")) ) {
            if ( !(ptr = getlogin()) ) {
                return false;
            }
        }
    }
    // Get home dir for this user
    struct passwd* pwd = getpwnam(ptr);
    if ( !pwd ||  pwd->pw_dir[0] == '\0') {
        return false;
    }
    home = pwd->pw_dir;
    return true;
}
#endif // NCBI_OS_UNIX


string CDir::GetHome(void)
{
    char*  str;
    string home;

#if defined(NCBI_OS_MSWIN)
    // Get home dir from environment variables
    // like - C:\Documents and Settings\user\Application Data
    str = getenv("APPDATA");
    if ( str ) {
        home = str;
    } else {
        // like - C:\Documents and Settings\user
        str = getenv("USERPROFILE");
        if ( str ) {
            home = str;
        }
    }
   
#elif defined(NCBI_OS_UNIX)
    // Try get home dir from environment variable
    str = getenv("HOME");
    if ( str ) {
        home = str;
    } else {
        // Try to retrieve the home dir -- first use user's ID, and if failed, 
        // then use user's login name.
        if ( !s_GetHomeByUID(home) ) { 
            s_GetHomeByLOGIN(home);
        }
    }
   
#elif defined(NCBI_OS_MAC)
    // Make sure we can use FindFolder() if not, report error
    long gesResponse;
    if (Gestalt(gestaltFindFolderAttr, &gesResponse) != noErr  ||
        (gesResponse & (1 << gestaltFindFolderPresent) == 0)) {
        return kEmptyStr;
    }

    // Store the current active directory
    long  saveDirID;
    short saveVRefNum;
    HGetVol((StringPtr) 0, &saveVRefNum, &saveDirID);

    // Find the preferences folder in the active System folder
    short vRefNum;
    long  dirID;
    OSErr err = FindFolder(kOnSystemDisk, kPreferencesFolderType,
                           kCreateFolder, &vRefNum, &dirID);

    FSSpec spec;
    if (err == noErr) {
        char dummy[FILENAME_MAX+1];
        dummy [0] = '\0';
        err = FSMakeFSSpec(vRefNum, dirID, (StringPtr) dummy, &spec);
    }

    // Restore the current active directory
    HSetVol((StringPtr) 0, saveVRefNum, saveDirID);

    // Convert to C++ string
    if (err == noErr) {
        str = 0;
        err = MacFSSpec2FullPathname(&spec, &str);
        home = str;
    }
#endif 

    // Add trailing separator if needed
    return AddTrailingPathSeparator(home);
}


string CDir::GetCwd()
{
    string cwd;

#ifdef NCBI_OS_UNIX

    char buf[4096];
    if (getcwd(buf, sizeof(buf) - 1)) {
        cwd = buf;
    }

#elif defined(NCBI_OS_MSWIN)

    char buf[4096];
    if (_getcwd(buf, sizeof(buf) - 1)) {
        cwd = buf;
    }

#endif

    return cwd;
}


CDir::~CDir(void)
{
    return;
}


#if defined(NCBI_OS_MAC)
static const CDirEntry MacGetIndexedItem(const CDir& container, SInt16 index)
{
    FSSpec dir = container.FSS();
    FSSpec fss;     // FSSpec of item gotten.
    SInt16 actual;  // Actual number of items gotten.  Should be one or zero.
    SInt16 itemIndex = index;
    OSErr err = GetDirItems(dir.vRefNum, dir.parID, dir.name, true, true, 
                            &fss, 1, &actual, &itemIndex);
    if (err != noErr) {
        throw err;
    }
    return CDirEntry(fss);
}
#endif


CDir::TEntries CDir::GetEntries(const string& mask) const
{
    TEntries contents;
    string x_mask    = mask.empty() ? string("*") : mask;
    string path_base = GetPath();
    if ( path_base[path_base.size() - 1] != GetPathSeparator() ) {
        path_base += GetPathSeparator();
    }

#if defined(NCBI_OS_MSWIN)
    // Append to the "path" mask for all files in directory
    string pattern = path_base + x_mask;

    // Open directory stream and try read info about first entry
    struct _finddata_t entry;
    long desc = _findfirst(pattern.c_str(), &entry);  // get first entry's name
    if (desc != -1) {
        contents.push_back(new CDirEntry(path_base + entry.name));
        while ( _findnext(desc, &entry) != -1 ) {
            contents.push_back(new CDirEntry(path_base + entry.name));
        }
        _findclose(desc);
    }

#elif defined(NCBI_OS_UNIX)
    DIR* dir = opendir(GetPath().c_str());
    if ( dir ) {
        while (struct dirent* entry = readdir(dir)) {
            if ( MatchesMask(entry->d_name, x_mask.c_str()) ) {
                contents.push_back(new CDirEntry(path_base + entry->d_name));
            }
        }
        closedir(dir);
    }

#elif defined(NCBI_OS_MAC)
    try {
        for (int index = 1;  ;  index++) {
            CDirEntry entry = MacGetIndexedItem(*this, index);
            if ( MatchesMask(entry.GetName().c_str(), x_mask.c_str()) ) {
                contents.push_back(new CDirEntry(entry));
            }
        }
    } catch (OSErr& err) {
        if (err != fnfErr) {
            throw COSErrException_Mac(err, "CDir::GetEntries() ");
        }
    }
#endif

    return contents;
}


bool CDir::Create(void) const
{
    TMode user_mode, group_mode, other_mode;

    GetDefaultMode(&user_mode, &group_mode, &other_mode);
    TMode mode = s_ConstructMode(user_mode, group_mode, other_mode);

#if defined(NCBI_OS_MSWIN)
    if ( mkdir(GetPath().c_str()) != 0 ) {
        return false;
    }
    return chmod(GetPath().c_str(), mode) == 0;

#elif defined(NCBI_OS_UNIX)

    return mkdir(GetPath().c_str(), mode) == 0;

#elif defined(NCBI_OS_MAC)
    OSErr err;
    long dirID;
	
    err = ::FSpDirCreate(&FSS(), smRoman, &dirID);
    return err == noErr;

#endif
}

bool CDir::CreatePath(void) const
{
    if (Exists()) {
        return true;
    }
    string path(GetPath());
    if (path.empty()) {
        return true;
    }
    if (path[path.length()-1] == GetPathSeparator()) {
        path.erase(path.length()-1);
    }
    CDir dir_this(path);
    if (dir_this.Exists()) {
        return true;
    }
    CDir dir_up(dir_this.GetDir());
    if (dir_up.CreatePath()) {
        return dir_this.Create();
    }
    return false;
}

bool CDir::Remove(EDirRemoveMode mode) const
{
    // Remove directory as empty
    if ( mode == eOnlyEmpty ) {
        return CParent::Remove(eOnlyEmpty);
    }
    // Read all entryes in derectory
    TEntries contents = GetEntries();

    // Remove
    ITERATE(TEntries, entry, contents) {
        string name = (*entry)->GetName();
#if defined(NCBI_OS_MAC)
        CDirEntry& item = **entry;
#else
        if ( name == "."  ||  name == ".."  ||  
             name == string(1, GetPathSeparator()) ) {
            continue;
        }
        // Get entry item with full pathname
        CDirEntry item(GetPath() + GetPathSeparator() + name);
#endif
        if ( mode == eRecursive ) {
            if ( !item.Remove(eRecursive) ) {
                return false;
            }
        } else {
            if ( item.IsDir() ) {
                continue;
            }
            if ( !item.Remove() ) {
                return false;
            }
        }
    }
    // Remove main directory
    return CParent::Remove(eOnlyEmpty);
}


//////////////////////////////////////////////////////////////////////////////
// CMemoryFile


// Platform-dependent memory file handle definition
struct SMemoryFileHandle {
#if defined(NCBI_OS_MSWIN)
    HANDLE  hMap;
#else
    int     hDummy;
#endif
};


bool CMemoryFile::IsSupported(void)
{
#if defined(NCBI_OS_MAC)
    return false;
#else
    return true;
#endif
}


CMemoryFile::CMemoryFile(const string&  file_name,
                         EMemMapProtect protect,
                         EMemMapShare   share)
    : m_Handle(0), m_Size(-1), m_DataPtr(0)
{
    x_Map(file_name, protect, share);

    if (GetSize() < 0) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "File memory mapping cannot be created");
    }
}


CMemoryFile::~CMemoryFile(void)
{
    Unmap();
}


void CMemoryFile::x_Map(const string&  file_name,
                        EMemMapProtect protect_attr,
                        EMemMapShare   share_attr)
{
    if ( !IsSupported() )
        return;

    m_Handle = new SMemoryFileHandle();

    for (;;) { // quasi-TRY block

        CFile file(file_name);
        m_Size = file.GetLength();

        if (m_Size < 0)
            break;

        // Special case
        if (m_Size == 0)
            return;
	
#if defined(NCBI_OS_MSWIN)
        // Name of a file-mapping object cannot contain '\'
        string x_name = NStr::Replace(file_name, "\\", "/");

        // Translate attributes 
        DWORD map_protect = 0, map_access = 0, file_share = 0, file_access = 0;
        switch (protect_attr) {
            case eMMP_Read:
                map_access  = FILE_MAP_READ;
                map_protect = PAGE_READONLY;
                file_access = GENERIC_READ;
                break;
            case eMMP_Write:
            case eMMP_ReadWrite:
                // On MS Windows platform Write & ReadWrite access
                // to the mapped memory is equivalent
                if  (share_attr == eMMS_Shared ) {
                    map_access = FILE_MAP_ALL_ACCESS;
                } else {
                    map_access = FILE_MAP_COPY;
                }
                map_protect = PAGE_READWRITE;
                // So the file also must be open for reading and writing
                file_access = GENERIC_READ | GENERIC_WRITE;
                break;
            default:
                _TROUBLE;
        }
        if (share_attr == eMMS_Shared) {
            file_share = FILE_SHARE_READ | FILE_SHARE_WRITE;
        } else {
            file_share = 0;
        }

        // If failed to attach to an existing file-mapping object then
        // create a new one (based on the specified file)
        m_Handle->hMap = OpenFileMapping(map_access, false, x_name.c_str());
        if ( !m_Handle->hMap ) { 
            HANDLE hFile = CreateFile(x_name.c_str(), file_access, 
                                      file_share, NULL, OPEN_EXISTING, 
                                      FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE)
                break;

            m_Handle->hMap = CreateFileMapping(hFile, NULL, map_protect,
                                               0, 0, x_name.c_str());
            CloseHandle(hFile);
            if ( !m_Handle->hMap )
                break;
        }
        m_DataPtr = MapViewOfFile(m_Handle->hMap, map_access,
                                  0, 0, m_Size);
        if ( !m_DataPtr ) {
            CloseHandle(m_Handle->hMap);
            break;
        }

#elif defined(NCBI_OS_UNIX)
        // Translate attributes 
        int map_protect = 0, map_share = 0, file_access = 0;
        switch (protect_attr) {
            case eMMP_Read:
                map_protect = PROT_READ;
                file_access = O_RDONLY;
                break;
            case eMMP_Write:
                map_protect = PROT_WRITE;
                if  (share_attr == eMMS_Shared ) {
                    // Must be read + write
                    file_access = O_RDWR;
                } else {
                    file_access = O_RDONLY;
                }
                break;
            case eMMP_ReadWrite:
                map_protect = PROT_READ | PROT_WRITE;
                if  (share_attr == eMMS_Shared ) {
                    file_access = O_RDWR;
                } else {
                    file_access = O_RDONLY;
                }
                break;
            default:
                _TROUBLE;
        }
        switch (share_attr) {
            case eMMS_Shared:
                map_share = MAP_SHARED;
                break;
            case eMMS_Private:
                map_share = MAP_PRIVATE;
                break;
            default:
                _TROUBLE;
        }
        // Open file
        int fd = open(file_name.c_str(), file_access);
        if (fd < 0) {
            break;
        }
        // Map file to memory
        m_DataPtr = mmap(0, (size_t) m_Size, map_protect, map_share, fd, 0);
        close(fd);
        if (m_DataPtr == MAP_FAILED) {
            break;
        }
#endif

        // Success
        return;
    }

    // Error; cleanup
    delete m_Handle;
    m_Handle = 0;
    m_Size = -1;
}


bool CMemoryFile::Flush(void) const
{
    // If file is not mapped do nothing
    if ( !m_Handle )
        return false;
    bool status = false;

#if defined(NCBI_OS_MSWIN)
    status = (FlushViewOfFile(m_DataPtr, 0) != 0);
#elif defined(NCBI_OS_UNIX)
    status = (msync((char*)m_DataPtr, (size_t) m_Size, MS_SYNC) == 0);
#endif
    return status;
}


bool CMemoryFile::Unmap(void)
{
    // If file is not mapped do nothing
    if ( !m_Handle )
        return true;
    bool status = false;

#if defined(NCBI_OS_MSWIN)
    status = (UnmapViewOfFile(m_DataPtr) != 0);
    if ( status  &&  m_Handle->hMap )
        status = (CloseHandle(m_Handle->hMap) != 0);
#elif defined(NCBI_OS_UNIX)
    status = (munmap((char*)m_DataPtr, (size_t) m_Size) == 0);
#endif
    delete m_Handle;

    // Reinitialize members
    m_Handle = 0;
    m_DataPtr = 0;
    m_Size = -1;
    return status;
}


#if !defined(HAVE_MADVISE)

bool CMemoryFile::MemMapAdviseAddr(void*, size_t, EMemMapAdvise) {
    return true;
}
bool CMemoryFile::MemMapAdvise(EMemMapAdvise) {
    return true;
}

#else  /* HAVE_MADVISE */

bool CMemoryFile::MemMapAdviseAddr(void* addr, size_t len,
                                   EMemMapAdvise advise)
{
    int adv;
    if (!addr || !len) {
        return false;
    }
    switch (advise) {
    case eMMA_Random:
        adv = MADV_RANDOM;     break;
    case eMMA_Sequential:
        adv = MADV_SEQUENTIAL; break;
    case eMMA_WillNeed:
        adv = MADV_WILLNEED;   break;
    case eMMA_DontNeed:
        adv = MADV_DONTNEED;   break;
    default:
        adv = MADV_NORMAL;
    }
    // Conversion type of "addr" to char* -- Sun Solaris fix
    return madvise((char*)addr, len, adv) == 0;
}


bool CMemoryFile::MemMapAdvise(EMemMapAdvise advise)
{
    if (m_DataPtr  &&   m_Size > 0) {
        return MemMapAdviseAddr(m_DataPtr, m_Size, advise);
    } else {
        return false;
    }
}

#endif  /* HAVE_MADVISE */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.50  2003/08/08 13:37:17  siyan
 * Changed GetTmpNameExt to GetTmpNameEx in CFile, as this is the more
 * appropriate name.
 *
 * Revision 1.49  2003/06/18 18:57:43  rsmith
 * alternate implementation of GetTmpNameExt replacing tempnam with mktemp for library missing tempnam
 *
 * Revision 1.48  2003/05/29 17:21:04  gouriano
 * added CreatePath() which creates directories recursively
 *
 * Revision 1.47  2003/04/11 14:04:49  ivanov
 * Fixed CDirEntry::GetMode (Mac OS) - store a mode only for a notzero pointers.
 * Fixed CDir::GetEntries -- do not try to close a dir handle if it was
 * not opened.
 *
 * Revision 1.46  2003/04/04 16:02:37  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.45  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.44  2003/04/02 16:22:34  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.43  2003/04/02 13:29:29  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 1.42  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.41  2003/02/28 21:06:02  ivanov
 * Added close() call to CTmpStream destructor before the file deleting.
 *
 * Revision 1.40  2003/02/14 19:30:50  ivanov
 * Added mode ios::trunc by default for files creates in CFile::CreateTmpFile().
 * Get read of some warnings - initialize variables in CMemoryFile::x_Map().
 *
 * Revision 1.39  2003/02/06 16:14:28  ivanov
 * CMemomyFile::x_Map(): changed file access attribute from O_RDWR to
 * O_RDONLY in case of private sharing.
 *
 * Revision 1.38  2003/02/05 22:07:15  ivanov
 * Added protect and sharing parameters to the CMemoryFile constructor.
 * Added CMemoryFile::Flush() method.
 *
 * Revision 1.37  2003/01/16 13:27:56  dicuccio
 * Added CDir::GetCwd()
 *
 * Revision 1.36  2002/11/05 15:46:49  dicuccio
 * Changed CDir::GetEntries() to store fully qualified path names - this permits
 * CDirEntry::GetType() to work on files outside of the local directory.
 *
 * Revision 1.35  2002/10/01 17:23:57  gouriano
 * more fine-tuning of ConvertToOSPath
 *
 * Revision 1.34  2002/10/01 17:02:53  gouriano
 * minor modification of ConvertToOSPath
 *
 * Revision 1.33  2002/10/01 14:18:45  gouriano
 * "optimize" result of ConvertToOSPath
 *
 * Revision 1.32  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.31  2002/09/17 20:44:58  lavr
 * Add comment in unreachable return from CDirEntry::MatchesMask()
 *
 * Revision 1.30  2002/09/05 18:35:38  vakatov
 * Formal code rearrangement to get rid of comp.warnings;  some nice-ification
 *
 * Revision 1.29  2002/07/18 20:22:59  lebedev
 * NCBI_OS_MAC: fcntl.h added
 *
 * Revision 1.28  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.27  2002/07/11 19:28:30  ivanov
 * Removed test stuff from MemMapAdvise[Addr]
 *
 * Revision 1.26  2002/07/11 19:21:30  ivanov
 * Added CMemoryFile::MemMapAdvise[Addr]()
 *
 * Revision 1.25  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.24  2002/06/07 16:11:37  ivanov
 * Chenget GetTime() -- using CTime instead time_t, modification time
 * by default
 *
 * Revision 1.23  2002/06/07 15:21:06  ivanov
 * Added CDirEntry::GetTime()
 *
 * Revision 1.22  2002/05/01 22:59:00  vakatov
 * A couple of (size_t) type casts to avoid compiler warnings in 64-bit
 *
 * Revision 1.21  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.20  2002/04/01 18:49:54  ivanov
 * Added class CMemoryFile. Added including <windows.h> under MS Windows
 *
 * Revision 1.19  2002/03/25 17:08:17  ucko
 * Centralize treatment of Cygwin as Unix rather than Windows in configure.
 *
 * Revision 1.18  2002/03/22 20:00:01  ucko
 * Tweak to work on Cygwin.  (Use Unix rather than MSWIN code).
 *
 * Revision 1.17  2002/02/07 21:05:47  kans
 * implemented GetHome (FindFolder kPreferencesFolderType) for Mac
 *
 * Revision 1.16  2002/01/24 22:18:02  ivanov
 * Changed CDirEntry::Remove() and CDir::Remove()
 *
 * Revision 1.15  2002/01/22 21:21:09  ivanov
 * Fixed typing error
 *
 * Revision 1.14  2002/01/22 19:27:39  ivanov
 * Added realization ConcatPathEx()
 *
 * Revision 1.13  2002/01/20 06:13:34  vakatov
 * Fixed warning;  formatted the source code
 *
 * Revision 1.12  2002/01/10 16:46:36  ivanov
 * Added CDir::GetHome() and some CDirEntry:: path processing functions
 *
 * Revision 1.11  2001/12/26 21:21:05  ucko
 * Conditionalize deletion of m_FSS on NCBI_OS_MAC.
 *
 * Revision 1.10  2001/12/26 20:58:22  juran
 * Use an FSSpec* member instead of an FSSpec, so a forward declaration can
 * be used.
 * Add copy constructor and assignment operator for CDirEntry on Mac OS,
 * thus avoiding memberwise copy which would blow up upon deleting the
 * pointer twice.
 *
 * Revision 1.9  2001/12/18 21:36:38  juran
 * Remove unneeded Mac headers.
 * (Required functions copied to ncbi_os_mac.cpp)
 * MoveRename PStr to PString in ncbi_os_mac.hpp.
 * Don't use MoreFiles xxxCompat functions.  They're for System 6.
 * Don't use global scope operator on functions copied into NCBI scope.
 *
 * Revision 1.8  2001/11/19 23:38:44  vakatov
 * Fix to compile with SUN WorkShop (and maybe other) compiler(s)
 *
 * Revision 1.7  2001/11/19 18:10:13  juran
 * Whitespace.
 *
 * Revision 1.6  2001/11/15 16:34:12  ivanov
 * Moved from util to corelib
 *
 * Revision 1.5  2001/11/06 14:34:11  ivanov
 * Fixed compile errors in CDir::Contents() under MS Windows
 *
 * Revision 1.4  2001/11/01 21:02:25  ucko
 * Fix to work on non-MacOS platforms again.
 *
 * Revision 1.3  2001/11/01 20:06:48  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.2  2001/09/19 16:22:18  ucko
 * S_IFDOOR is nonportable; make sure it exists before using it.
 * Fix type of second argument to CTmpStream's constructor (caught by gcc 3).
 *
 * Revision 1.1  2001/09/19 13:06:09  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
