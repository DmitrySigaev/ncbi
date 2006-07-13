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
#include <corelib/ncbistr.hpp>
#include <set>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/general/Dbtag.hpp>

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


static bool s_NoNameSubtype(CSubSource::TSubtype val)
{
    if (val == CSubSource::eSubtype_germline    ||
        val == CSubSource::eSubtype_rearranged  ||
        val == CSubSource::eSubtype_transgenic  ||
        val == CSubSource::eSubtype_environmental_sample) {
        return true;
    }
    return false;
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
        
        if (s_NoNameSubtype(val) ) {
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


// remove those with no name unless it has a subtype that doesn't need a name.
static bool s_SubSourceRemove(const CRef<CSubSource>& s1)
{
    return  ! s1->IsSetName()  &&  ! s_NoNameSubtype(s1->GetSubtype());
}


// is st1 < st2
static bool s_SubsourceCompare(const CRef<CSubSource>& st1,
                               const CRef<CSubSource>& st2)
{
    if (st1->GetSubtype() < st2->GetSubtype()) {
        return true;
    } else if (st1->GetSubtype() == st2->GetSubtype()) {
        if ( st1->IsSetName()  &&  st2->IsSetName()) {
            if (NStr::CompareNocase(st1->GetName(), st2->GetName()) < 0) {
                return true;
            }
        } else if ( ! st1->IsSetName()  &&  st2->IsSetName()) {
            return true;
        }
    }
    return false;
}


// Two SubSource's are equal and duplicates if:
// they have the same subtype
// and the same name (or don't require a name).
static bool s_SubsourceEqual(const CRef<CSubSource>& st1,
                             const CRef<CSubSource>& st2)
{
    if ( st1->GetSubtype() == st2->GetSubtype() ) {
        if ( s_NoNameSubtype(st2->GetSubtype())  ||  
             ( ! st1->IsSetName()  &&  ! st2->IsSetName() ) ||
             ( st1->IsSetName()  &&  st2->IsSetName()  &&
               NStr::CompareNocase(st1->GetName(), st2->GetName()) == 0) ) {
            return true;
        }            
    }
    return false;
}

/*
    Strip all parentheses and commas from the
    beginning and end of the string.
*/
void x_TrimParensAndCommas(string& str)
{
    SIZE_TYPE st = str.find_first_not_of("(),");
    if (st == NPOS){
        str.erase();
    } else if (st > 0) {
        str.erase(0, st);
    }
    
    SIZE_TYPE en = str.find_last_not_of(",()");
    if (en < str.size() - 1) {
        str.erase(en + 1);
    }
}


void x_CombinePrimerStrings(string& orig_seq, const string& new_seq)
{
    if (new_seq.empty()) {
        return;
    }
    if (orig_seq.empty()) {
        orig_seq = new_seq;
        return;
    }
    string new_seq_trim(new_seq);
    x_TrimParensAndCommas(new_seq_trim);
    if ( orig_seq.find(new_seq_trim) != NPOS ) {
        return;
    }
    x_TrimParensAndCommas(orig_seq);
    orig_seq = '(' + orig_seq + ',' + new_seq_trim + ')';
}

void CCleanup_imp::x_SubtypeCleanup(CBioSource& bs)
{
    _ASSERT(bs.IsSetSubtype());
    
    CBioSource::TSubtype& subtypes = bs.SetSubtype();
    
    NON_CONST_ITERATE (CBioSource::TSubtype, it, subtypes) {
        if (*it) {
            BasicCleanup(**it);
        }
    }
    
    // remove those with no name unless it has a subtype that doesn't need a name.
    subtypes.remove_if(s_SubSourceRemove);
    
    // merge any duplicate fwd_primer_seq and rev_primer_seq.
    // and any duplicate fwd_primer_name and rev_primer_name.
    // these are iterators pointing to the subtype into which we will merge others.
    CBioSource::TSubtype::iterator fwd_primer_seq = subtypes.end();
    CBioSource::TSubtype::iterator rev_primer_seq = subtypes.end();
    CBioSource::TSubtype::iterator fwd_primer_name = subtypes.end();
    CBioSource::TSubtype::iterator rev_primer_name = subtypes.end();
    CBioSource::TSubtype::iterator it = subtypes.begin();
    while (it != subtypes.end()) {
        if (*it) {
            CSubSource& ss = **it;
            if ( ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_seq  || 
                 ss.GetSubtype() == CSubSource::eSubtype_rev_primer_seq ) {
                NStr::ToUpper(ss.SetName());
                ss.SetName(NStr::Replace(ss.GetName(), " ", kEmptyStr));
                if (ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_seq) {
                    if (fwd_primer_seq == subtypes.end() ) {
                        fwd_primer_seq = it;
                    } else {
                        x_CombinePrimerStrings((*fwd_primer_seq)->SetName(), ss.GetName());
                        it = subtypes.erase(it);
                        continue;
                    }
                } else if (ss.GetSubtype() == CSubSource::eSubtype_rev_primer_seq) {
                    if (rev_primer_seq == subtypes.end() ) {
                        rev_primer_seq = it;
                    } else {
                        x_CombinePrimerStrings((*rev_primer_seq)->SetName(), ss.GetName());
                        it = subtypes.erase(it);
                        continue;
                    }
                }
            } else if ( ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_name  || 
                        ss.GetSubtype() == CSubSource::eSubtype_rev_primer_name ) {
                if (ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_name) {
                    if (fwd_primer_name == subtypes.end() ) {
                        fwd_primer_name = it;
                    } else {
                        x_CombinePrimerStrings((*fwd_primer_name)->SetName(), ss.GetName());
                        it = subtypes.erase(it);
                        continue;
                    }
                } else if (ss.GetSubtype() == CSubSource::eSubtype_rev_primer_name) {
                    if (rev_primer_name == subtypes.end() ) {
                        rev_primer_name = it;
                    } else {
                        x_CombinePrimerStrings((*rev_primer_name)->SetName(), ss.GetName());
                        it = subtypes.erase(it);
                        continue;
                    }
                }
            }
        }
        ++it;
    }
    
    // sort and remove duplicates.
    // Do not sort before merging primer_seq's above.
    subtypes.sort(s_SubsourceCompare);
    subtypes.unique(s_SubsourceEqual);    
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
    
    if (oref.IsSetDb()) {
        COrg_ref::TDb& dbxref = oref.SetDb();
        
        // dbxrefs cleanup
        COrg_ref::TDb::iterator it = dbxref.begin();
        while (it != dbxref.end()) {
            if (it->Empty()) {
                it = dbxref.erase(it);
                continue;
            }
            BasicCleanup(**it);
            
            ++it;
        }
        
        // sort/unique db_xrefs
        stable_sort(dbxref.begin(), dbxref.end(), SDbtagCompare());
        it = unique(dbxref.begin(), dbxref.end(), SDbtagEqual());
        dbxref.erase(it, dbxref.end());
    }
    
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


// is om1 < om2
// sort by subname first because of how we check equality below.
static bool s_OrgModCompareNameFirst(const CRef<COrgMod>& om1,
                                     const CRef<COrgMod>& om2)
{
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) < 0) {
        return true;
    }
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) == 0  &&
        om1->GetSubtype() < om2->GetSubtype() ) {
        return true;
    }
    return false;
}


