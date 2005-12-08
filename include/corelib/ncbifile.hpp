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
 * Authors: Vladimir Ivanov, Denis Vakatov
 *
 *
 */

/// @file ncbifile.hpp
///
/// Defines classes CDirEntry, CFile, CDir, CSymLink, CMemoryFile,
/// CFileException to allow various file and directory operations.
///

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_mask.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <vector>

#include <sys/types.h>
#if defined(HAVE_SYS_STAT_H)
#  include <sys/stat.h>
#endif

/** @addtogroup Files
 *
 * @{
 */


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// CFileException --
///
/// Define exceptions generated for file operations.
///
/// CFileException inherits its basic functionality from CCoreException
/// and defines additional error codes for file operations.

class CFileException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eMemoryMap,
        eRelativePath,
        eNotExists,
        eDiskInfo
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMemoryMap:    return "eMemoryMap";
        case eRelativePath: return "eRelativePath";
        case eNotExists:    return "eNotExists";
        case eDiskInfo:     return "eDiskInfo";
        default:            return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CFileException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDirEntry --
///
/// Base class to work with files and directories.
///
/// Models a directory entry in the file system.  Assumes that
/// the path argument has the following form, where any or
/// all components may be missing:
///
/// <dir><base><ext>
///
/// - dir  - file path             ("/usr/local/bin/"  or  "c:\\windows\\")
/// - base - file name without ext ("autoexec")
/// - ext  - file extension        (".bat" - whatever goes after a last dot)
///
/// Supported filename formats:  MS DOS/Windows, UNIX, and MAC.

class NCBI_XNCBI_EXPORT CDirEntry
{
public:
    /// Default constructor.
    CDirEntry(void);

    /// Constructor using specified path string.
    CDirEntry(const string& path);

    /// Copy constructor.
    CDirEntry(const CDirEntry& other);

    /// Destructor.
    virtual ~CDirEntry(void);

    /// Get entry path.
    const string& GetPath(void) const;

    /// Reset path string.
    void Reset(const string& path);

    /// Assignment operator.
    CDirEntry& operator= (const CDirEntry& other);


    //
    // Path processing.
    //

    /// Split a path string into its basic components.
    ///
    /// @param path
    ///   Path string to be split.
    /// @param dir
    ///   The directory component that is returned. This will always have
    ///   a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   File name with both directory (if any) and extension (if any)
    ///   parts stripped.
    /// @param ext
    ///   The extension component (if any), always has a leading dot
    ///   (example: ".bat").
    static void SplitPath(const string& path,
                          string* dir = 0, string* base = 0, string* ext = 0);

    /// Get the directory component for this directory entry.
    string GetDir (void) const;

    /// Get the base entry name with extension (if any).
    string GetName(void) const;

    /// Get the base entry name without extension.
    string GetBase(void) const;

    /// Get extension name.
    string GetExt (void) const;

    /// Assemble a path from basic components.
    ///
    /// @param dir
    ///   The directory component to make the path string. This will always
    ///   have a terminating path separator (example: "/usr/local/").
    /// @param base
    ///   The base name of the file component that is used to make up the path.
    /// @param ext
    ///   The extension component. This will always be added with a leading dot
    ///   (input of either "bat" or ".bat" gets added to the path as ".bat").
    /// @return
    ///   Path built from the components.
    static string MakePath(const string& dir  = kEmptyStr,
                           const string& base = kEmptyStr,
                           const string& ext  = kEmptyStr);

    /// Get path separator symbol specific for the current platform.
    static char GetPathSeparator(void);

    /// Check whether a character "c" is a path separator symbol
    /// specific for the current platform.
    static bool IsPathSeparator(const char c);

    /// Add trailing path separator, if needed.
    static string AddTrailingPathSeparator(const string& path);

    /// Delete trailing path separator, if any.
    static string DeleteTrailingPathSeparator(const string& path);

    /// Convert relative "path" on any OS to the current
    /// OS-dependent relative path. 
    static string ConvertToOSPath(const string& path);

    /// Check if a "path" is absolute for the current OS.
    ///
    /// Note that the "path" must be for the current OS. 
    static bool IsAbsolutePath(const string& path);

    /// Check if the "path" is absolute for any OS.
    ///
    /// Note that the "path" can be for any OS (MSWIN, UNIX, MAC).
    static bool IsAbsolutePathEx(const string& path);

    /// Create a relative path between two points in the file
    /// system specified by their absolute paths.
    ///
    /// @param path_from 
    ///   Absolute path that defines start of the relative path.
    /// @param path_to
    ///   Absolute path that defines endpoint of the relative path.
    /// @return
    ///   Relative path (empty string if the paths are the same).
    ///   Throw CFileException on error (e.g. if any of the paths is not
    ///   absolute, or if it is impossible to create a relative path, such
    ///   as in case of different disks on MS-Windows).
    static string CreateRelativePath(const string& path_from, 
                                     const string& path_to);

    /// Concatenate two parts of the path for the current OS.
    ///
    /// Note that the arguments must be OS-specific.
    /// @param first
    ///   First part of the path which can be either absolute or relative.
    /// @param second
    ///   Fecond part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPath(const string& first, const string& second);

    /// Concatenate two parts of the path for any OS.
    ///
    /// Note that the arguments are not OS-specific.
    /// @param first
    ///   First part of the path which can be either absolute or relative.
    /// @param second
    ///   Second part of the path must always be relative.
    /// @return
    ///   The concatenated path.
    static string ConcatPathEx(const string& first, const string& second);

    /// Normalize a path.
    ///
    /// Remove from an input "path" all redundancy, and if possible,
    /// convert it to more simple form for the current OS.
    /// Note that "path" must be OS-specific.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases)
    static string NormalizePath(const string& path,
                                EFollowLinks  follow_links = eIgnoreLinks);


    //
    // Checks & manipulations.
    //

    /// Match a "name" against a simple filename "mask".
    static bool MatchesMask(const char* name, const char* mask,
                            NStr::ECase use_case = NStr::eCase);

    /// Match a "name" against a set of "masks"
    static bool MatchesMask(const char* name, const CMask& mask,
                            NStr::ECase use_case = NStr::eCase);

    /// Check the entry existence.
    virtual bool Exists(void) const;

    /// Copy flags.
    /// Note that update modification time for directory depends on the OS.
    /// Normally it gets updated when a new directory entry is added/removed.
    /// On the other hand, changing contents of files of that directory
    /// doesn't usually affect the directory modification time.
    enum ECopyFlags {
        /// The following three flags define what to do when the
        /// destination entry already exists:
        /// - Overwrite the destination
        fCF_Overwrite       = (1<< 1), 
        /// - Update older entries only (compare modification times).
        fCF_Update          = (1<< 2) | fCF_Overwrite,
        /// - Backup destination if it exists.
        fCF_Backup          = (1<< 3) | fCF_Overwrite,
        /// All above flags can be applied to the top directory only
        /// (not for every file therein), to process the directory
        /// as a single entity for overwriting, updaing or backing up.
        fCF_TopDirOnly      = (1<< 6),
        /// If destination entry exists, it must have the same type as source.
        fCF_EqualTypes      = (1<< 7),
        /// Copy entries following their sym.links, not the links themselves.
        fCF_FollowLinks     = (1<< 8),
        fCF_Verify          = (1<< 9),  ///< Verify data after copying
        fCF_PreserveOwner   = (1<<10),  ///< Preserve owner/group
        fCF_PreservePerm    = (1<<11),  ///< Preserve permissions/attributes
        fCF_PreserveTime    = (1<<12),  ///< Preserve date/times
        fCF_PreserveAll     = fCF_PreserveOwner | fCF_PreservePerm |
                              fCF_PreserveTime,
        fCF_Recursive       = (1<<14),  ///< Copy recursively (for dir only)
        /// Skip all entries for which we don't have Copy() method.
        fCF_SkipUnsupported = (1<<15),
        /// Default flags.
        fCF_Default         = fCF_Recursive | fCF_FollowLinks
    };
    typedef unsigned int TCopyFlags;    ///< Binary OR of "ECopyFlags"

