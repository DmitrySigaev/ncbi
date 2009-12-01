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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Implementation of utility classes and functions.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>

#include <vector>
#include <algorithm>
#include <list>

#include "utilities.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


// =============================================================================
//                                    Functions
// =============================================================================


bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss)
{
    for ( CTypeConstIterator <CBioseq_set> si(se); si; ++si ) {
        if ( si->GetClass() == clss ) {
            return true;
        }
    }
    return false;
}


bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope)
{
    CBioseq_Handle bsh = scope->GetBioseqHandle(loc);
    const CSeq_entry& se = *bsh.GetTopLevelEntry().GetCompleteSeq_entry();

    if ( bsh.IsSetInst_Repr() ) {
        CBioseq_Handle::TInst::TRepr repr = bsh.GetInst_Repr();
        if ( repr == CSeq_inst::eRepr_delta ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_nuc_prot) ) {
                return true;
            }
        }
        if ( repr == CSeq_inst::eRepr_seg ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_parts) ) {
                return true;
            }
        }
    }

    return false;
}


// Check if string is either empty or contains just white spaces
bool IsBlankStringList(const list< string >& str_list)
{
    ITERATE( list< string >, str, str_list ) {
        if ( !NStr::IsBlank(*str) ) {
            return false;
        }
    }
    return true;
}


int GetGIForSeqId(const CSeq_id& id)
{
    int gi = 0;
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));

    try {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        gi = scope->GetGi (idh);
	} catch (CException &x1) {
	} catch (std::exception &x2) {
    }
    return gi;
}



CScope::TIds GetSeqIdsForGI(int gi)
{
    CScope::TIds id_list;
    CSeq_id tmp_id;
    tmp_id.SetGi(gi);
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();

    try {
        id_list = scope->GetIds(tmp_id);

	} catch (CException &x1) {
	} catch (std::exception &x2) {
    }
    return id_list;
}


string GetSequenceStringFromLoc
(const CSeq_loc& loc,
 CScope& scope)
{
    CNcbiOstrstream oss;
    CFastaOstream fasta_ostr(oss);
    fasta_ostr.SetFlag(CFastaOstream::fAssembleParts);
    fasta_ostr.SetFlag(CFastaOstream::fInstantiateGaps);
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        const CSeq_loc& part = citer.GetSeq_loc();
        CBioseq_Handle bsh = scope.GetBioseqHandle(part);
        fasta_ostr.WriteSequence (bsh, &part);
    }
    string s = CNcbiOstrstreamToString(oss);
    if (NStr::EndsWith(s, "\n")) {
        s = s.substr(0, s.size() - 1);
    }
    return s;
}


CSeqVector GetSequenceFromLoc
(const CSeq_loc& loc,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding)
{
    CConstRef<CSeqMap> map = 
        CSeqMap::CreateSeqMapForSeq_loc(loc, &scope);
    ENa_strand strand = sequence::GetStrand(loc, &scope);

    return CSeqVector(*map, scope, coding, strand);
}


CSeqVector GetSequenceFromFeature
(const CSeq_feat& feat,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding,
 bool product)
{

    if ( (product   &&  !feat.CanGetProduct())  ||
         (!product  &&  !feat.CanGetLocation()) ) {
        return CSeqVector();
    }

    const CSeq_loc* loc = product ? &feat.GetProduct() : &feat.GetLocation();
    return GetSequenceFromLoc(*loc, scope, coding);
}


/***** Calculate Accession for a given object *****/

static string s_GetBioseqAcc(const CBioseq_Handle& handle)
{
    if (handle) {
        try {
            return sequence::GetId(handle).GetSeqId()->GetSeqIdString();
        } catch (CException&) {}
    }
    return kEmptyStr;
}


static string s_GetSeq_featAcc(const CSeq_feat& feat, CScope& scope)
{
    CBioseq_Handle seq = sequence::GetBioseqFromSeqLoc(feat.GetLocation(), scope);
    return s_GetBioseqAcc(seq);
}


static string s_GetBioseqAcc(const CBioseq& seq, CScope& scope)
{
    CBioseq_Handle handle = scope.GetBioseqHandle(seq);
    return s_GetBioseqAcc(handle);
}

