#ifndef CORELIB__NCBIFILE__HPP
#define CORELIB__NCBIFILE__HPP

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
 * Author: Vladimir Ivanov, Denis Vakatov
 *
 * File Description:  Files and directories accessory functions
 *
 */

#include <corelib/ncbistd.hpp>
#include <vector>


#ifdef NCBI_OS_MAC
struct FSSpec;
#endif


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Base class to work with files and directories
//
// We assume that the path argument has the following form, where any or all
// components may be missing.
//
// <dir><title><ext>
//
// dir   - file path             ("/usr/local/bin/"  or  "c:\windows\")
// title - file name without ext ("autoexec")
// ext   - file extension        (".bat" -- whatever goes after the last dot)
//
// Supported filename formats:  DOS, UNIX and MAC.
//

class CDirEntry
{
public:
    CDirEntry();
#ifdef NCBI_OS_MAC
    CDirEntry(const CDirEntry& other);
    CDirEntry& operator= (const CDirEntry& other);
    CDirEntry(const FSSpec& fss);
#endif
    CDirEntry(const string& path);
    void Reset(const string& path);
    virtual ~CDirEntry(void);

	bool operator== (const CDirEntry& other) const;
#ifdef NCBI_OS_MAC
	const FSSpec& FSS() const;
#endif
    // Get entry path
    string GetPath(void) const;


    //
    // Path processing
    //

    // Break a path into components.
    // "dir" will always have terminating path separator (e.g. "/usr/local/").
    // "ext" will always start from dot (e.g. ".bat").
    static void SplitPath(const string& path,
                          string* dir = 0, string* base = 0, string* ext = 0);
    string GetDir (void) const;  // directory this entry belongs to
    string GetName(void) const;  // base entry name (with extension)
    string GetBase(void) const;  // base entry name without extension
    string GetExt (void) const;  // entry name extension

    // Create a path from components
    static string MakePath(const string& dir  = kEmptyStr,
                           const string& base = kEmptyStr,
                           const string& ext  = kEmptyStr);

    // Get path separator symbol specific for the platform
    static char GetPathSeparator(void);

    // Check character "c" at path separator symbol specific for the platform
    static bool IsPathSeparator(const char c);

    // Add a trailing path separator (if needed)
    static string AddTrailingPathSeparator(const string& path);

    // Convert relative "path" (any OS) to current OS dependent relative path 
    static string ConvertToOSPath(const string& path);

    // Detect if the "path" is absolute.
    //  1) "path" must be for current OS 
    static bool IsAbsolutePath(const string& path);
    //  2) "path" can be for any OS (MSWIN, UNIX, MAC )
    static bool IsAbsolutePathEx(const string& path);

    // Concatenate 2 parts of the path. "first" path can be either absolute or
    // relative;  "second" must always be relative.
    //  1) "first" and "second" must be paths for the current OS
    static string ConcatPath(const string& first, const string& second);
    //  2) "first" and "second" can be paths for any OS (MSWIN, UNIX, MAC)
    static string ConcatPathEx(const string& first, const string& second);


    //
    // Checks & manipulations
    //

    // Match "name" against the filename "mask"
    static bool MatchesMask(const char *name, const char *mask);

    // Check existence of entry "path"
    virtual bool Exists(void) const;

    // Rename entry
    bool Rename(const string& new_path);

    // Directory remove mode
    enum EDirRemoveMode {
        eOnlyEmpty,     // Remove only empty directory
        eNonRecursive,  // Remove all files in directory, but not remove
                        // subdirectories and files in it.
        eRecursive      // Remove all files and subdirectories
    };

    // Remove entry
    // NOTE: "mode" appoint removing method only for directories
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
    
    // See also GetType() below for other entry types
    bool IsFile(void) const;
    bool IsDir(void) const;

    // Get type of entry
    enum EType {
        eFile = 0,     // Regular file
        eDir,          // Directory
        ePipe,         // Pipe
        eLink,         // Symbolic link     (UNIX only)
        eSocket,       // Socket            (UNIX only)
        eDoor,         // Door              (UNIX only)
        eBlockSpecial, // Block special     (UNIX only)
        eCharSpecial,  // Character special
        //
        eUnknown       // Unknown type
    };
    // NOTE: On error (e.g. if the entry does not exist), return "eUnknown".
    EType GetType(void) const;


