/* $Id$
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
 * Author:  Robert G. Smith
 *
 * File Description:
 *   implementation of BasicCleanup for CBioSource and sub-objects.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <set>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::



/****  BioSource cleanup ****/

void CCleanup_imp::BasicCleanup(CBioSource& bs)
{
    if (bs.IsSetOrg()) {
        BasicCleanup(bs.SetOrg());
        // convert COrg_reg.TMod string to SubSource objects
        x_OrgModToSubtype(bs);
    }
    if (bs.IsSetSubtype()) {
        x_SubtypeCleanup(bs);
    }
}


static CSubSource* s_StringToSubSource(const string& str)
{
    size_t pos = str.find('=');
    if (pos == NPOS) {
        pos = str.find(' ');
    }
    string subtype = str.substr(0, pos);
    try {
        CSubSource::TSubtype val = CSubSource::GetSubtypeValue(subtype);
        
        string name;
        if (pos != NPOS) {
            string name = str.substr(pos + 1);
        }
        NStr::TruncateSpacesInPlace(name);
        
        if (val == CSubSource::eSubtype_germline    ||
            val == CSubSource::eSubtype_rearranged  ||
            val == CSubSource::eSubtype_transgenic  ||
            val == CSubSource::eSubtype_environmental_sample) {
            if (NStr::IsBlank(name)) {
                name = " ";
            }
        }
        
        if (NStr::IsBlank(name)) {
            return NULL;
        }
        
        size_t num_spaces = 0;
        bool has_comma = false;
        ITERATE (string, it, name) {
            if (isspace((unsigned char)(*it))) {
                ++num_spaces;
            } else if (*it == ',') {
                has_comma = true;
                break;
            }
        }
        
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        return new CSubSource(val, name);
    } catch (CSerialException&) {}
    return NULL;
}


void CCleanup_imp::x_OrgModToSubtype(CBioSource& bs)
{
    _ASSERT(bs.IsSetOrg());
    
    if (!bs.SetOrg().IsSetMod()) {
        return;
    }
    CBioSource::TOrg::TMod& mod_list = bs.SetOrg().SetMod();
    
    CBioSource::TOrg::TMod::iterator it = mod_list.begin();
    while (it != mod_list.end()) {
        CRef<CSubSource> subsrc(s_StringToSubSource(*it));
        if (subsrc) {
            bs.SetSubtype().push_back(subsrc);
            it = mod_list.erase(it);
        } else {
            ++it;
        }
    }
}


struct SSubsourceCompare
{
    bool operator()(const CRef<CSubSource>& s1, const CRef<CSubSource>& s2) {
        return s1->IsSetSubtype()  &&  s2->IsSetSubtype()  &&
        s1->GetSubtype() < s2->GetSubtype();
    }
};


void CCleanup_imp::x_SubtypeCleanup(CBioSource& bs)
{
    _ASSERT(bs.IsSetSubtype());
    
    typedef multiset<CRef<CSubSource>, SSubsourceCompare> TSorter;
    
    CBioSource::TSubtype& subtypes = bs.SetSubtype();
    TSorter   tmp;
    
    NON_CONST_ITERATE (CBioSource::TSubtype, it, subtypes) {
        if (*it) {
            BasicCleanup(**it);
            tmp.insert(*it);
        }
    }
    
    subtypes.clear();
    ITERATE (TSorter, it, tmp) {
        subtypes.push_back(*it);
    }
}


void CCleanup_imp::BasicCleanup(COrg_ref& oref)
{
    CLEAN_STRING_MEMBER(oref, Taxname);
    CLEAN_STRING_MEMBER(oref, Common);
    CLEAN_STRING_LIST(oref, Mod);
    CLEAN_STRING_LIST(oref, Syn);
    
    if (oref.IsSetOrgname()) {
        if (oref.IsSetMod()) {
            x_ModToOrgMod(oref);
        }
        BasicCleanup(oref.SetOrgname());
    }
    
    // !! To do: cleanup dbxref (sort, unique)
}


static COrgMod* s_StringToOrgMod(const string& str)
{
    try {
        size_t pos = str.find('=');
        if (pos == NPOS) {
            pos = str.find(' ');
        }
        if (pos == NPOS) {
            return NULL;
        }
        
        string subtype = str.substr(0, pos);
        string subname = str.substr(pos + 1);
        NStr::TruncateSpacesInPlace(subname);
        
        
        size_t num_spaces = 0;
        bool has_comma = false;
        ITERATE (string, it, subname) {
            if (isspace((unsigned char)(*it))) {
                ++num_spaces;
            } else if (*it == ',') {
                has_comma = true;
                break;
            }
        }
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        
        return new COrgMod(subtype, subname);
    } catch (CSerialException&) {}
    return NULL;
}


void CCleanup_imp::x_ModToOrgMod(COrg_ref& oref)
{
    _ASSERT(oref.IsSetMod()  &&  oref.IsSetOrgname());
    
    COrg_ref::TMod& mod_list = oref.SetMod();
    COrg_ref::TOrgname& orgname = oref.SetOrgname();
    
    COrg_ref::TMod::iterator it = mod_list.begin();
    while (it != mod_list.end()) {
        CRef<COrgMod> orgmod(s_StringToOrgMod(*it));
        if (orgmod) {
            orgname.SetMod().push_back(orgmod);
            it = mod_list.erase(it);
        } else {
            ++it;
        }
    }   
}


void CCleanup_imp::BasicCleanup(COrgName& on)
{
    CLEAN_STRING_MEMBER(on, Attrib);
    CLEAN_STRING_MEMBER(on, Lineage);
    CLEAN_STRING_MEMBER(on, Div);
    if (on.IsSetMod()) {
        NON_CONST_ITERATE (COrgName::TMod, it, on.SetMod()) {
            BasicCleanup(**it);
        }
    }
}


void CCleanup_imp::BasicCleanup(COrgMod& om)
{
    CLEAN_STRING_MEMBER(om, Subname);
    CLEAN_STRING_MEMBER(om, Attrib);
}


void CCleanup_imp::BasicCleanup(CSubSource& ss)
{
    CLEAN_STRING_MEMBER(ss, Name);
    CLEAN_STRING_MEMBER(ss, Attrib);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2006/03/23 18:30:56  rsmith
 * cosmetic and comment changes.
 *
 * Revision 1.1  2006/03/20 14:21:25  rsmith
 * move Biosource related cleanup to its own file.
 *
 *
 *
 * ===========================================================================
 */
