#ifndef CORELIB___VERSION__HPP
#define CORELIB___VERSION__HPP

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
 * Authors:  Denis Vakatov, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 *
 */

/// @file version.hpp
/// Define CVersionInfo, a version info storage class.


#include <corelib/ncbistd.hpp>


/** @addtogroup Version
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CVersionInfo


/////////////////////////////////////////////////////////////////////////////
///
/// CVersionInfo --
///
/// Define class for storing version information.

class NCBI_XNCBI_EXPORT CVersionInfo
{
public:
    /// Constructor.
    CVersionInfo(int  ver_major,
                 int  ver_minor,
                 int  patch_level = 0,
                 const string& name        = kEmptyStr);

    /// Constructor.
    CVersionInfo(const CVersionInfo& version);

    /// Destructor.
    virtual ~CVersionInfo() {}

    /// Print version information.
    ///
    /// Version information is printed in the following forms:
    /// - <ver_major>.<ver_minor>.<patch_level>
    /// - <ver_major>.<ver_minor>.<patch_level> (<name>)
    virtual string Print(void) const;

    /// Major version
    int GetMajor(void) const { return m_Major; }
    /// Minor version
    int GetMinor(void) const { return m_Minor; }
    /// Patch level
    int GetPatchLevel(void) const { return m_PatchLevel; }

    const string& GetName(void) const { return m_Name; }

    static const CVersionInfo kAny;      /// { 0,  0,  0}
    static const CVersionInfo kLatest;   /// {-1, -1, -1}

    /// Version comparison result
    /// @sa Match
    enum EMatch {
        eNonCompatible,           ///< major, minor does not match
        eConditionallyCompatible, ///< patch level incompatibility
        eBackwardCompatible,      ///< patch level is newer
        eFullyCompatible          ///< exactly the same version
    };

    /// Check if version matches another version.
    /// @param version_info
    ///   Version Info to compare with
    EMatch Match(const CVersionInfo& version_info) const;

    /// Check if version is all zero (major, minor, patch)
    /// Convention is that all-zero version used in requests as 
    /// "get me anything". 
    /// @sa kAny
    bool IsAny() const 
       { return (!m_Major && !m_Minor && !m_PatchLevel); }

    /// Check if version is all -1 (major, minor, patch)
    /// Convention is that -1 version used in requests as 
    /// "get me the latest version". 
    /// @sa kLatest
    bool IsLatest() const 
       { return (m_Major == -1 && m_Minor == -1 && m_PatchLevel == -1); }

protected:
    int           m_Major;       ///< Major number
    int           m_Minor;       ///< Minor number
    int           m_PatchLevel;  ///< Patch level
    const  string m_Name;        ///< Name
};


/// Algorithm function to find version in the container
///
/// Scans the provided iterator for version with the same major and
/// minor version and the newest patch level.
///
/// @param first
///    first iterator to start search 
/// @param last
///    ending iterator (typically returned by end() fnction of an STL
///    container)
/// @return 
///    iterator on the best version or last
template<class It>
It FindVersion(It first, It last, const CVersionInfo& info)
{
    It  best_version = last;  // not found by default
    int best_major = -1;
    int best_minor = -1;
    int best_patch_level = -1;

    for ( ;first != last; ++first) {
        const CVersionInfo& vinfo = *first;
        
        int major = vinfo.GetMajor();
        int minor = vinfo.GetMinor();
        int patch_level = vinfo.GetPatchLevel();

        if (info.GetMajor() == -1) {  // best major search
            if (major > best_major) { 
                best_version = it;
                best_major = major;
                best_minor = minor;
                best_patch_level = patch_level;
                continue;
            }
        } else { // searching for the specific major version
            if (info.GetMajor() != major) {
                continue;
            }
        }

        if (info.GetMinor() == -1) {  // best minor search
            if (minor > best_minor) {
                best_version = it;
                best_major = major;
                best_minor = minor;
                best_patch_level = patch_level;
                continue;
            }
        } else { 
            if (info.GetMinor() != minor) {
                continue;
            }
        }

        // always looking for the best patch
        if (patch_level > best_patch_level) {
                best_version = it;
                best_major = major;
                best_minor = minor;
                best_patch_level = patch_level;
                continue;
        }

    
    } // for
    
    return best_version;
}

/* @} */


END_NCBI_SCOPE




/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2003/11/07 16:29:15  kuznets
 * Added CVersionInfo::IsAny(), IsLatest()
 *
 * Revision 1.6  2003/10/30 19:24:44  kuznets
 * Merged together version of CVersionInfo mastered by Denis with my
 * version of CVersionInfo...Best of both versions.
 *
 * Revision 1.5  2003/10/30 16:38:08  kuznets
 * CVersionInfo changed:
 *  - added accessors for major, minor and patch level
 *  - Match function to compare versions
 * Added FindVersion algorithm to scan containers for the best version
 *
 * Revision 1.4  2003/09/08 12:17:42  siyan
 * Documentation changes.
 *
 * Revision 1.3  2003/04/01 19:19:52  siyan
 * Added doxygen support
 *
 * Revision 1.2  2002/12/26 19:18:07  dicuccio
 * Added empty virtual destructor
 *
 * Revision 1.1  2002/12/26 17:11:10  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif // CORELIB___VERSION__HPP