    //
    // Access permissions
    //

    // Entry's access permissions
    enum EMode {
        fExecute = 1,
        fWrite   = 2,
        fRead    = 4,
        // initial defaults for dirs
        fDefaultDirUser  = fRead | fExecute | fWrite,
        fDefaultDirGroup = fRead | fExecute,
        fDefaultDirOther = fRead | fExecute,
        // initial defaults for non-dir entries (files, etc.)
        fDefaultUser     = fRead | fWrite,
        fDefaultGroup    = fRead,
        fDefaultOther    = fRead,
        // special flag:  ignore all other flags, use current default mode
        fDefault = 8
    };
    typedef unsigned int TMode;

    // Get this (existing) entry's mode (permissions).
    // NOTE: On WINDOWS, there is only "user_mode". "group_" and "other_"
    // modes will be ignored.
    bool GetMode(TMode* user_mode,
                 TMode* group_mode = 0,
                 TMode* other_mode = 0) const;

    // Set mode for this (existing) entry.
    // "fDefault" will set the mode as specified in the SetDefaultMode(),
    // or (if SetDefaultMode() was not called for this object or was
    // called with "fDefault") as specified in the SetDefaultModeGlobal(), or
    // else as specific for this class of entry (e.g. "fDefaultDir*" for dirs).
    bool SetMode(TMode user_mode,  // e.g. fDefault
                 TMode group_mode = fDefault,
                 TMode other_mode = fDefault) const;

    // Set default mode for all CDirEntry objects (except for those having
    // their own mode set with SetDefaultMode()).
    static void SetDefaultModeGlobal(EType entry_type,
                                     TMode user_mode,  // e.g. fDefault
                                     TMode group_mode = fDefault,
                                     TMode other_mode = fDefault);

    // Set mode for this one object only.
    // By default ("fDefault), mode will be set to the current global mode
    // (specified by SetDefaultModeGlobal()).
    virtual void SetDefaultMode(EType entry_type,
                                TMode user_mode,  // e.g. fDefault
                                TMode group_mode = fDefault,
                                TMode other_mode = fDefault);

protected:
    static void GetDefaultModeGlobal(EType  entry_type,
                                     TMode* user_mode,
                                     TMode* group_mode,
                                     TMode* other_mode);
    void GetDefaultMode(TMode* user_mode,
                        TMode* group_mode,
                        TMode* other_mode) const;

private:
#ifdef NCBI_OS_MAC
    FSSpec* m_FSS;
#else
    // Full entry path
    string m_Path;
#endif

    // Global and local current default mode (NOTE: no "fDefault" here!)
    enum EWho {
        eUser = 0,
        eGroup,
        eOther
    };
    static TMode m_DefaultModeGlobal[eUnknown][3/*EWho*/];
    TMode        m_DefaultMode[3/*EWho*/];
};



//////////////////////////////////////////////////////////////////////////////
//
// Class for support work with files
//
// NOTE: Next functions are unsafe in multithreaded applications:
//       - GetTmpName() (use GetTmpNameExt() -- it is safe);
//       - CreateTmpFile() with empty first parameter
//         (use CreateTmpFileExt() -- it is safe);
//

class CFile : public CDirEntry
{
    typedef CDirEntry CParent;
public:
    CFile(const string& file);
    virtual ~CFile(void);

    // Check existence for file "filename"
    virtual bool Exists(void) const;

    // Get size of file. Return "-1" if was error obtaining size.
    Int8 GetLength(void) const;

    // Get temporary file name
    // NOTE:
    //   - return "kEmptyStr" if was error get temporary file name
    //   - the "prefix" may be don't specified or it must be up to five
    //     characters length
    static string GetTmpName(void);
    static string GetTmpNameExt(const string& dir,
                                const string& prefix = kEmptyStr);