    /// Copy the entry to a location specified by "new_path".
    ///
    /// The Copy() method must be overloaded in derived classes
    /// that support copy operation.
    /// @param new_path
    ///   New path/name of an entry.
    /// @param flags
    ///   Flags specifying how to copy the entry.
    /// @param buf_size
    ///   Buffer size to use while copying the file contents.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   CFile::Copy, CDir::Copy, CLink::Copy, CopyToDir
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0);

    /// Copy the entry to a specified directory.
    ///
    /// The target entry name will be "dir/entry".
    /// @param dir
    ///   Directory name to copy into.
    /// @param flags
    ///   Flags specifying how to copy the entry.
    /// @param buf_size
    ///   Buffer size to use while copying the file contents.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   Copy
    bool CopyToDir(const string& dir, TCopyFlags flags = fCF_Default,
                   size_t buf_size = 0);

    /// Rename flags
    enum ERenameFlags {
        /// Remove destination if it exists.
        fRF_Overwrite   = (1<<1),
        /// Update older entries only (compare modification times).
        fRF_Update      = (1<<2) | fCF_Overwrite,
        /// Backup destination if it exists before renaming.
        fRF_Backup      = (1<<3) | fCF_Overwrite,
        /// If destination entry exists, it must have the same type as source.
        fRF_EqualTypes  = (1<<4),
        /// Rename entries following sym.links, not the links themselves.
        fRF_FollowLinks = (1<<5),
        /// Default flags
        fRF_Default     = 0
    };
    typedef unsigned int TRenameFlags;   ///< Binary OR of "ERenameFlags"

    /// Rename entry.
    ///
    /// @param new_path
    ///   New path/name of an entry.
    /// @param flags
    ///   Flags specifying how to rename the entry.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    ///   NOTE that if flag fRF_Update is set, the function returns TRUE and
    ///   just removes the current entry in case when destination entry
    ///   exists and has its modification time newer than the current entry.
    /// @sa
    ///   ERenameFlags, Copy
    bool Rename(const string& new_path, TRenameFlags flags = fRF_Default);

    /// Get backup suffix.
    ///
    /// @sa
    ///   SetBackupSuffix, Backup, Rename, Copy
    static const string& GetBackupSuffix(void);

    /// Set backup suffix.
    ///
    /// @sa
    ///   GetBackupSuffix, Backup, Rename, Copy
    static void SetBackupSuffix(const string& suffix);

    /// Backup modes
    enum EBackupMode {
        eBackup_Copy    = (1<<1),       ///< Copy entry
        eBackup_Rename  = (1<<2),       ///< Rename entry
        eBackup_Default = eBackup_Copy  ///< Default mode
    };

    /// Backup an entry.
    ///
    /// Create a copy of the current entry with the same name and an extension
    /// specified by SetBackupSuffix(). By default this extension is ".bak".
    /// Backups can be automatically created in 'copy' or 'rename' operations.
    /// If an entry with the name of the backup already exists, then it will
    /// be deleted (if possible). The current entry name components are
    /// changed to reflect the backed up copy.
    /// @param suffix
    ///   Extension to add to backup entry. If empty, GetBackupSuffix()
    ///   is be used.
    /// @param mode
    ///   Backup mode. Specifies what to do, copy the entry or just rename it.
    /// @param copyflags
    ///   Flags to copy the entry. Used only if mode is eBackup_Copy,
    /// @param copybufsize
    ///   Buffer size to use while copying the file contents.
    ///   Used only if 'mode' is eBackup_Copy,
    /// @return
    ///   TRUE if backup created successfully; FALSE otherwise.
    /// @sa
    ///   EBackupMode
    bool Backup(const string& suffix      = kEmptyStr, 
                EBackupMode   mode        = eBackup_Default,
                TCopyFlags    copyflags   = fCF_Default,
                size_t        copybufsize = 0);

    /// Directory remove mode.
    enum EDirRemoveMode {
        eOnlyEmpty,     ///< Remove only empty directory
        eNonRecursive,  ///< Remove all files in the directory, but do not
                        ///< remove non-empty subdirectories and files in them
                        ///< (it removes empty child directories though)
        eRecursive,     ///< Remove all files and subdirectories
        eTopDirOnly     ///< Same as eNonRecursive but preserves
                        ///< empty child directories
    };

    /// Remove a directory entry.
    ///
    /// Removes directory using the specified "mode".
    /// @sa
    ///   EDirRemoveMode
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
    
    /// Directory entry type.
    enum EType {
        eFile = 0,     ///< Regular file
        eDir,          ///< Directory
        ePipe,         ///< Pipe
        eLink,         ///< Symbolic link     (UNIX only)
        eSocket,       ///< Socket            (UNIX only)
        eDoor,         ///< Door              (UNIX only)
        eBlockSpecial, ///< Block special     (UNIX only)
        eCharSpecial,  ///< Character special
        //
        eUnknown       ///< Unknown type
    };

    /// Construct a directory entry object of a specified type.
    ///
    /// An object of specified type will be constucted in memory only,
    /// file sytem will not be modified.
    /// @param type
    ///   Define a type of the object to create.
    /// @return
    ///   A pointer to newly created entry. If a class for specified type
    ///   is not defined, generic CDirEntry will be returned. Do not forget
    ///   to delete the returned pointer when it is no longer used.
    /// @sa
    ///   CFile, CDir, CSymLink
    static CDirEntry* CreateObject(EType type, const string& path = kEmptyStr);

    /// Get a type of constructed object.
    ///
    /// @return
    ///   Return one of the values in EType. Return "eUnknown" for CDirEntry.
    /// @sa
    ///   CreateObject, GetType
    virtual EType GetObjectType(void) const { return eUnknown; };

    /// Alternate stat structure for use instead of the standard struct stat.
    /// The alternate stat can have useful, but non-posix fields, which
    /// are usually highly platform-dependent, and named differently
    /// in the underlying data structures on different systems.
    struct SStat {
        struct stat orig;  ///< Original stat structure
        // Nanoseconds for dir entry times (if available)
        long  mtime_nsec;  ///< Nanoseconds for modification time
        long  ctime_nsec;  ///< Nanoseconds for creation time
        long  atime_nsec;  ///< Nanoseconds for last access time
    };

    /// Get status information on a dir entry.
    ///
    /// By default have the same behaviour as UNIX's lstat().
    /// @param buffer
    ///   Pointer to structure that stores results.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases).
    /// @return
    ///   Return TRUE if the file-status information is obtained,
    ///   FALSE otherwise (errno may be set).
    bool Stat(struct SStat *buffer,
              EFollowLinks follow_links = eIgnoreLinks) const;

    /// Get a type of a directory entry.
    ///
    /// @return
    ///   Return one of the values in EType. If the directory entry does
    ///   not exist, return "eUnknown".
    /// @sa
    ///   IsFile, IsDir, IsLink
    EType GetType(EFollowLinks follow = eIgnoreLinks) const;

    /// Get a type of a directory entry by status information.
    ///
    /// @param st
    ///   Status file information.
    /// @return
    ///   Return one of the values in EType. If the directory entry does
    ///   not exist, return "eUnknown".
    /// @sa
    ///   IsFile, IsDir, IsLink
    static EType GetType(const struct stat& st);

    /// Check whether a directory entry is a file.
    /// @sa
    ///   GetType
    bool IsFile(EFollowLinks follow = eFollowLinks) const;

    /// Check whether a directory entry is a directory.
    /// @sa
    ///   GetType
    bool IsDir(EFollowLinks follow = eFollowLinks) const;

    /// Check whether a directory entry is a symbolic link (alias).
    /// @sa
    ///   GetType
    bool IsLink(void) const;

    /// Get an entry name that a link points to.
    ///
    /// @return
    ///   The real entry name that the link points to. Return an empty
    ///   string if the entry is not a link, or cannot be dereferenced.
    ///   The dereferenced name can be another symbolic link.
    /// @sa 
    ///   GetType, IsLink, DereferenceLink
    string LookupLink(void);

    /// Dereference a link.
    ///
    /// If the current entry is a symbolic link, then dereference it
    /// recursively until it is no further a link (but a file, directory,
    /// etc, or does not exist). Replace the entry path string with
    /// the dereferenced path.
    /// @sa 
    ///   GetType, IsLink, LookupLink, GetPath
    void DereferenceLink(void);

    /// Get time stamp(s) of a directory entry.
    ///
    /// The creation time under MS windows is an actual creation time of the
    /// entry. Under UNIX "creation" time is the time of last entry status
    /// change. If either OS or file system does not support some time type
    /// (modification/creation/last access), then the corresponding CTime
    /// gets returned "empty".
    /// Returned times are always in CTime's time zone format (eLocal/eGMT). 
    /// NOTE that what GetTime returns may not be equal to the the time(s)
    /// previously set by SetTime, as the behavior depends on the OS and
    /// the file system used.
    /// @return
    ///   TRUE if time(s) obtained successfully, FALSE otherwise.
    /// @sa
    ///   GetTimeT, SetTime, Stat
    bool GetTime(CTime* modification,
                 CTime* creation    = 0, 
                 CTime* last_access = 0) const;

    /// Get time stamp(s) of a directory entry (time_t version).
    ///
    /// Use GetTime() if you need precision of more than 1 second.
    /// Returned time(s) are always in GMT format. 
    /// @return
    ///   TRUE if time(s) obtained successfully, FALSE otherwise.
    /// @sa
    ///   GetTime, SetTimeT
    bool GetTimeT(time_t* modification,
                  time_t* creation    = 0, 
                  time_t* last_access = 0) const;

    /// Set time stamp(s) of a directory entry.
    ///
    /// The process must own the file or have write permissions in order
    /// to change the time(s). Any parameter with a value of zero will
    /// not cause the corresponding time stamp change.
    /// NOTE that what GetTime can later return may not be equal to the
    /// the time(s) set by SetTime, as the behavior depends on the OS and
    /// the file system used.
    /// Also, on UNIX it is impossible to change creation time of an entry
    /// (we silently ignore the "creation" time stamp on that platform).
    /// @param modification
    ///   Entry modification time to set.
    /// @param creation
    ///   Entry creation time to set. On some platforms it cannot be changed,
    ///   so this parameter will be quietly ignored.
    /// @param last_access
    ///   Entry last access time to set. It cannot be less than the entry
    ///   creation time, otherwise it will be set equal to the creation time.
    /// @return
    ///   TRUE if the time(s) changed successfully, FALSE otherwise.
    /// @sa
    ///   SetTimeT, GetTime
    bool SetTime(CTime* modification = 0,
                 CTime* creation     = 0,
                 CTime* last_access  = 0) const;

    /// Set time stamp(s) of a directory entry (time_t version).
    ///
    /// Use SetTime() if you need precision of more than 1 second.
    /// @param modification
    ///   Entry modification time to set.
    /// @param creation
    ///   Entry creation time to set. On some platforms it cannot be changed,
    ///   so this parameter will be quietly ignored.
    /// @param last_access
    ///   Entry last access time to set. It cannot be less than the entry
    ///   creation time, otherwise it will be set equal to the creation time.
    /// @return
    ///   TRUE if the time(s) changed successfully, FALSE otherwise.
    /// @sa
    ///   SetTime, GetTimeT
    bool SetTimeT(time_t* modification = 0,
                  time_t* creation     = 0,
                  time_t* last_access  = 0) const;


    /// What IsNewer() should do if the dir entry does not exist or
    /// is not accessible.
    /// @sa IsNewer
    enum EIfAbsent {
        eIfAbsent_Throw,    ///< Throw an exception
        eIfAbsent_Newer,    ///< Deem absent entry to be "newer"
        eIfAbsent_NotNewer  ///< Deem absent entry to be "older"
    };

    /// Check if the current entry is newer than a specified date/time.
    ///
    /// @param tm
    ///   Time to compare with the current entry modification time.
    /// @param if_absent
    ///   What to do if If the entry does not exist or is not accessible.
    /// @return
    ///   TRUE if the entry's modification time is newer than specified time.
    ///   Return FALSE otherwise.
    /// @sa
    ///   GetTime, EIfAbsent
    bool IsNewer(time_t    tm, 
                 EIfAbsent if_absent /* = eIfAbsent_Throw*/) const;

    /// Check if the current entry is newer than a specified date/time.
    ///
    /// @param tm
    ///   Time to compare with the current entry modification time.
    /// @param if_absent
    ///   What to do if If the entry does not exist or is not accessible.
    /// @return
    ///   TRUE if the entry's modification time is newer than specified time.
    ///   Return FALSE otherwise.
    /// @sa
    ///   GetTime, EIfAbsent
    bool IsNewer(const CTime& tm,
                 EIfAbsent    if_absent /* = eIfAbsent_Throw*/) const;

    /// What path version of IsNewer() should do if the dir entry or specified
    /// path does not exist or is not accessible. Default flags (0) mean
    /// throwing an exceptions if one of dir entries does not exists.
    /// @sa IsNewer
    enum EIfAbsent2 {
        fHasThisNoPath_Newer    = (1 << 0),
        fHasThisNoPath_NotNewer = (1 << 1),
        fNoThisHasPath_Newer    = (1 << 2),
        fNoThisHasPath_NotNewer = (1 << 3),
        fNoThisNoPath_Newer     = (1 << 4),
        fNoThisNoPath_NotNewer  = (1 << 5)
    };
    typedef int TIfAbsent2;   ///< Binary OR of "EIfAbsent2"

    /// Check if the current entry is newer than some other.
    ///
    /// @param path
    ///   An entry name, of which to compare the modification times.
    /// @return
    ///   TRUE if the modification time of the current entry is newer than
    ///   the modification time of the specified entry, or if that entry
    ///   doesn't exist. Return FALSE otherwise.
    /// @sa
    ///   GetTime
    bool IsNewer(const string& path,
                 TIfAbsent2    if_absent /* = 0 throw*/) const;

    /// Check if the current entry and the given entry_name are identical.
    ///
    /// Note that entries can be checked accurately only on UNIX.
    /// On MS Windows function can check it only by file name.
    /// @param entry_name
    ///   An entry name, of which to compare current entry.
    /// @return
    ///   TRUE if both entries exists and identical. Return FALSE otherwise.
    /// @param follow_links
    ///   Whether to follow symlinks (shortcuts, aliases)
    bool IsIdentical(const string& entry_name,
                     EFollowLinks  follow_links = eIgnoreLinks) const;

    /// Get an entry owner.
    ///
    /// WINDOWS:
    ///   Retrieve the name of the account and the name of the first
    ///   group, which the account belongs to. The obtained group name may
    ///   be an empty string, if we don't have permissions to get it.
    ///   Win32 really does not use groups, but they exist for the sake
    ///   of POSIX compatibility.
    ///   Windows 2000/XP: In addition to looking up for local accounts,
    ///   local domain accounts, and explicitly trusted domain accounts,
    ///   it also can look for any account in any known domain around.
    /// UNIX:
    ///   Retrieve an entry owner:group pair.
    /// @param owner
    ///   Pointer to a string to receive an owner name.
    /// @param group
    ///   Pointer to a string to receive a group name. 
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   SetOwner
    bool GetOwner(string* owner, string* group = 0,
                  EFollowLinks follow = eFollowLinks) const;

    /// Set an entry owner.
    ///
    /// You should have administrative rights to change an owner.
    /// WINDOWS:
    ///   Only administrative privileges (Backup, Restore and Take Ownership)
    ///   grant rights to change ownership. Without one of the privileges, an
    ///   administrator cannot take ownership of any file or give ownership
    ///   back to the original owner. Also, we cannot change user group here,
    ///   so it will be ignored.
    /// UNIX:
    ///   The owner of an entry can change the group to any group of which
    ///   that owner is a member of. The super-user may assign the group
    ///   arbitrarily.
    /// @param owner
    ///   New owner name to set.
    /// @param group
    ///   New group name to set.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   GetOwner
    bool SetOwner(const string& owner, const string& group = kEmptyStr,
                  EFollowLinks follow = eFollowLinks) const;

    //
    // Access permissions.
    //

    /// Directory entry access permissions.
    enum EMode {
        fExecute = 1,       ///< Execute permission
        fWrite   = 2,       ///< Write permission
        fRead    = 4,       ///< Read permission
        // initial defaults for directories
        fDefaultDirUser  = fRead | fExecute | fWrite,
                            ///< Default user permission for a dir.
        fDefaultDirGroup = fRead | fExecute,
                            ///< Default group permission for a dir.
        fDefaultDirOther = fRead | fExecute,
                            ///< Default other permission for a dir.
        // initial defaults for non-dir entries (files, etc.)
        fDefaultUser     = fRead | fWrite,
                            ///< Default user permission for a file
        fDefaultGroup    = fRead,
                            ///< Default group permission for a file
        fDefaultOther    = fRead,
                            ///< Default other permission for a file
        fDefault = 8        ///< Special flag:  ignore all other flags,
                            ///< use current default mode
    };
    typedef unsigned int TMode;  ///< Binary OR of "EMode"

    enum ESpecialModeBits {
        fSticky = 1,
        fSetGID = 2,
        fSetUID = 4
    };
    typedef unsigned int TSpecialModeBits; ///< Bitwise OR of ESpecialModeBits

    /// Get permission mode(s) of a directory entry.
    ///
    /// On WINDOWS: There is only the "user_mode" permission setting,
    /// and "group_mode" and "other_mode" settings will be ignored.
    /// The "user_mode" will be combined with effective permissions for
    /// the current process owner on specified directory entry.
    /// @return
    ///   TRUE upon successful retrieval of permission mode(s),
    ///   FALSE otherwise.
    /// @sa
    ///   SetMode
    bool GetMode(TMode*            user_mode,
                 TMode*            group_mode   = 0,
                 TMode*            other_mode   = 0,
                 TSpecialModeBits* special_bits = 0) const;

    /// Set permission mode(s) of a directory entry.
    ///
    /// Permissions are set as specified by the passed values for user_mode,
    /// group_mode and other_mode. Passing "fDefault" will cause the
    /// corresponding mode to be taken and set from its default setting.
    /// @return
    ///   TRUE if permission successfully set;  FALSE, otherwise.
    /// @sa
    ///   SetDefaultMode, SetDefaultModeGlobal, GetMode
    bool SetMode(TMode            user_mode,  // e.g. fDefault
                 TMode            group_mode   = fDefault,
                 TMode            other_mode   = fDefault,
                 TSpecialModeBits special_bits = 0) const;

    /// Set default permission modes globally for all CDirEntry objects.
    ///
    /// The default mode is set globally for all CDirEntry objects except for
    /// those having their own mode set with individual SetDefaultMode().
    ///
    /// When "fDefault" is passed as a value of the mode parameters,
    /// the default mode takes places for the value as defined in EType:
    ///
    /// If user_mode is "fDefault", then the default is to take
    /// - "fDefaultDirUser" if this is a directory, or
    /// - "fDefaultUser" if this is a file.
    /// 
    /// If group_mode is "fDefault", then the default is to take
    /// - "fDefaultDirGroup" if this is a directory, or
    /// - "fDefaultGroup" if this is a file.
    ///
    /// If other_mode is "fDefault", then the default is to take
    /// - "fDefaultDirOther" if this is a directory, or
    /// - "fDefaultOther" if this is a file.
    static void SetDefaultModeGlobal(EType entry_type,
                                     TMode user_mode,  // e.g. fDefault
                                     TMode group_mode = fDefault,
                                     TMode other_mode = fDefault);

    /// Set default mode(s) for this directory entry only.
    ///
    /// When "fDefault" is passed as a value of the parameters,
    /// the corresponding mode will be taken and set from the global mode
    /// as specified by SetDefaultModeGlobal().
    virtual void SetDefaultMode(EType entry_type,
                                TMode user_mode,  // e.g. fDefault
                                TMode group_mode = fDefault,
                                TMode other_mode = fDefault);