static const CBioseq* s_GetSeqFromSet(const CBioseq_set& bsst, CScope& scope)
{
    const CBioseq* retval = NULL;

    switch (bsst.GetClass()) {
        case CBioseq_set::eClass_gen_prod_set:
            // find the genomic bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    const CSeq_inst& inst = (*it)->GetSeq().GetInst();
                    if (inst.IsSetMol()  &&  inst.GetMol() == CSeq_inst::eMol_dna) {
                        retval = &(*it)->GetSeq();
                        break;
                    }
                }
            }
            break;
        case CBioseq_set::eClass_nuc_prot:
            // find the nucleotide bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()  &&  (*it)->GetSeq().IsNa()) {
                    retval = &(*it)->GetSeq();
                    break;
                }
            }
            break;
        case CBioseq_set::eClass_segset:
            // find the master bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    retval = &(*it)->GetSeq();
                    break;
                }
            }
            break;

        default:
            // find the first bioseq
            CTypeConstIterator<CBioseq> seqit(ConstBegin(bsst));
            if (seqit) {
                retval = &(*seqit);
            }
            break;
    }

    return retval;
}


string GetAccessionFromObjects(const CSerialObject* obj, const CSeq_entry* ctx, CScope& scope)
{
    if (ctx) {
        if (ctx->IsSeq()) {
            return s_GetBioseqAcc(ctx->GetSeq(), scope);
        } else if (ctx->IsSet()) {
            const CBioseq* seq = s_GetSeqFromSet(ctx->GetSet(), scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope);
            }
        }
    } else if (obj) {
        if (obj->GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
            const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*obj);
            return s_GetSeq_featAcc(feat, scope);
        } else if (obj->GetThisTypeInfo() == CBioseq::GetTypeInfo()) {
            const CBioseq& seq = dynamic_cast<const CBioseq&>(*obj);
            return s_GetBioseqAcc(seq, scope);
        } else if (obj->GetThisTypeInfo() == CBioseq_set::GetTypeInfo()) {
            const CBioseq_set& bsst = dynamic_cast<const CBioseq_set&>(*obj);
            const CBioseq* seq = s_GetSeqFromSet(bsst, scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope);
            }
        } else if (obj->GetThisTypeInfo() == CSeq_annot::GetTypeInfo()) {
            CSeq_annot_Handle ah = scope.GetSeq_annotHandle (dynamic_cast<const CSeq_annot&>(*obj));
            if (ah) {
                CSeq_entry_Handle seh = ah.GetParentEntry();
                if (seh) {
                    if (seh.IsSeq()) {
                        return s_GetBioseqAcc(seh.GetSeq());
                    } else if (seh.IsSet()) {
                        CBioseq_set_Handle bsh = seh.GetSet();
                        const CBioseq_set& bsst = *(bsh.GetCompleteBioseq_set());
                        const CBioseq* seq = s_GetSeqFromSet(bsst, scope);
                        if (seq != NULL) {
                            return s_GetBioseqAcc(*seq, scope);
                        }
                    }
                }
            }

        } // TO DO: graph
    }
    return kEmptyStr;
}


CBioseq_set_Handle GetGenProdSetParent (CBioseq_set_Handle set)
{
    CBioseq_set_Handle gps;

    CSeq_entry_Handle parent = set.GetParentEntry();
    if (!parent) {
        return gps;
    } else if (!(parent = parent.GetParentEntry())) {
        return gps;
    } else if (!parent.IsSet()) {
        return gps;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set) {
        return parent.GetSet();
    } else {
        return GetGenProdSetParent (parent.GetSet());
    }    
}


CBioseq_set_Handle GetGenProdSetParent (CBioseq_Handle bioseq)
{
    CBioseq_set_Handle gps;

    CSeq_entry_Handle parent = bioseq.GetParentEntry();
    if (!parent) {
        return gps;
    } else if (!(parent = parent.GetParentEntry())) {
        return gps;
    } else if (!parent.IsSet()) {
        return gps;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set) {
        return parent.GetSet();
    } else {
        return GetGenProdSetParent (parent.GetSet());
    }
}