// Two OrgMod's are equal and duplicates if:
// they have the same subname and same subtype
// or one has subtype 'other'.
static bool s_OrgModEqual(const CRef<COrgMod>& om1, const CRef<COrgMod>& om2)
{
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) == 0  &&
        (om1->GetSubtype() == om2->GetSubtype() ||
         om2->GetSubtype() == COrgMod::eSubtype_other)) {
        return true;
    }
    return false;
}



// is om1 < om2
// to sort subtypes together.
static bool s_OrgModCompareSubtypeFirst(const CRef<COrgMod>& om1,
                                        const CRef<COrgMod>& om2)
{
    if (om1->GetSubtype() < om2->GetSubtype()) {
        return true;
    }
    if (om1->GetSubtype() == om2->GetSubtype()  &&
        NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) < 0 ) {
        return true;
    }
    return false;
}

void CCleanup_imp::BasicCleanup(COrgName& on)
{
    CLEAN_STRING_MEMBER(on, Attrib);
    CLEAN_STRING_MEMBER(on, Lineage);
    CLEAN_STRING_MEMBER(on, Div);
    if (on.IsSetMod()) {
        COrgName::TMod& mods = on.SetMod();
        COrgName::TMod::iterator it = mods.begin();
        while (it != mods.end() ) {
            BasicCleanup(**it);
            if ((*it)->GetSubname().empty()) {
                it = mods.erase(it);
            } else {
                ++it;
            }
        }
        
        // if type of COrgName::TMod is changed from 'list' 
        // these will need to be changed.
        mods.sort(s_OrgModCompareNameFirst);
        mods.unique(s_OrgModEqual);
        mods.sort(s_OrgModCompareSubtypeFirst);
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


// ExtendedCleanup methods
// combine BioSource descriptors
// Was MergeDupBioSources in C Toolkit
void CCleanup_imp::x_MergeDuplicateBioSources (CBioSource& src, CBioSource& add_src)
{
    // merge genome
    if ((!src.CanGetGenome() || src.GetGenome() == CBioSource::eGenome_unknown)
        && add_src.CanGetGenome()) {
        src.SetGenome(add_src.GetGenome());
    }
    // merge origin
    if ((!src.CanGetOrigin() || src.GetOrigin() == CBioSource::eOrigin_unknown)
        && add_src.CanGetOrigin()) {
        src.SetOrigin(add_src.GetOrigin());
    }
    // merge focus
    if ((!src.CanGetIs_focus() || !src.IsSetIs_focus())
        && add_src.CanGetIs_focus()
        && add_src.IsSetIs_focus()) {
        src.SetIs_focus();
    }
    // merge subtypes
    if (add_src.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, add_src.GetSubtype()) {
            src.SetSubtype().push_back(*it);
        }
        add_src.ResetSubtype();
    }
    // merge in OrgRef
    if (add_src.CanGetOrg() && src.CanGetOrg()) {
        // merge modifiers
        if (add_src.GetOrg().CanGetMod()) {
            ITERATE (COrg_ref::TMod, it, add_src.GetOrg().GetMod()) {
                src.SetOrg().SetMod().push_back(*it);
            }
            add_src.SetOrg().ResetMod();
        }
        // merge db
        if (add_src.GetOrg().CanGetDb()) {
            ITERATE (COrg_ref::TDb, it, add_src.GetOrg().GetDb()) {
                src.SetOrg().SetDb().push_back(*it);
            }
            add_src.SetOrg().ResetDb();
        }
        // merge syn
        if (add_src.GetOrg().CanGetSyn()) {
            ITERATE (COrg_ref::TSyn, it, add_src.GetOrg().GetSyn()) {
                src.SetOrg().SetSyn().push_back(*it);
            }
            add_src.SetOrg().ResetSyn();
        }
        // merge in orgname
        if (add_src.GetOrg().CanGetOrgname()) {
            if (!src.GetOrg().CanGetOrgname()) {
                src.SetOrg().ResetOrgname();
            }
            if (add_src.GetOrg().GetOrgname().CanGetMod()) {
                // merge mod
                ITERATE (COrgName::TMod, it, add_src.GetOrg().GetOrgname().GetMod()) {
                    src.SetOrg().SetOrgname().SetMod().push_back(*it);
                }
                add_src.SetOrg().SetOrgname().ResetMod();
            }
            
            // merge gcode
            if (add_src.GetOrg().GetOrgname().CanGetGcode()
                && (!src.GetOrg().GetOrgname().CanGetGcode()
                    || src.GetOrg().GetOrgname().GetGcode() == 0)) {
                src.SetOrg().SetOrgname().SetGcode(add_src.GetOrg().GetOrgname().GetGcode());
            }
            // merge mgcode
            if (add_src.GetOrg().GetOrgname().CanGetMgcode()
                && (!src.GetOrg().GetOrgname().CanGetMgcode()
                    || src.GetOrg().GetOrgname().GetMgcode() == 0)) {
                src.SetOrg().SetOrgname().SetMgcode(add_src.GetOrg().GetOrgname().GetMgcode());
            }
            // merge lineage
            if (add_src.GetOrg().GetOrgname().CanGetLineage()
                && !NStr::IsBlank(add_src.GetOrg().GetOrgname().GetLineage())
                && (!src.GetOrg().GetOrgname().CanGetLineage()
                    || NStr::IsBlank(src.GetOrg().GetOrgname().GetLineage()))) {
                src.SetOrg().SetOrgname().SetLineage(add_src.GetOrg().GetOrgname().GetLineage());
            }
            // merge div
            if (add_src.GetOrg().GetOrgname().CanGetDiv()
                && !NStr::IsBlank(add_src.GetOrg().GetOrgname().GetDiv())
                && (!src.GetOrg().GetOrgname().CanGetDiv()
                    || NStr::IsBlank(src.GetOrg().GetOrgname().GetDiv()))) {
                src.SetOrg().SetOrgname().SetDiv(add_src.GetOrg().GetOrgname().GetDiv());
            }
        }
    }    
}


void CCleanup_imp::x_MergeDuplicateBioSources (CSeq_descr& sdr)
{
    CSeq_descr::Tdata& current_list = sdr.Set();
    CSeq_descr::Tdata new_list;    
    
    new_list.clear();
 
    while (current_list.size() > 0) {
        CRef< CSeqdesc > sd = current_list.back();
        current_list.pop_back();
        if ((*sd).Which() == CSeqdesc::e_Source 
             && (*sd).GetSource().CanGetOrg()
             && (*sd).GetSource().GetOrg().CanGetTaxname()
             && !NStr::IsBlank((*sd).GetSource().GetOrg().GetTaxname())) {
            bool found_match = false;
            for (CSeq_descr::Tdata::iterator it = sdr.Set().begin();
                 it != sdr.Set().end() && ! found_match; ++it) {
                if ((**it).Which() == CSeqdesc::e_Source
                    && (**it).GetSource().CanGetOrg()
                    && (**it).GetSource().GetOrg().CanGetTaxname()
                    && NStr::Equal((*sd).GetSource().GetOrg().GetTaxname(),
                                   (**it).GetSource().GetOrg().GetTaxname())) {
                    // add information from sd to **it     
                    x_MergeDuplicateBioSources ((**it).SetSource(), (*sd).SetSource());                                   
                    found_match = true;
                }
            }
            if (!found_match) {
               new_list.push_front(sd);
            }
        } else {
            new_list.push_front(sd);
        }
    }
    
    while (new_list.size() > 0) {
        CRef< CSeqdesc > sd = new_list.front();
        new_list.pop_front();
        current_list.push_back (sd);
    }
}


void CCleanup_imp::x_MergeDuplicateBioSources(CBioseq& bs)
{
    if (bs.IsSetDescr()) {
        x_MergeDuplicateBioSources(bs.SetDescr());
        if (bs.SetDescr().Set().empty()) {
            bs.ResetDescr();
        }
    }
}


void CCleanup_imp::x_MergeDuplicateBioSources(CBioseq_set& bss)
{
    if (bss.IsSetDescr()) {
        x_MergeDuplicateBioSources(bss.SetDescr());
        if (bss.SetDescr().Set().empty()) {
            bss.ResetDescr();
        }
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    x_MergeDuplicateBioSources(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    x_MergeDuplicateBioSources(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


void CCleanup_imp::x_CleanOrgNameStrings(COrgName &on)
{
    if (on.CanGetAttrib()) {
        CleanVisString(on.SetAttrib());
    }
    if (on.CanGetLineage()) {
        CleanVisString(on.SetLineage());
    }
    if (on.CanGetDiv()) {
        CleanVisString(on.SetDiv());
    }
    if (on.CanGetMod()) {
        NON_CONST_ITERATE(COrgName::TMod, modI, on.SetMod()) {
            CleanVisString ((*modI)->SetSubname());
        }
    }
}


void CCleanup_imp::x_ExtendedCleanStrings (COrg_ref &org)
{
    EXTENDED_CLEAN_STRING_MEMBER(org, Taxname);
    EXTENDED_CLEAN_STRING_MEMBER(org, Common);
    if (org.CanGetMod()) {
        CleanVisStringList (org.SetMod());
    }
    if (org.CanGetSyn()) {
        CleanVisStringList (org.SetSyn());
    }
    if (org.CanGetOrgname()) {
        x_CleanOrgNameStrings(org.SetOrgname());
    }
}


void CCleanup_imp::x_ExtendedCleanSubSourceList (CBioSource &bs)
{
    if (bs.IsSetSubtype()) {
        CBioSource::TSubtype& subtypes = bs.SetSubtype();
        CBioSource::TSubtype tmp;
        tmp.clear();
        NON_CONST_ITERATE (CBioSource::TSubtype, it, subtypes) {
            if ((*it)->CanGetAttrib()) {
                CleanVisString((*it)->SetAttrib());
            }
            int subtype = (*it)->GetSubtype();
            if (subtype != CSubSource::eSubtype_germline
                && subtype != CSubSource::eSubtype_rearranged
                && subtype != CSubSource::eSubtype_transgenic
                && subtype != CSubSource::eSubtype_environmental_sample) {
                CleanVisString((*it)->SetName());
                if (!NStr::IsBlank((*it)->GetName())) {
                    tmp.push_back(*it);
                }
            } else {
                tmp.push_back(*it);
            }            
        }
        if (subtypes.size() > tmp.size()) {
            subtypes.clear();
            NON_CONST_ITERATE (CBioSource::TSubtype, it, tmp) {
                subtypes.push_back(*it);
            }            
        }
    }    
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.9  2006/07/13 22:16:13  ucko
 * x_TrimParensAndCommas: call string::erase rather than string::clear
 * for compatibility with GCC 2.95.
 *
 * Revision 1.8  2006/07/13 19:31:15  ucko
 * Pass remove_if(), sort(), and unique() functions rather than predicate
 * objects, which are overkill and break on WorkShop.
 *
 * Revision 1.7  2006/07/13 17:12:12  rsmith
 * Bring up to date with C BSEC.
 *
 * Revision 1.6  2006/07/06 15:10:52  bollin
 * avoid setting empty values
 *
 * Revision 1.5  2006/07/05 17:26:11  bollin
 * cleared compiler error
 *
 * Revision 1.4  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.3  2006/06/28 13:22:39  bollin
 * added step to merge duplicate biosources to ExtendedCleanup
 *
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