protected:
    /// Get the default global mode.
    ///
    /// For use in derived classes like CDir and CFile.
    static void GetDefaultModeGlobal(EType  entry_type,
                                     TMode* user_mode,
                                     TMode* group_mode,
                                     TMode* other_mode);

    /// Get the default mode.
    ///
    /// For use by derived classes like CDir and CFile.
    void GetDefaultMode(TMode* user_mode,
                        TMode* group_mode,
                        TMode* other_mode) const;

private:
    string        m_Path;    ///< Full path of this directory entry

    /// Which default mode: user, group, or other.
    ///
    /// Used as an index into an array that contains default mode values;
    /// so there is no "fDefault" as an enumeration value for EWho here!
    enum EWho {
        eUser = 0,           ///< User mode
        eGroup,              ///< Group mode
        eOther               ///< Other mode
    };

    /// Holds default mode values
    TMode         m_DefaultMode[3/*EWho*/];

    /// Holds default mode global values, per entry type
    static TMode  m_DefaultModeGlobal[eUnknown][3/*EWho*/];

    /// Backup suffix
    static string m_BackupSuffix;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFile --
///
/// Define class to work with files.
///
/// Models a file in a file system. Basic functionality is derived from
/// CDirEntry and extended for files.

class NCBI_XNCBI_EXPORT CFile : public CDirEntry
{
    typedef CDirEntry CParent;    ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CFile(void);