CBioseq_Handle GetNucBioseq (CBioseq_set_Handle bioseq_set)
{
    CBioseq_Handle nuc;

    if (!bioseq_set) {
        return nuc;
    }
    CBioseq_CI bit(bioseq_set, CSeq_inst::eMol_na);
    if (bit) {
        nuc = *bit;
    } else {
        CSeq_entry_Handle parent = bioseq_set.GetParentEntry();
        if (parent && (parent = parent.GetParentEntry())
            && parent.IsSet()) {
            nuc = GetNucBioseq (parent.GetSet());
        }
    }
    return nuc;
}
       

CBioseq_Handle GetNucBioseq (CBioseq_Handle bioseq)
{
    CBioseq_Handle nuc;

    if (bioseq.IsNucleotide()) {
        return bioseq;
    }
    CSeq_entry_Handle parent = bioseq.GetParentEntry();
    if (parent && (parent = parent.GetParentEntry())
        && parent.IsSet()) {
        nuc = GetNucBioseq (parent.GetSet());
    }
    return nuc;
}


EAccessionFormatError ValidateAccessionString (string accession, bool require_version)
{
    if (NStr::IsBlank (accession)) {
        return eAccessionFormat_null;
    } else if (accession.length() >= 16) {
        return eAccessionFormat_too_long;
    } else if (accession.length() < 3 
               || ! isalpha (accession.c_str()[0]) 
               || ! isupper (accession.c_str()[0])
               || ! isalpha (accession.c_str()[1])
               || ! isupper (accession.c_str()[1])) {
        return eAccessionFormat_no_start_letters;
    }
    
    string str = accession;
    if (NStr::StartsWith (str, "NZ_")) {
        str = str.substr(3);
    }
    
    const char *cp = str.c_str();
    int numAlpha = 0;

    while (isalpha (*cp)) {
        numAlpha++;
        cp++;
    }

    int numUndersc = 0;

    while (*cp == '_') {
        numUndersc++;
        cp++;
    }

    int numDigits = 0;
    while (isdigit (*cp)) {
        numDigits++;
        cp++;
    }

    if ((*cp != '\0' && *cp != ' ' && *cp != '.') || numUndersc > 1) {
        return eAccessionFormat_wrong_number_of_digits;
    }

    EAccessionFormatError rval = eAccessionFormat_valid;

    if (require_version) {
        if (*cp != '.') {
            rval = eAccessionFormat_missing_version;
        }
        cp++;
        int numVersion = 0;
        while (isdigit (*cp)) {
            numVersion++;
            cp++;
        }
        if (numVersion < 1) {
            rval = eAccessionFormat_missing_version;
        } else if (*cp != '\0' && *cp != ' ') {
            rval = eAccessionFormat_bad_version;
        }
    }


    if (numUndersc == 0) {
        if ((numAlpha == 1 && numDigits == 5) 
            || (numAlpha == 2 && numDigits == 6)
            || (numAlpha == 3 && numDigits == 5)
            || (numAlpha == 4 && numDigits == 8)
            || (numAlpha == 5 && numDigits == 7)) {
            return rval;
        } 
    } else if (numUndersc == 1) {
        if (numAlpha != 2 || (numDigits != 6 && numDigits != 8 && numDigits != 9)) {
            return eAccessionFormat_wrong_number_of_digits;
        }
        char first_letter = accession.c_str()[0];
        char second_letter = accession.c_str()[1];
        if (first_letter == 'N' || first_letter == 'X' || first_letter == 'Z') { 
            if (second_letter == 'M' || second_letter == 'C'
                || second_letter == 'T' || second_letter == 'P'
                || second_letter == 'G' || second_letter == 'R'
                || second_letter == 'S' || second_letter == 'W'
                || second_letter == 'W' || second_letter == 'Z') {
                return rval;
            }
        }
        if ((first_letter == 'A' || first_letter == 'Y')
            && second_letter == 'P') {
            return rval;
        }
    }

    return eAccessionFormat_wrong_number_of_digits;
}


bool s_FeatureIdsMatch (const CFeat_id& f1, const CFeat_id& f2)
{
    if (!f1.IsLocal() || !f2.IsLocal()) {
        return false;
    } else if (f1.GetLocal().IsStr() && f2.GetLocal().IsStr()
               && NStr::EqualNocase(f1.GetLocal().GetStr(), f2.GetLocal().GetStr())) {
        return true;
    } else if (f1.GetLocal().IsId() && f2.GetLocal().IsId()
               && f1.GetLocal().GetId() == f2.GetLocal().GetId()) {
        return true;
    } else {
        return false;
    }
}


