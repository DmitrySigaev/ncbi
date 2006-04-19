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
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objtools/format/context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CAutoDefAvailableModifier::CAutoDefAvailableModifier() 
                    : m_AllUnique (true), 
                      m_AllPresent (true), 
                      m_IsUnique(true), 
                      m_IsOrgMod(true), 
                      m_SubSrcType(CSubSource::eSubtype_other),
                      m_OrgModType(COrgMod::eSubtype_other)
{
    m_ValueList.clear();
}


CAutoDefAvailableModifier::CAutoDefAvailableModifier(unsigned int type, bool is_orgmod)
                    : m_AllUnique (true), 
                      m_AllPresent (true), 
                      m_IsUnique(true), 
                      m_IsOrgMod(is_orgmod), 
                      m_SubSrcType(CSubSource::eSubtype_other),
                      m_OrgModType(COrgMod::eSubtype_other)
{
    m_ValueList.clear();
    if (is_orgmod) {
        m_OrgModType = (COrgMod::ESubtype) type;
    } else {
        m_SubSrcType = (CSubSource::ESubtype) type;
    }
}
    

CAutoDefAvailableModifier::~CAutoDefAvailableModifier()
{
}

   
void CAutoDefAvailableModifier::SetOrgModType(COrgMod::ESubtype orgmod_type)
{
    m_IsOrgMod = true;
    m_OrgModType = orgmod_type;
}


void CAutoDefAvailableModifier::SetSubSourceType(CSubSource::ESubtype subsrc_type)
{
    m_IsOrgMod = false;
    m_SubSrcType = subsrc_type;
}


void CAutoDefAvailableModifier::ValueFound(string val_found)
{
    bool found = false;
    if (NStr::Equal("", val_found)) {
        m_AllPresent = false;
    } else {
        for (unsigned int k = 0; k < m_ValueList.size(); k++) {
            if (NStr::Equal(val_found, m_ValueList[k])) {
                m_AllUnique = false;
                found = true;
                break;
            }
        }
        if (!found && m_ValueList.size() > 0) {
            m_IsUnique = false;
        }
        if (!found) {
            m_ValueList.push_back(val_found);
        }
    }
}


bool CAutoDefAvailableModifier::AnyPresent() const
{
    if (m_ValueList.size() > 0) {
        return true;
    } else {
        return false;
    }
}


unsigned int CAutoDefAvailableModifier::GetRank() const
{
    if (m_IsOrgMod) {
        if (m_OrgModType == COrgMod::eSubtype_strain) {
            return 3;
        } else if (m_OrgModType == COrgMod::eSubtype_isolate) {
            return 5;
        } else if (m_OrgModType == COrgMod::eSubtype_cultivar) {
            return 7;
        } else if (m_OrgModType == COrgMod::eSubtype_specimen_voucher) {
            return 8;
        } else if (m_OrgModType == COrgMod::eSubtype_ecotype) {
            return 9;
        } else if (m_OrgModType == COrgMod::eSubtype_type) {
            return 10;
        } else if (m_OrgModType == COrgMod::eSubtype_serotype) {
            return 11;
        } else if (m_OrgModType == COrgMod::eSubtype_authority) {
            return 12;
        } else if (m_OrgModType == COrgMod::eSubtype_breed) {
            return 13;
        }
    } else {
        if (m_SubSrcType == CSubSource::eSubtype_transgenic) {
            return 0;
        } else if (m_SubSrcType == CSubSource::eSubtype_plasmid_name) {
            return 1;
         } else if (m_SubSrcType == CSubSource::eSubtype_endogenous_virus_name)  {
            return 2;
        } else if (m_SubSrcType == CSubSource::eSubtype_clone) {
            return 4;
        } else if (m_SubSrcType == CSubSource::eSubtype_haplotype) {
            return 6;
        }
    }
    return 50;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2006/04/19 13:43:50  dicuccio
* Stylistic changes.  Made several accessors const.
*
* Revision 1.3  2006/04/17 17:42:21  ucko
* Drop extraneous and disconcerting inclusion of gui headers.
*
* Revision 1.2  2006/04/17 17:39:37  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