    /// Constructor using specified path string.
    CFile(const string& file);

    /// Copy constructor.
    CFile(const CDirEntry& file);

    /// Destructor.
    virtual ~CFile(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eFile; };

    /// Check existence of file.
    virtual bool Exists(void) const;

    /// Get size of file.
    ///
    /// @return
    /// - size of file, if no error.
    /// - -1, if there was an error obtaining file size.
    Int8 GetLength(void) const;

    /// Copy a file.
    ///
    /// @param new_path
    ///    Target entry path/name.
    /// @param flags
    ///   Flags specified how to copy entry.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   TCopyFlags
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0);

    /// Compare files by content.
    ///
    /// @param file
    ///   File name to compare.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if files content is equal; FALSE otherwise.
    bool Compare(const string& file, size_t buf_size = 0);


    //
    // Temporary files
    //

    /// Temporary file creation mode
    enum ETmpFileCreationMode {
        eTmpFileCreate,     ///< Create empty file for each GetTmpName* call.
        eTmpFileGetName     ///< Get name of the file only.
    };

    /// Get temporary file name.
    ///
    /// @param mode
    ///   Temporary file creation mode.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpNameEx, ETmpFileCreationMode
    static string GetTmpName(ETmpFileCreationMode mode = eTmpFileGetName);

    /// Get temporary file name.
    ///
    /// @param dir
    ///   Directory in which temporary file is to be created;
    ///   default of kEmptyStr means that system temporary directory will
    ///   be used or current, if a system temporary directory cannot
    ///   be determined.
    /// @param prefix
    ///   Temporary file name will be prefixed by value of this parameter;
    ///   default of kEmptyStr means that, effectively, no prefix value will
    ///   be used.
    /// @param mode
    ///   Temporary file creation mode. 
    ///   If set to "eTmpFileCreate", empty file with unique name will be
    ///   created. Please, do not forget to remove it yourself as soon as it
    ///   is no longer needed. On some platforms "eTmpFileCreate" mode is 
    ///   equal to "eTmpFileGetName".
    ///   If set to "eTmpFileGetName", returns only the name of the temporary
    ///   file, without creating it. This method is faster but it have
    ///   potential race condition, when other process can leave as behind and
    ///   create file with the same name first.
    /// @return
    ///   Name of temporary file, or "kEmptyStr" if there was an error
    ///   getting temporary file name.
    /// @sa
    ///    GetTmpName, ETmpFileCreationMode
    static string GetTmpNameEx(const string&        dir    = kEmptyStr,
                               const string&        prefix = kEmptyStr,
                               ETmpFileCreationMode mode   = eTmpFileGetName);


    /// What type of temporary file to create.
    enum ETextBinary {
        eText,          ///< Create text file
        eBinary         ///< Create binary file
    };

    /// Which operations to allow on temporary file.
    enum EAllowRead {
        eAllowRead,     ///< Allow read and write
        eWriteOnly      ///< Allow write only
    };

    /// Create temporary file and return pointer to corresponding stream.
    ///
    /// The temporary file will be automatically deleted after the stream
    /// object is deleted. If the file exists before the function call, then
    /// after the function call it will be removed. Also any previous contents
    /// of the file will be overwritten.
    /// @param filename
    ///   Use this value as name of temporary file. If "kEmptyStr" is passed
    ///   generate a temporary file name.
    /// @param text_binary
    ///   Specifies if temporary filename should be text ("eText") or binary
    ///   ("eBinary").
    /// @param allow_read
    ///   If set to "eAllowRead", read and write are permitted on temporary
    ///   file. If set to "eWriteOnly", only write is permitted on temporary
    ///   file.
    /// @return
    ///   - Pointer to corresponding stream, or
    ///   - NULL if error encountered.
    /// @sa
    ///   CreateTmpFileEx
    static fstream* CreateTmpFile(const string& filename    = kEmptyStr,
                                  ETextBinary   text_binary = eBinary,
                                  EAllowRead    allow_read  = eAllowRead);

    /// Create temporary file and return pointer to corresponding stream.
    ///
    /// Similar to CreateTmpEx() except that you can also specify the directory
    /// in which to create the temporary file and the prefix string to be used
    /// for creating the temporary file.
    ///
    /// The temporary file will be automatically deleted after the stream
    /// object is deleted. If the file exists before the function call, then
    /// after the function call it will be removed. Also any previous contents
    /// of the file will be overwritten.
    /// @param dir
    ///   The directory in which the temporary file is to be created. If not
    ///   specified, the temporary file will be created in the current 
    ///   directory.
    /// @param prefix
    ///   Use this value as the prefix for temporary file name. If "kEmptyStr"
    ///   is passed generate a temporary file name.
    /// @param text_binary
    ///   Specifies if temporary filename should be text ("eText") or binary
    ///   ("eBinary").
    /// @param allow_read
    ///   If set to "eAllowRead", read and write are permitted on temporary
    ///   file. If set to "eWriteOnly", only write is permitted on temporary
    ///   file.
    /// @return
    ///   - Pointer to corresponding stream, or
    ///   - NULL if error encountered.
    /// @sa
    ///   CreateTmpFile
    static fstream* CreateTmpFileEx(const string& dir         = ".",
                                    const string& prefix      = kEmptyStr,
                                    ETextBinary   text_binary = eBinary,
                                    EAllowRead    allow_read  = eAllowRead);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CDir --
///
/// Define class to work with directories.
///
/// NOTE: The following functions are unsafe in multithreaded applications:
///       - static bool Exists() (for Mac only);
///       - bool Exists() (for Mac only).

class NCBI_XNCBI_EXPORT CDir : public CDirEntry
{
    typedef CDirEntry CParent;  ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CDir(void);

    /// Constructor using specified directory name.
    CDir(const string& dirname);

    /// Copy constructor.
    CDir(const CDirEntry& dir);

    /// Destructor.
    virtual ~CDir(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eDir; };

    /// Check if directory "dirname" exists.
    virtual bool Exists(void) const;

    /// Get user "home" directory.
    static string GetHome(void);

    /// Get temporary directory.
    static string GetTmpDir(void);

    /// Get the current working directory.
    static string GetCwd(void);

    /// Define a list of pointers to directory entries.
    typedef list< AutoPtr<CDirEntry> > TEntries;

    /// Flags for GetEntries()
    /// @sa GetEntries, GetEntriesPtr
    enum EGetEntriesFlags {
        fIgnoreRecursive = (1<<1), ///< Supress self recursive elements ("..")
        fCreateObjects   = (1<<2), ///< Get objects accordingly to entry type
                                   ///< (CFile,CDir,...), not just a CDirEntry
        fNoCase          = (1<<3), ///< Use case insensitive compare by mask
        // These flags added for backward compatibility and will be removed
        // in the future, so don't use it.
        eAllEntries       = 0,
        eIgnoreRecursive  = fIgnoreRecursive

    };
    typedef int TGetEntriesFlags; ///< Binary OR of "EGetEntriesFlags"