bool s_StringHasPMID (string str)
{
    if (NStr::IsBlank (str)) {
        return false;
    }

    size_t pos = NStr::Find (str, "(PMID ");
    if (pos == string::npos) {
        return false;
    }

    const char *ptr = str.c_str() + pos + 6;
    unsigned int numdigits = 0;
    while (*ptr != 0 && *ptr != ')') {
        if (isdigit (*ptr)) {
            numdigits++;
        }
        ptr++;
    }

    if (*ptr == ')' && numdigits > 0) {
        return true;
    } else {
        return false;
    }
}


bool HasBadCharacter (string str)
{
    if (NStr::Find (str, "?") != string::npos
        || NStr::Find (str, "!") != string::npos
        || NStr::Find (str, "~") != string::npos) {
        return true;
    } else {
        return false;
    }
}


bool EndsWithBadCharacter (string str)
{
    if (NStr::EndsWith (str, "_") || NStr::EndsWith (str, ".") 
        || NStr::EndsWith (str, ",") || NStr::EndsWith (str, ":")
        || NStr::EndsWith (str, ";")) {
        return true;
    } else {
        return false;
    }
}


bool IsBioseqWithIdInSet (const CSeq_id& id, CBioseq_set_Handle set)
{
    bool found = false;
    for (CBioseq_CI b_ci (set);
         b_ci && !found;
         ++b_ci) {
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(b_ci->GetCompleteBioseq())) {
            if (id.Compare(**id_it) == CSeq_id::e_YES) {
                found = true;
                break;
            }
        }
    }
    return found;
}


int CheckDate (const CDate& date, bool require_full_date)
{
    int rval = eDateValid_valid;

    if (date.IsStr()) {
        if (NStr::IsBlank (date.GetStr()) || NStr::Equal (date.GetStr(), "?")) {
            rval |= eDateValid_bad_str;
        }
    } else if (date.IsStd()) {
        if (!date.GetStd().IsSetYear() || date.GetStd().GetYear() == 0) {
            rval |= eDateValid_bad_year;
        }
        if (date.GetStd().IsSetMonth() && date.GetStd().GetMonth() > 12) {
            rval |= eDateValid_bad_month;
        }
        if (date.GetStd().IsSetDay() && date.GetStd().GetDay() > 31) {
            rval |= eDateValid_bad_day;
        }
        if (require_full_date) {
            if (!date.GetStd().IsSetMonth() || date.GetStd().GetMonth() == 0) {
                rval |= eDateValid_bad_month;
            }
            if (!date.GetStd().IsSetDay() || date.GetStd().GetDay() == 0) {
                rval |= eDateValid_bad_day;
            }
        }
        if (date.GetStd().IsSetSeason() && !NStr::IsBlank (date.GetStd().GetSeason())) {
            const char * cp = date.GetStd().GetSeason().c_str();
            while (*cp != 0) {
                if (isalpha (*cp) || *cp == '-') {
                    // these are the only acceptable characters
                } else {
                    rval |= eDateValid_bad_season;
                    break;
                }
                ++cp;
            }
        }
    } else {
        rval |= eDateValid_bad_other;
    }
    return rval;
}


string GetDateErrorDescription (int flags)
{
    string reasons = "";

    if (flags & eDateValid_empty_date) {
        reasons += "EMPTY_DATE ";
    }
    if (flags & eDateValid_bad_str) {
        reasons += "BAD_STR ";
    }
    if (flags & eDateValid_bad_year) {
        reasons += "BAD_YEAR ";
    }
    if (flags & eDateValid_bad_month) {
        reasons += "BAD_MONTH ";
    }
    if (flags & eDateValid_bad_day) {
        reasons += "BAD_DAY ";
    }
    if (flags & eDateValid_bad_season) {
        reasons += "BAD_SEASON ";
    }
    if (flags & eDateValid_bad_other) {
        reasons += "BAD_OTHER ";
    }
    return reasons;
}


bool IsBioseqTSA (const CBioseq& seq, CScope* scope) 
{
    if (!scope) {
        return false;
    }
    bool is_tsa = false;
    CBioseq_Handle bsh = scope->GetBioseqHandle(seq);
    if (bsh) {
        CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_Molinfo);
        while (desc_ci && !is_tsa) {
            if (desc_ci->GetMolinfo().IsSetTech() && desc_ci->GetMolinfo().GetTech() == CMolInfo::eTech_tsa) {
                is_tsa = true;
            }
            ++desc_ci;
        }
    }
    return is_tsa;
}