    // Create a temporary file, return pointer to corresponding stream.
    // On error, NULL will be returned.
    // NOTE: The temporary file will be automatically deleted after
    //       delete streem object.
    //       If file exists before call function, then after call it will
    //       be removed also.
    enum ETextBinary {
        eText,
        eBinary
    };
    enum EAllowRead {
        eAllowRead,
        eWriteOnly
    };
    static fstream* CreateTmpFile(const string& filename = kEmptyStr,
                                  ETextBinary text_binary = eBinary,
                                  EAllowRead  allow_read  = eAllowRead);
    static fstream* CreateTmpFileEx(const string& dir = ".",
                                    const string& prefix = kEmptyStr,
                                    ETextBinary text_binary = eBinary,
                                    EAllowRead  allow_read  = eAllowRead);
};



//////////////////////////////////////////////////////////////////////////////
//
// Class for support work with directories
//
//
// NOTE: Next functions are unsafe in multithreaded applications:
//       - static bool Exists() (for Mac only);
//       - bool Exists() (for Mac only);
//       - iterators (also is unsafe in single threaded applications if
//         one iterator use inside another);

class CDir : public CDirEntry
{
    typedef CDirEntry CParent;
public:
    CDir();
#ifdef NCBI_OS_MAC
    CDir(const FSSpec& fss);
#endif
    CDir(const string& dirname);
    virtual ~CDir(void);

    // Check if directory "dirname" exists
    virtual bool Exists(void) const;

    // Get user "home" dir
    static string GetHome(void);

    // Return an array containing all directory entries.
    // Use file name mask "mask" (if passed non-empty).
    typedef vector< AutoPtr<CDirEntry> > TEntries;
    TEntries GetEntries(const string& mask = kEmptyStr) const;

    // Create the directory using "dirname" passed in the constructor
    bool Create(void) const;

    // Delete existing directory
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
};



//////////////////////////////////////////////////////////////////////////////
//
// Inline
//


// CDirEntry

#ifndef NCBI_OS_MAC
inline
void CDirEntry::Reset(const string& path)
{
    m_Path = path;
}

inline
string CDirEntry::GetPath(void) const
{
    return m_Path;
}
#endif

inline
string CDirEntry::GetDir(void) const
{
    string dir;
    SplitPath(GetPath(), &dir);
    return dir;
}

inline
string CDirEntry::GetName(void) const
{
    string title, ext;
    SplitPath(GetPath(), 0, &title, &ext);
    return title + ext;
}

inline
string CDirEntry::GetBase(void) const
{
    string base;
    SplitPath(GetPath(), 0, &base);
    return base;
}

inline
string CDirEntry::GetExt(void) const
{
    string ext;
    SplitPath(GetPath(), 0, 0, &ext);
    return ext;
}

inline
bool CDirEntry::IsFile(void) const
{
    return GetType() == eFile;
}

inline
bool CDirEntry::IsDir(void) const
{
    return GetType() == eDir;
}

inline
bool CDirEntry::Exists(void) const
{
    return GetType() != eUnknown;
}


// CFile

inline
bool CFile::Exists(void) const
{
    return GetType() == eFile;
}


// CDir

inline
bool CDir::Exists(void) const
{
    return GetType() == eDir;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2002/01/24 22:17:40  ivanov
 * Changed CDirEntry::Remove() and CDir::Remove()
 *
 * Revision 1.8  2002/01/10 16:46:09  ivanov
 * Added CDir::GetHome() and some CDirEntry:: path processing functions
 *
 * Revision 1.7  2001/12/26 20:58:23  juran
 * Use an FSSpec* member instead of an FSSpec, so a forward declaration can 
 * be used.
 * Add copy constructor and assignment operator for CDirEntry on Mac OS,
 * thus avoiding memberwise copy which would blow up upon deleting the 
 * pointer twice.
 *
 * Revision 1.6  2001/12/13 20:14:34  juran
 * Add forward declaration of struct FSSpec for Mac OS.
 *
 * Revision 1.5  2001/11/19 18:07:38  juran
 * Change Contents() to GetEntries().
 * Implement MatchesMask().
 *
 * Revision 1.4  2001/11/15 16:30:46  ivanov
 * Moved from util to corelib
 *
 * Revision 1.3  2001/11/01 21:02:18  ucko
 * Fix to work on non-MacOS platforms again.
 *
 * Revision 1.2  2001/11/01 20:06:49  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.1  2001/09/19 13:04:18  ivanov
 * Initial revision
 * ===========================================================================
 */


#endif  /* CORELIB__NCBIFILE__HPP */