    /// Get directory entries based on the specified "mask".
    ///
    /// @param mask
    ///   Use to select only entries that match this mask.
    ///   Do not use file mask if set to "kEmptyStr".
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const string&    mask  = kEmptyStr,
                        TGetEntriesFlags flags = 0) const;

    /// Get directory entries based on the specified set of "masks".
    ///
    /// @param mask
    ///   Use to select only entries that match this set of masks.
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const vector<string>& masks,
                        TGetEntriesFlags flags = 0) const;

    /// Get directory entries based on the specified set of "masks".
    ///
    /// @param mask
    ///   Use to select only entries that match this set of masks.
    /// @param mode
    ///   Defines which entries return.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   An array containing all directory entries.
    TEntries GetEntries(const CMask&     masks, 
                        TGetEntriesFlags flags = 0) const;


    /// Versions of GetEntries() which returns pointer to TEntries.
    /// This methods are faster on big directories than GetEntries().
    /// NOTE: Do not forget to release allocated memory using return pointer.
    TEntries* GetEntriesPtr(const string&    mask  = kEmptyStr,
                            TGetEntriesFlags flags = 0) const;

    TEntries* GetEntriesPtr(const vector<string>& masks,
                            TGetEntriesFlags flags = 0) const;

    TEntries* GetEntriesPtr(const CMask&     masks,
                            TGetEntriesFlags flags = 0) const;

    // OBSOLETE functions. Will be deleted soon.
    // Please use versions of GetEntries*() listed above.
    typedef TGetEntriesFlags EGetEntriesMode;
    TEntries GetEntries    (const string&    mask,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries GetEntries    (const vector<string>& masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries GetEntries    (const CMask&     masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const string&    mask,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const vector<string>& masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;
    TEntries* GetEntriesPtr(const CMask&     masks,
                            EGetEntriesMode  mode,
                            NStr::ECase      use_case) const;

    /// Create the directory using "dirname" passed in the constructor.
    /// 
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    bool Create(void) const;

    /// Create the directory path recursively possibly more than one at a time.
    ///
    /// @return
    ///   TRUE if operation successful; FALSE otherwise.
    bool CreatePath(void) const;

    /// Copy directory.
    ///
    /// @param new_path
    ///   New path/name for entry.
    /// @param flags
    ///   Flags specified how to copy directory.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   CDirEntry::TCopyFlags, CDirEntry::Copy, CFile::Copy
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0);

    /// Delete existing directory.
    ///
    /// @param mode
    ///   - If set to "eOnlyEmpty" the directory can be removed only if it
    ///   is empty.
    ///   - If set to "eNonRecursive" remove only files in directory, but do
    ///   not remove subdirectories and files in them.
    ///   - If set to "eRecursive" remove all files in directory, and
    ///   subdirectories.
    /// @return
    ///   TRUE if operation successful; FALSE otherwise.
    /// @sa
    ///   EDirRemoveMode
    virtual bool Remove(EDirRemoveMode mode = eRecursive) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CSymLink --
///
/// Define class to work with symbolic links.
///
/// Models the files in a file system. Basic functionality is derived from
/// CDirEntry and extended for files.

class NCBI_XNCBI_EXPORT CSymLink : public CDirEntry
{
    typedef CDirEntry CParent;    ///< CDirEntry is the parent class

public:
    /// Default constructor.
    CSymLink(void);

    /// Constructor using specified path string.
    CSymLink(const string& link);

    /// Copy constructor.
    CSymLink(const CDirEntry& link);

    /// Destructor.
    virtual ~CSymLink(void);

    /// Get a type of constructed object.
    virtual EType GetObjectType(void) const { return eLink; };

    /// Check existence of link.
    virtual bool Exists(void) const;

    /// Create symbolic link.
    ///
    /// @param path
    ///   Path to some entry that link will be ponted to.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    ///   Return FALSE also if link already exists.
    bool Create(const string& path);

    /// Copy link.
    ///
    /// @param new_path
    ///   Target entry path/name.
    /// @param flags
    ///   Flags specified how to copy entry.
    /// @param buf_size
    ///   Size of buffer to read file.
    ///   Zero value means using default buffer size.
    /// @return
    ///   TRUE if operation successful; FALSE, otherwise.
    /// @sa
    ///   CDirEntry::TCopyFlags, CDirEntry::Copy, Create
    virtual bool Copy(const string& new_path, TCopyFlags flags = fCF_Default,
                      size_t buf_size = 0);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFileUtil --
///
/// Utility functions.
/// 
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CFileUtil
{
public:

    ///   Unix:
    ///       The path name to any file/dir withing filesystem.
    ///   MS Windows:
    ///       The root directory of the disk, or UNC name. It must include
    ///       a trailing backslash (for example, \\MyServer\MyShare\, C:\).
    ///   The "." can be used to get disk space on current disk.
    ///   GetDiskInformation

    /// Get disk space information.
    ///
    /// Get information for the user associated with the calling thread only.
    /// If per-user quotas are in use, that some returned values may be less
    /// than the total number of free/total bytes on the disk.
    /// @param path
    ///   String that specifies filesystem for which information
    ///   is to be returned. 
    /// @return
    ///   The amount of free space.
    /// @sa 
    ///   GetDiskInformation
    static Uint8 GetFreeDiskSpace(const string& path);

    /// Get disk space information.
    ///
    /// Get information for the user associated with the calling thread only.
    /// If per-user quotas are in use, that some returned values may be less
    /// than the total number of free/total bytes on the disk.
    /// @param path
    ///   String that specifies filesystem for which information
    ///   is to be returned. 
    /// @return
    ///   The amount of free space.
    /// @sa 
    ///   GetDiskInformation
    static Uint8 GetTotalDiskSpace(const string& path);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFileDeleteList --
///
/// Define a list of dir entries for deletion.
///
/// Each Object of this class maintains a list of names of dir entries
/// that will be deleted from the file system when the object
/// goes out of scope.
///
/// Note: Directories will be removed recursively, symbolic links -- 
/// without dir entries which they points to.

class NCBI_XNCBI_EXPORT CFileDeleteList : public CObject
{
public:
    /// Destructor removes all dir entries on list.
    ~CFileDeleteList();

    typedef list<string> TNames;

    /// Add a dir entry for later deletion.
    void Add(const string& entryname);
    /// Get the underlying list.
    const TNames& GetNames() const;
    /// Set the underlying list.
    void SetNames(TNames& names);

private:
    TNames  m_Names;   ///< List of dir entries for deletion
};


/////////////////////////////////////////////////////////////////////////////
///
/// CFileDeleteAtExit --
///
/// This class is used to mark dir entries for deletion at application exit.
/// @sa CFileDeleteList

class NCBI_XNCBI_EXPORT CFileDeleteAtExit
{
public:
    /// Add the name of a dir entry; it will be deleted on (normal) exit
    static void Add(const string& entryname);

    /// Get underlying static CFileDeleteList object
    static const CFileDeleteList& GetDeleteList();
    /// Set the underlying static CFileDeleteList object
    static void SetDeleteList(CFileDeleteList& list);
};


/////////////////////////////////////////////////////////////////////////////
//
//  Find files algorithms
//

/// File finding flags
enum EFindFiles {
    fFF_File       = (1<<0),             ///< find files
    fFF_Dir        = (1<<1),             ///< find directories
    fFF_Recursive  = (1<<2),             ///< descend into sub-dirs
    fFF_Nocase     = (1<<3),             ///< case-insensitive names search
    fFF_Default    = fFF_File | fFF_Dir  ///< default behaviur
};
/// Bitwise OR of "EFindFiles"
typedef int TFindFiles; 


/// Find files in the specified directory
template<class TFindFunc>
TFindFunc FindFilesInDir(const CDir&            dir,
                         const vector<string>&  masks,
                         TFindFunc              find_func,
                         TFindFiles             flags = fFF_Default)
{
    CDir::TGetEntriesFlags ge_flags = CDir::fIgnoreRecursive;
    if (flags & fFF_Nocase) {
        ge_flags |= CDir::fNoCase;
    }
    CDir::TEntries contents = dir.GetEntries(masks, ge_flags);

    ITERATE(CDir::TEntries, it, contents) {
        const CDirEntry& dir_entry = **it;

        if (dir_entry.IsDir()) {
            if (flags & fFF_Dir) {
                find_func(dir_entry);
            }
            if (flags & fFF_Recursive) {
                CDir nested_dir(dir_entry.GetPath());
                find_func = 
                  FindFilesInDir(nested_dir, masks, find_func, flags);
            }
        }
        else if (dir_entry.IsFile() && (flags & fFF_File)) {
            find_func(dir_entry);
        }
    } // ITERATE
    return find_func;
}


/// Find files in the specified directory
template<class TFindFunc>
TFindFunc FindFilesInDir(const CDir&   dir,
                         const CMask&  masks,
                         TFindFunc     find_func,
                         TFindFiles    flags = fFF_Default)
{
    CDir::TGetEntriesFlags ge_flags = CDir::fIgnoreRecursive;
    if (flags & fFF_Nocase) {
        ge_flags |= CDir::fNoCase;
    }
    CDir::TEntries contents = dir.GetEntries(masks, ge_flags);

    ITERATE(CDir::TEntries, it, contents) {
        const CDirEntry& dir_entry = **it;

        if (dir_entry.IsDir()) {
            if (flags & fFF_Dir) {
                find_func(dir_entry);
            }
            if (flags & fFF_Recursive) {
                CDir nested_dir(dir_entry.GetPath());
                find_func = 
                    FindFilesInDir(nested_dir, masks, find_func, flags);
            }
        }
        else if (dir_entry.IsFile() && (flags & fFF_File)) {
            find_func(dir_entry);
        }
    } // ITERATE
    return find_func;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries
/// Functor call should match: void Functor(const CDirEntry& dir_entry)
///

template<class TPathIterator, 
         class TMaskIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator path_begin,
                    TPathIterator path_end,
                    TMaskIterator mask_begin,
                    TMaskIterator mask_end,
                    TFindFunc     find_func,
                    TFindFiles    flags = fFF_Default)
{
    vector<string> masks;
    for (; mask_begin != mask_end; ++mask_begin) {
        masks.push_back(*mask_begin);
    }

    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, find_func, flags);
    }
    return find_func;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Generic algorithm for file search
///
/// Algorithm scans the provided directories using iterators,
/// finds files to match the masks and stores all calls functor
/// object for all found entries
/// Functor call should match: void Functor(const CDirEntry& dir_entry)
///

template<class TPathIterator, 
         class TFindFunc>
TFindFunc FindFiles(TPathIterator path_begin,
                    TPathIterator path_end,
                    const CMask&  masks,
                    TFindFunc     find_func,
                    TFindFiles    flags = fFF_Default)
{
    for (; path_begin != path_end; ++path_begin) {
        const string& dir_name = *path_begin;
        CDir dir(dir_name);
        find_func = FindFilesInDir(dir, masks, find_func, flags);
    }
    return find_func;
}


/// Functor for generic FindFiles, adds file name to the specified container
template<class TNames>
class CFindFileNamesFunc
{
public:
    CFindFileNamesFunc(TNames& names) : m_FileNames(&names) {}

    void operator()(const CDirEntry& dir_entry)
    {
        m_FileNames->push_back(dir_entry.GetPath());
    }
protected:
    TNames*  m_FileNames;
};


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator>
void FindFiles(TContainer&           out, 
               TPathIterator         first_path, 
               TPathIterator         last_path, 
               const vector<string>& masks,
               TFindFiles            flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, 
              masks.begin(), masks.end(), func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator>
void FindFiles(TContainer&    out, 
               TPathIterator  first_path, 
               TPathIterator  last_path, 
               const CMask&   masks,
               TFindFiles     flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, masks, func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Utility algorithm scans the provided directories using iterators
/// finds files to match the masks and stores all found files in 
/// the container object.
///

template<class TContainer, class TPathIterator, class TMaskIterator>
void FindFiles(TContainer&    out, 
               TPathIterator  first_path,
               TPathIterator  last_path, 
               TMaskIterator  first_mask,
               TMaskIterator  last_mask,
               TFindFiles     flags = fFF_Default)
{
    CFindFileNamesFunc<TContainer> func(out);
    FindFiles(first_path, last_path, 
              first_mask, last_mask, func, flags);
}


/////////////////////////////////////////////////////////////////////////////
///
/// fwd-decl of struct containing OS-specific mem.-file handle.

struct SMemoryFileHandle;
struct SMemoryFileAttrs;


/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFile_Base --
///
/// Define base class for support file memory mapping.

class NCBI_XNCBI_EXPORT CMemoryFile_Base
{
public:
    /// Which operations are permitted in memory map file.
    typedef enum {
        eMMP_Read,        ///< Data can be read
        eMMP_Write,       ///< Data can be written
        eMMP_ReadWrite    ///< Data can be read and written
    } EMemMapProtect;

    /// Whether to share changes or not.
    typedef enum {
        eMMS_Shared,      ///< Changes are shared
        eMMS_Private      ///< Changes are private (write do not change file)
    } EMemMapShare;

    /// Constructor.
    ///
    CMemoryFile_Base(void);


    /// Check if memory-mapping is supported by the C++ Toolkit on this
    /// platform.
    static bool IsSupported(void);

    /// What type of data access pattern will be used for mapped region.
    ///
    /// Advises the VM system that the a certain region of user mapped memory 
    /// will be accessed following a type of pattern. The VM system uses this 
    /// information to optimize work with mapped memory.
    ///
    /// NOTE: Now works on UNIX platform only.
    typedef enum {
        eMMA_Normal,      ///< No further special treatment
        eMMA_Random,      ///< Expect random page references
        eMMA_Sequential,  ///< Expect sequential page references
        eMMA_WillNeed,    ///< Will need these pages
        eMMA_DontNeed     ///< Don't need these pages
    } EMemMapAdvise;


    /// Advise on memory map usage for specified region.
    ///
    /// @param addr
    ///   Address of memory region whose usage is being advised.
    /// @param len
    ///   Length of memory region whose usage is being advised.
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///   TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdvise
    static bool MemMapAdviseAddr(void* addr, size_t len,EMemMapAdvise advise);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFileSegment --
///
/// Define auxiliary class for mapping a memory file region of the file
/// into the address space of the calling process. 
/// 
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFileSegment : public CMemoryFile_Base
{
public:
    /// Constructor.
    ///
    /// Maps a view of the file, represented by "handle", into the address
    /// space of the calling process. 
    /// @param handle
    ///   Handle to view of the mapped file. 
    /// @param attr
    ///   Specify operations permitted on memory mapped region.
    /// @param offset
    ///   The file offset where mapping is to begin.
    ///   Cannot accept values less than 0.
    /// @param length
    ///   Number of bytes to map. The parameter value should be more than 0.
    /// @sa
    ///   EMemMapProtect, EMemMapShare, GetPtr, GetSize, GetOffset
    CMemoryFileSegment(SMemoryFileHandle& handle,
                       SMemoryFileAttrs&  attrs,
                       off_t              offset,
                       size_t             lendth);

    /// Destructor.
    ///
    /// Unmaps a mapped area of the file.
    ~CMemoryFileSegment(void);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    void* GetPtr(void) const;

    /// Get offset of the mapped area from beginning of file.
    ///
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    ///   Always return value passed in constructor even if data
    ///   was not successfully mapped.
    off_t GetOffset(void) const;

    /// Get length of the mapped area.
    ///
    /// @return
    ///   - Length in bytes of the mapped area, or
    ///   - 0 if not mapped.
    size_t GetSize(void) const;

    /// Get pointer to beginning of really mapped data.
    ///
    /// When the mapping object is creating and the offset is not a multiple
    /// of the allocation granularity, that offset and length can be adjusted
    /// to match it. The "length" value will be automatically increased on the
    /// difference between passed and real offsets.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///    GetRealOffset, GetRealSize, GetPtr 
    void* GetRealPtr(void) const;

    /// Get real offset of the mapped area from beginning of file.
    ///
    /// This real offset is adjusted to system's memory allocation granularity
    /// value and can be less than requested "offset" in the constructor.
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    ///   Always return adjusted value even if data was not successfully mapped.
    /// @sa
    ///    GetRealPtr, GetRealSize, GetOffset 
    off_t GetRealOffset(void) const;

    /// Get real length of the mapped area.
    ///
    /// Number of really mapped bytes of file. This length value can be
    /// increased if "offset" is not a multiple of the allocation granularity.
    /// @return
    ///   - Length in bytes of the mapped area, or
    ///   - 0 if not mapped.
    /// @sa
    ///    GetRealPtr, GetRealOffset, GetSize 
    size_t GetRealSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if not mapped or if an error occurs.
    bool Flush(void) const;

    /// Unmap file view from memory.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///     TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw excepton otherwise.
    void x_Verify(void) const;

private:
    // Values for user
    void*   m_DataPtr;     ///< Pointer to the beginning of the mapped area.
                           ///> The user seen this one.
    off_t   m_Offset;      ///< Requested starting offset of the
                           ///< mapped area from beginning of file.
    size_t  m_Length;      ///< Requested length of the mapped area.

    // Internal real values
    void*   m_DataPtrReal; ///< Real pointer to the beginning of the mapped
                           ///< area which should be fried later.
    off_t   m_OffsetReal;  ///< Corrected starting offset of the
                           ///< mapped area from beginning of file.
    size_t  m_LengthReal;  ///< Corrected length of the mapped area.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFileMap --
///
/// Define class for support a partial file memory mapping.
///
/// Note, that all mapped into memory file segments have equal protect and
/// share attributes, because they restricted with file open mode.
/// Note, that the mapping file must exists and have non-zero length.
/// This class cannot increase size of mapped file. If the size of
/// the mapped file changes, the effect of references to portions of
/// the mapped region that correspond to added or removed portions of the
/// file is unspecified.
///
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFileMap : public CMemoryFile_Base
{
public:
    /// Constructor.
    ///
    /// Initialize the memory mapping on file "file_name".
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    CMemoryFileMap(const string&  file_name,
                   EMemMapProtect protect_attr = eMMP_Read,
                   EMemMapShare   share_attr   = eMMS_Shared);

    /// Destructor.
    ///
    /// Calls Unmap() and cleans up memory.
    ~CMemoryFileMap(void);

    /// Map file segment if it is mapped.
    ///
    /// @param offset
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automatically increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetOffset(). The parameter must be more than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using GetSize().
    ///   The value 0 means that all file size will be mapped.
    /// @return
    ///   - Pointer to start of data, or
    ///   - NULL if mapped to a file of zero length, or if not mapped.
    /// @sa
    ///   Unmap
    void* Map(off_t offset, size_t length);

    /// Unmap file segment.
    ///
    /// @param ptr
    ///   Pointer returned by Map().
    /// @return
    ///   TRUE on success; or FALSE on error.
    /// @sa
    ///   Map
    bool Unmap(void* ptr);

    /// Unmap all mapped segment.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    ///   In case of error some segments can be not unmapped.
    bool UnmapAll(void);

    /// Get offset of the mapped segment from beginning of the file.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by Map().
    /// @return
    ///   Offset in bytes of mapped segment from beginning of the file.
    ///   Returned value is a value of "offset" parameter passed
    ///   to MapSegment() method for specified "ptr".
    off_t GetOffset(void* ptr) const;

    /// Get length of the mapped segment.
    ///
    /// @param prt
    ///   Pointer to mapped data returned by MapSegment().
    /// @return
    ///   Length in bytes of the mapped area.
    ///   Returned value is a value of "length" parameter passed
    ///   to MapSegment() method for specified "ptr".
    size_t GetSize(void* ptr) const;

    /// Get length of the mapped file.
    ///
    /// @return
    ///   Size in bytes of the mapped file.
    Int8 GetFileSize(void) const;

    /// Flush memory mapped file segment.
    ///
    /// Flush specified mapped segment by writing all modified copies of
    /// memory pages to the underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void* ptr) const;

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///     TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(void* ptr, EMemMapAdvise advise) const;

private:
    // Open file mapping for file with name m_FileName.
    void x_Open(void);

    // Unmap mapped memory and close all handles.
    void x_Close(void);

    // Get segment by pointer to data.
    CMemoryFileSegment* x_Get(void* ptr) const;

protected:
    string              m_FileName;  ///< File name. 
    SMemoryFileHandle*  m_Handle;    ///< Memory file handle.
    SMemoryFileAttrs*   m_Attrs;     ///< Specify operations permitted on
                                     ///< memory mapped file and mapping mode.

    typedef map<void*,CMemoryFileSegment*> TSegments;
    TSegments           m_Segments;  ///< Map of pointers to mapped segments.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryFile --
///
/// Define class for support file memory mapping.
///
/// This is a simple version of the CMemoryFileMap class, supported one
/// mapped segment only.
///
/// Note, that the mapping file must exists and have non-zero length.
/// This class cannot increase size of mapped file. If the size of
/// the mapped file changes, the effect of references to portions of
/// the mapped region that correspond to added or removed portions of the
/// file is unspecified.
///
/// Throws an exceptions on error.

class NCBI_XNCBI_EXPORT CMemoryFile : public CMemoryFileMap
{
public:
    /// Constructor.
    ///
    /// Initialize the memory mapping on file "file_name".
    /// @param filename
    ///   Name of file to map to memory.
    /// @param protect_attr
    ///   Specify operations permitted on memory mapped file.
    /// @param share_attr
    ///   Specify if change to memory mapped file can be shared or not.
    /// @param offset
    ///   The file offset where mapping is to begin. If the offset is not
    ///   a multiple of the allocation granularity, that it can be decreased 
    ///   to match it. The "length" value will be automatically increased on
    ///   the difference between passed and real offsets. The real offset can
    ///   be obtained using GetOffset(). The parameter must be more than 0.
    /// @param length
    ///   Number of bytes to map. This value can be increased if "offset"
    ///   is not a multiple of the allocation granularity.
    ///   The real length of mapped region can be obtained using GetSize().
    ///   The value 0 means that all file size will be mapped.
    /// @sa
    ///   EMemMapProtect, EMemMapShare
    CMemoryFile(const string&  file_name,
                EMemMapProtect protect_attr = eMMP_Read,
                EMemMapShare   share_attr   = eMMS_Shared,
                off_t          offset       = 0,
                size_t         lendth       = 0);

    /// Unmap file if mapped.
    ///
    /// @return
    ///   TRUE on success; or FALSE on error.
    bool Unmap(void);

    /// Get pointer to beginning of data.
    ///
    /// @return
    ///   Pointer to start of data.
    void* GetPtr(void) const;

    /// Get offset of the mapped area from beginning of the file.
    ///
    /// The offset can be adjusted to system's memory allocation granularity
    /// value and can differ from "offset" parameter in the class constructor
    /// or in the Map() method.
    /// @return
    ///   Offset in bytes of mapped area from beginning of the file.
    off_t GetOffset(void) const;

    /// Get length of the mapped region.
    ///
    /// @return
    ///   - Length in bytes of the mapped region, or
    ///   - 0 if not mapped or file is empty.
    size_t GetSize(void) const;

    /// Flush by writing all modified copies of memory pages to the
    /// underlying file.
    ///
    /// NOTE: By default data will be flushed in the destructor.
    /// @return
    ///   - TRUE, if all data was flushed successfully.
    ///   - FALSE, if an error occurs.
    bool Flush(void) const;

    /// Advise on memory map usage.
    ///
    /// @param advise
    ///   One of the values in EMemMapAdvise that advises on expected
    ///   usage pattern.
    /// @return
    ///   - TRUE, if memory advise operation successful. Always return
    ///   TRUE if memory advise not implemented such as on Windows system.
    ///   - FALSE, if memory advise operation not successful.
    /// @sa
    ///   EMemMapAdvise, MemMapAdviseAddr
    bool MemMapAdvise(EMemMapAdvise advise) const;

private:
    // Check that file is mapped, throw excepton otherwise.
    void x_Verify(void) const;

private:
    void* m_Ptr;  ///< Pointer to mapped view of file.
};


/* @} */



//////////////////////////////////////////////////////////////////////////////
//
// Inline
//


// CDirEntry

inline
CDirEntry::CDirEntry(void)
{
    return;
}

inline
const string& CDirEntry::GetPath(void) const
{
    return m_Path;
}

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
bool CDirEntry::IsFile(EFollowLinks follow) const
{
    return GetType(follow) == eFile;
}

inline
bool CDirEntry::IsDir(EFollowLinks follow) const
{
    return GetType(follow) == eDir;
}

inline
bool CDirEntry::IsLink(void) const
{
    return GetType(eIgnoreLinks) == eLink;
}

inline
bool CDirEntry::Exists(void) const
{
    return GetType() != eUnknown;
}

inline
bool CDirEntry::MatchesMask(const char* name, const char* mask,
                            NStr::ECase use_case)
{
    return NStr::MatchesMask(name, mask, use_case);
}

inline
bool CDirEntry::MatchesMask(const char* name, const CMask& mask,
                            NStr::ECase use_case) 
{
    return mask.Match(name, use_case);
}

inline 
bool CDirEntry::CopyToDir(const string& dir, TCopyFlags flags,
                          size_t buf_size)
{
    string path = MakePath(dir, GetName());
    return Copy(path, flags, buf_size);
}

inline
const string& CDirEntry::GetBackupSuffix(void)
{
    return m_BackupSuffix;
}

inline
void CDirEntry::SetBackupSuffix(const string& suffix)
{
    m_BackupSuffix = suffix;
}


// CFile

inline
CFile::CFile(void)
{
    return;
}


inline
CFile::CFile(const string& filename) : CParent(filename)
{ 
    SetDefaultMode(eFile, fDefault, fDefault, fDefault);
}

inline
CFile::CFile(const CDirEntry& file) : CParent(file)
{
    return;
}

inline
bool CFile::Exists(void) const
{
    return IsFile();
}


// CDir

inline
CDir::CDir(void)
{
    return;
}

inline
CDir::CDir(const string& dirname) : CParent(dirname)
{
    SetDefaultMode(eDir, fDefault, fDefault, fDefault);
}

inline
CDir::CDir(const CDirEntry& dir) : CDirEntry(dir)
{
    return;
}

inline
bool CDir::Exists(void) const
{
    return IsDir();
}

inline CDir::TEntries 
CDir::GetEntries(const string&    mask,
                 EGetEntriesMode  mode,
                 NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(mask, mode);
}

inline CDir::TEntries
CDir::GetEntries(const vector<string>& masks,
                 EGetEntriesMode  mode,
                 NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(masks, mode);
}

inline CDir::TEntries
CDir::GetEntries(const CMask&     masks,
                 EGetEntriesMode  mode,
                 NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntries(masks, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const string&    mask,
                    EGetEntriesMode  mode,
                    NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(mask, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const vector<string>& masks,
                    EGetEntriesMode  mode,
                    NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(masks, mode);
}

inline CDir::TEntries*
CDir::GetEntriesPtr(const CMask&     masks,
                    EGetEntriesMode  mode,
                    NStr::ECase      use_case) const
{
    if (use_case == NStr::eNocase) mode |= fNoCase;
    return GetEntriesPtr(masks, mode);
}


// CSymLink

inline
CSymLink::CSymLink(void)
{
    return;
}

inline
CSymLink::CSymLink(const string& link) : CParent(link)
{ 
    // We dont need SetDefaultMode() here
    return;
}

inline
CSymLink::CSymLink(const CDirEntry& link) : CDirEntry(link)
{
    return;
}

inline
bool CSymLink::Exists(void) const
{
    return IsLink();
}


// CFileDelete*

inline
void CFileDeleteList::Add(const string& entryname)
{
    m_Names.push_back(entryname);
}

inline
const CFileDeleteList::TNames& CFileDeleteList::GetNames(void) const
{
    return m_Names;
}

inline
void CFileDeleteList::SetNames(CFileDeleteList::TNames& names)
{
    m_Names = names;
}



// CMemoryFileSegment

inline
void* CMemoryFileSegment::GetPtr(void) const
{
    return m_DataPtr;
}

inline
size_t CMemoryFileSegment::GetSize(void) const
{
    return m_Length;
}


inline
off_t CMemoryFileSegment::GetOffset(void) const
{
    return m_Offset;
}

inline
void* CMemoryFileSegment::GetRealPtr(void) const
{
    return m_DataPtrReal;
}

inline
size_t CMemoryFileSegment::GetRealSize(void) const
{
    return m_LengthReal;
}


inline
off_t CMemoryFileSegment::GetRealOffset(void) const
{
    return m_OffsetReal;
}


// CMemoryFileMap


inline
off_t CMemoryFileMap::GetOffset(void* ptr) const
{
    return x_Get(ptr)->GetOffset();
}

inline
size_t CMemoryFileMap::GetSize(void* ptr) const
{
    return x_Get(ptr)->GetSize();
}

inline
Int8 CMemoryFileMap::GetFileSize(void) const
{
    return CFile(m_FileName).GetLength();
}

inline
bool CMemoryFileMap::Flush(void* ptr) const
{
    return x_Get(ptr)->Flush();
}

inline
bool CMemoryFileMap::MemMapAdvise(void* ptr, EMemMapAdvise advise) const
{
    return x_Get(ptr)->MemMapAdvise(advise);
}



// CMemoryFile

inline
void* CMemoryFile::GetPtr(void) const
{
    return m_Ptr;
}

inline
size_t CMemoryFile::GetSize(void) const
{
    // Special case: file is not mapped and its length is zero.
    if ( !m_Ptr  &&  GetFileSize() == 0) {
        return 0;
    }
    x_Verify();
    return CMemoryFileMap::GetSize(m_Ptr);
}

inline
off_t CMemoryFile::GetOffset(void) const
{
    x_Verify();
    return CMemoryFileMap::GetOffset(m_Ptr);
}

inline
bool CMemoryFile::Flush(void) const
{
    x_Verify();
    return CMemoryFileMap::Flush(m_Ptr);
}

inline
bool CMemoryFile::MemMapAdvise(EMemMapAdvise advise) const
{
    x_Verify();
    return CMemoryFileMap::MemMapAdvise(m_Ptr, advise);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.69  2005/12/08 14:12:15  ivanov
 * Minor comment fix for CDirEntry::GetMode()
 *
 * Revision 1.68  2005/11/30 12:01:21  ivanov
 * CFileUtil:: split GetDiskSpace() to GetFreeDiskSpace() and
 * GetTotalDiskSpace().
 *
 * Revision 1.67  2005/11/28 16:14:38  ivanov
 * + CFileUtil::GetDiskSpace
 *
 * Revision 1.66  2005/08/11 11:27:17  ivanov
 * CDirEntry::GetObjectType -- return "eUnknown" for CDirEntry
 *
 * Revision 1.65  2005/08/11 11:17:27  ivanov
 * Added method GetObjectType() to all CDirEntry based classes.
 * Made CDirEntry::GetType() static.
 * Added CDirEntry::EGetEntriesFlags type and 'flag' versions of
 * CDirEntry::GetEntries[Ptr](). Marked 'enum' versions as obsolete.
 *
 * Revision 1.64  2005/07/21 12:02:53  ivanov
 * Move CDirEntry::Copy() implementation to .cpp file.
 * Minor comment changes.
 *
 * Revision 1.63  2005/07/12 11:16:04  ivanov
 * CDirEntry::IsNewer() -- added additional argument which specify what
 * to do if the dir entry does not exist or is not accessible.
 *
 * Revision 1.62  2005/06/21 13:39:26  ivanov
 * CDir::TEntries: use list<> instead of vector<> as container class
 * + CDir::GetEntriesPtr()
 *
 * Revision 1.61  2005/06/14 13:05:02  ivanov
 * + CDirEntry::IsIdentical()
 * + CDirEntry::GetType(const struct stat&)
 *
 * Revision 1.60  2005/06/10 20:06:54  lavr
 * Special mode bits have been added
 *
 * Revision 1.59  2005/05/27 13:36:19  lavr
 * CDirEntry::Stat() to return bool (not int)
 * EDirRemoveMode::eTopDirOnly added
 *
 * Revision 1.58  2005/05/26 20:14:13  lavr
 * Brush inline documenting comments;
 * GetPath() and GetBackupSuffix() to return const string&
 *
 * Revision 1.57  2005/05/20 11:23:05  ivanov
 * Added new classes CFileDeleteList and CFileDeleteAtExit.
 * CMemoryFile[Map](): changed default share attribute from eMMS_Shared.
 *
 * Revision 1.56  2005/04/28 14:08:16  ivanov
 * Added time_t and CTime versions of CDirEntry::IsNewer()
 *
 * Revision 1.55  2005/04/12 19:06:39  ucko
 * Move EFollowLinks to ncbimisc.hpp.
 *
 * Revision 1.54  2005/04/12 11:25:09  ivanov
 * CDirEntry: added struct SStat and method Stat() to get additional non-posix
 * OS-dependent info. Now it can get only nanoseconds for entry times.
 * CDirEntry::SetTime[T]() -- added parameter to change creation time
 * (where possible). Minor comments changes and cosmetics.
 *
 * Revision 1.53  2005/03/23 15:37:13  ivanov
 * + CDirEntry:: CreateObject, Get/SetTimeT
 * Changed Copy/Rename in accordance that flags "Update" and "Backup"
 * also means "Overwrite".
 *
 * Revision 1.52  2005/03/22 14:20:48  ivanov
 * + CDirEntry:: operator=, Copy, CopyToDir, Get/SetBackupSuffix, Backup,
 *               IsLink, LookupLink, DereferenceLink, IsNewer, Get/SetOwner
 * + CFile:: Copy, Compare
 * + CDir:: Copy
 * + CSymLink
 * Added default constructors to all CDirEntry based classes.
 * CDirEntry::Rename -- added flags parameter.
 * CDir::GetEntries, CDirEntry::MatchesMask - added parameter for case
 * sensitive/insensitive matching.
 * Added additional flag for find files algorithms, EFindFiles += fFF_Nocase.
 * Dropped MacOS 9 support.
 *
 * Revision 1.51  2005/01/31 11:48:39  ivanov
 * Added CMask versions of:
 *     CDirEntries::MatchesMask(), CDirEntries::GetEntries()
 *     FindFiles(), FindFilesInDir().
 * Some cosmetics.
 *
 * Revision 1.50  2004/10/08 12:43:45  ivanov
 * Moved CDirEntry::Reset() to .cpp file
 *
 * Revision 1.49  2004/10/06 18:19:53  ivanov
 * Removed deleting trailing path separator from the CDirEntry::Reset(),
 * because in this case CDirEntry works incorrectly with root directories
 * like "/","\","E:\"
 *
 * Revision 1.48  2004/09/02 16:18:04  ivanov
 * CFileException:: added errcode sctring for eRelativePath
 *
 * Revision 1.47  2004/08/19 13:01:51  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.46  2004/08/03 12:04:57  ivanov
 * + CMemoryFile_Base   - base class for all memory mapping classes.
 * + CMemoryFileSegment - auxiliary class for mapping a memory file region.
 * + CMemoryFileMap     - class for support a partial file memory mapping.
 * * CMemoryFile        - now is the same as CMemoryFileMap but have only
 *                        one big mapped segment with offset 0 and length
 *                        equal to length of file.
 *
 * Revision 1.45  2004/07/29 18:20:40  ivanov
 * Added missed implementation for CMemoryFile::MemMapAdvise()
 *
 * Revision 1.44  2004/07/28 16:22:52  ivanov
 * Renamed CMemoryFileMap -> CMemoryFileSegment
 *
 * Revision 1.43  2004/07/28 15:47:17  ivanov
 * + CMemoryFile_Base, CMemoryFileMap.
 * Added "offset" and "length" parameters to CMemoryFile constructor to map
 * a part of file.
 *
 * Revision 1.42  2004/05/18 16:51:25  ivanov
 * Added CDir::GetTmpDir()
 *
 * Revision 1.41  2004/04/29 16:18:58  ivanov
 * operator== defined only for MAC OS
 *
 * Revision 1.40  2004/04/29 16:14:03  kuznets
 * Removed unnecessary typename
 *
 * Revision 1.39  2004/04/29 15:14:17  kuznets
 * + Generic FindFile algorithm capable of recursive searches
 * CDir::GetEntries received additional parameter to ignore self
 * recursive directory entries (".", "..")
 *
 * Revision 1.38  2004/04/28 19:04:16  ucko
 * Give GetType(), IsFile(), and IsDir() an optional EFollowLinks
 * parameter (currently only honored on Unix).
 *
 * Revision 1.37  2004/03/17 15:39:37  ivanov
 * CFile:: Fixed possible race condition concerned with temporary file name
 * generation. Added ETmpFileCreationMode enum. Fixed GetTmpName[Ex] and
 * CreateTmpFile[Ex] class methods.
 *
 * Revision 1.36  2004/03/11 22:16:52  vakatov
 * Cosmetics
 *
 * Revision 1.35  2004/01/05 21:41:55  gorelenk
 * += Exception throwing in CDirEntry::CreateRelativePath()
 *
 * Revision 1.34  2004/01/05 20:06:44  gorelenk
 * + CDirEntry::CreateRelativePath()
 *
 * Revision 1.33  2003/11/28 16:23:03  ivanov
 * + CDirEntry::SetTime()
 *
 * Revision 1.32  2003/11/05 16:27:18  kuznets
 * +FindFile template algorithm
 *
 * Revision 1.31  2003/11/05 15:35:44  kuznets
 * Added CDir::GetEntries() based on set of masks
 *
 * Revision 1.30  2003/10/23 12:11:37  ucko
 * Drop <memory> (now unneeded, and should have gone to ncbifile.cpp anyway)
 *
 * Revision 1.29  2003/10/23 03:18:53  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.28  2003/10/08 15:44:53  ivanov
 * Added CDirEntry::DeleteTrailingPathSeparator()
 *
 * Revision 1.27  2003/10/01 14:32:09  ucko
 * +EFollowLinks
 *
 * Revision 1.26  2003/09/30 15:08:28  ucko
 * Reworked CDirEntry::NormalizePath, which now handles .. correctly in
 * all cases and optionally resolves symlinks (on Unix).
 *
 * Revision 1.25  2003/09/16 15:18:13  ivanov
 * + CDirEntry::NormalizePath()
 *
 * Revision 1.24  2003/08/29 16:56:27  ivanov
 * Removed commit about unsafe GetTmpName() and CreateTmpFile() functions in the MT env
 *
 * Revision 1.23  2003/08/08 13:35:29  siyan
 * Changed GetTmpNameExt to GetTmpNameEx, as this is the more appropriate name.
 *
 * Revision 1.22  2003/08/06 13:45:35  siyan
 * Document changes.
 *
 * Revision 1.21  2003/05/29 17:21:31  gouriano
 * added CreatePath() which creates directories recursively
 *
 * Revision 1.20  2003/03/31 16:54:25  siyan
 * Added doxygen support
 *
 * Revision 1.19  2003/02/05 22:07:32  ivanov
 * Added protect and sharing parameters to the CMemoryFile constructor.
 * Added CMemoryFile::Flush() method.
 *
 * Revision 1.18  2003/01/16 13:03:47  dicuccio
 * Added CDir::GetCwd()
 *
 * Revision 1.17  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.16  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.15  2002/07/11 19:21:58  ivanov
 * Added CMemoryFile::MemMapAdvise[Addr]()
 *
 * Revision 1.14  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.13  2002/06/07 16:11:09  ivanov
 * Chenget GetTime() -- using CTime instead time_t, modification time by default
 *
 * Revision 1.12  2002/06/07 15:20:41  ivanov
 * Added CDirEntry::GetTime()
 *
 * Revision 1.11  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.10  2002/04/01 18:49:07  ivanov
 * Added class CMemoryFile
 *
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