static string s_GetAuthorsString (const CAuth_list& auth_list)
{
    string auth_str = "";

    if (!auth_list.IsSetNames()) {
        return auth_str;
    }

    vector<string> name_list;

    if (auth_list.GetNames().IsStd()) {
        ITERATE (CAuth_list::TNames::TStd, auth_it, auth_list.GetNames().GetStd()) {
            if ((*auth_it)->IsSetName()) {
                string label;
                (*auth_it)->GetName().GetLabel(&label);
                name_list.push_back(label);
            }
        }
    } else if (auth_list.GetNames().IsMl()) {
        ITERATE (CAuth_list::TNames::TMl, auth_it, auth_list.GetNames().GetMl()) {
            name_list.push_back((*auth_it));
        }
    } else if (auth_list.GetNames().IsStr()) {
        ITERATE (CAuth_list::TNames::TStr, auth_it, auth_list.GetNames().GetStr()) {
            name_list.push_back((*auth_it));
        }
    }

    auth_str = name_list.back();
    name_list.pop_back();
    if (name_list.size() > 0) {
        auth_str = "and " + auth_str;
        if (name_list.size() == 1) {
            auth_str = name_list.front() + auth_str;
        } else {        
            while (name_list.size() > 0) {
                string this_name = name_list.back();
                name_list.pop_back();
                auth_str = this_name + ", " + auth_str;
            }
        }
    }

    return auth_str;
}


static string s_GetAuthorsString (const CPubdesc& pd) 
{
    string authors_string = "";

    FOR_EACH_PUB_ON_PUBDESC (pub, pd) {
        if ((*pub)->IsSetAuthors()) {
            authors_string = s_GetAuthorsString ((*pub)->GetAuthors());
            break;
        }
    }
    return authors_string;
}


void GetPubdescLabels 
(const CPubdesc& pd, 
 vector<int>& pmids, vector<int>& muids, vector<int>& serials,
 vector<string>& published_labels, vector<string>& unpublished_labels)
{
    string label = "";
    bool   is_published = false;
    bool   need_label = false;
    FOR_EACH_PUB_ON_PUBDESC (it, pd) {
        if ((*it)->IsPmid()) {
            pmids.push_back ((*it)->GetPmid());
            is_published = true;
        } else if ((*it)->IsMuid()) {
            muids.push_back ((*it)->GetMuid());
            is_published = true;
        } else if ((*it)->IsGen()) {
            if ((*it)->GetGen().IsSetCit() 
                && NStr::StartsWith ((*it)->GetGen().GetCit(), "BackBone id_pub", NStr::eNocase)) {
                need_label = true;
            }
            if ((*it)->GetGen().IsSetSerial_number()) {
                serials.push_back ((*it)->GetGen().GetSerial_number());
                if ((*it)->GetGen().IsSetCit() 
                    || (*it)->GetGen().IsSetJournal()
                    || (*it)->GetGen().IsSetDate()) {
                    need_label = true;
                }
            } else {
                need_label = true;
            }
        } else {
            need_label = true;
        }
        if (need_label && NStr::IsBlank(label)) {
            // create unique label
            (*it)->GetLabel(&label, CPub::eContent, true);
            label += "; " + s_GetAuthorsString (pd);
        }
    }
    if (!NStr::IsBlank(label)) {
        if (is_published) {
            published_labels.push_back(label);
        } else {
            unpublished_labels.push_back(label);
        }
    }
}


bool IsNCBIFILESeqId (const CSeq_id& id)
{
    if (!id.IsGeneral() || !id.GetGeneral().IsSetDb()
        || !NStr::Equal(id.GetGeneral().GetDb(), "NCBIFILE")) {
        return false;
    } else {
        return true;
    }
}


bool IsRefGeneTrackingObject (const CUser_object& user)
{
    bool rval = false;

    if (user.IsSetType()) {
        const CObject_id& oi = user.GetType();
	    if (oi.IsStr() && NStr::EqualNocase(oi.GetStr(), "RefGeneTracking")) {
            rval = true;
        }
    }
    return rval;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
