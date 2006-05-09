#ifndef OBJMGR_UTIL_AUTODEF_AVAILABLE_MODIFIER__HPP
#define OBJMGR_UTIL_AUTODEF_AVAILABLE_MODIFIER__HPP

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
* Author:  Colleen Bollin
*
* File Description:
*   Creates unique definition lines for sequences in a set using organism
*   descriptions and feature clauses.
*/

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJUTIL_EXPORT CAutoDefAvailableModifier
{
public:
    CAutoDefAvailableModifier();
    CAutoDefAvailableModifier(unsigned int type, bool is_orgmod);
    CAutoDefAvailableModifier (const CAutoDefAvailableModifier& copy);
    ~CAutoDefAvailableModifier();
    
    void SetOrgModType(COrgMod::ESubtype orgmod_type);
    void SetSubSourceType(CSubSource::ESubtype subsrc_type);
    void ValueFound(string val_found);
    
    bool AnyPresent() const;
    bool AllUnique() const { return m_AllUnique; }
    bool AllPresent() const { return m_AllPresent; }
    bool IsUnique() const { return m_IsUnique; }
    bool IsOrgMod() const { return m_IsOrgMod; }
    bool IsRequested() const { return m_IsRequested; }
    void SetRequested(bool is_requested) { m_IsRequested = is_requested; }

    CSubSource::ESubtype GetSubSourceType() const { return m_SubSrcType; }
    COrgMod::ESubtype GetOrgModType() const { return m_OrgModType; }
    void FirstValue(string &first_val);

    unsigned int GetRank() const;
    typedef vector<string> TValueVector;
    
    bool operator<(const CAutoDefAvailableModifier& rhs) const;
    bool operator==(const CAutoDefAvailableModifier& rhs) const;
    
    static string GetSubSourceLabel (CSubSource::ESubtype st);
    static string GetOrgModLabel(COrgMod::ESubtype st);

    string Label() const;
    
private:
    bool m_IsOrgMod;
    CSubSource::ESubtype m_SubSrcType;
    COrgMod::ESubtype    m_OrgModType;
    bool m_AllUnique;
    bool m_AllPresent;
    bool m_IsUnique;
    bool m_IsRequested;
    TValueVector m_ValueList;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2006/05/09 16:27:54  bollin
* added functions to allow modifiers to be selected in dialog
*
* Revision 1.3  2006/04/19 13:43:50  dicuccio
* Stylistic changes.  Made several accessors const.
*
* Revision 1.2  2006/04/17 17:39:36  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:01  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

#endif //OBJMGR_UTIL_AUTODEF_AVAILABLE_MODIFIER__HPP
