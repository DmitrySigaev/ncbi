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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   CVersionInfo -- a version info storage class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/version.hpp>


BEGIN_NCBI_SCOPE

const CVersionInfo CVersionInfo::kAny(0, 0, 0);
const CVersionInfo CVersionInfo::kLatest(-1, -1, -1);


CVersionInfo::CVersionInfo(int ver_major,
                           int  ver_minor,
                           int  patch_level, 
                           const string& name) 
    : m_Major(ver_major),
      m_Minor(ver_minor),
      m_PatchLevel(patch_level),
      m_Name(name)

{
}


CVersionInfo::CVersionInfo(const CVersionInfo& version)
    : m_Major(version.m_Major),
      m_Minor(version.m_Minor),
      m_PatchLevel(version.m_PatchLevel),
      m_Name(version.m_Name)
{
}


string CVersionInfo::Print(void) const
{
    CNcbiOstrstream os;
    os << m_Major << "." << m_Minor << "." << m_PatchLevel;
    if ( !m_Name.empty() ) {
        os << " (" << m_Name << ")";
    }
    return CNcbiOstrstreamToString(os);
}


CVersionInfo::EMatch 
CVersionInfo::Match(const CVersionInfo& version_info) const
{
    if (GetMajor() != version_info.GetMajor())
        return eNonCompatible;

    if (GetMinor() != version_info.GetMinor())
        return eNonCompatible;

    if (GetPatchLevel() == version_info.GetPatchLevel()) {
        return eFullyCompatible;
    }

    if (GetPatchLevel() > version_info.GetPatchLevel()) {
        return eBackwardCompatible;
    }

    return eConditionallyCompatible;

}


bool IsBetterVersion(const CVersionInfo& info, 
                     const CVersionInfo& cinfo,
                     int&  best_major, 
                     int&  best_minor,
                     int&  best_patch_level)
{
    int major = cinfo.GetMajor();
    int minor = cinfo.GetMinor();
    int patch_level = cinfo.GetPatchLevel();

    if (info.GetMajor() == -1) {  // best major search
        if (major > best_major) { 
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
        }
    } else { // searching for the specific major version
        if (info.GetMajor() != major) {
            return false;
        }
    }

    if (info.GetMinor() == -1) {  // best minor search
        if (minor > best_minor) {
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
        }
    } else { 
        if (info.GetMinor() != minor) {
            return false;
        }
    }

    // always looking for the best patch
    if (patch_level > best_patch_level) {
            best_major = major;
            best_minor = minor;
            best_patch_level = patch_level;
            return true;
    }
    return false;    
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.6  2003/11/19 21:46:59  vasilche
 * Removed extra '\0' from version info.
 *
 * Revision 1.5  2003/11/17 19:51:17  kuznets
 * + IsBetterVersion service function
 *
 * Revision 1.4  2003/10/30 19:25:35  kuznets
 * Reflecting changes in hpp file.
 *
 * Revision 1.3  2003/10/30 16:38:50  kuznets
 * Implemented CVersionInfo::Match
 *
 * Revision 1.2  2003/01/13 20:42:50  gouriano
 * corrected the problem with ostrstream::str(): replaced such calls with
 * CNcbiOstrstreamToString(os)
 *
 * Revision 1.1  2002/12/26 17:10:47  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
