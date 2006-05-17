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
 *   Implementation of BasicCleanup for GBQuals in Seq-feat's.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <util/static_map.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/seqport_util.hpp>
#include <vector>

#include <objmgr/util/seq_loc_util.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// seq-feat.qual


bool CCleanup_imp::BasicCleanup(CGene_ref& gene, const CGb_qual& gb_qual)
{
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();

    bool retval = false;
    if (NStr::EqualNocase(qual, "map")) {
        if (! (gene.IsSetMaploc()  ||  NStr::IsBlank(val)) ) {
            retval = true;
            gene.SetMaploc(val);
        }
    } else if (NStr::EqualNocase(qual, "allele")) {
        if ( ! (gene.IsSetAllele()  ||  NStr::IsBlank(val)) ) {
            retval = true;
            gene.SetAllele(val);
        }
    } else if (NStr::EqualNocase(qual, "locus_tag")) {
        if ( ! (gene.IsSetLocus_tag()  ||  NStr::IsBlank(val)) ) {
            retval = true;
            gene.SetLocus_tag(val);
        }
    } else if (NStr::EqualNocase(qual, "gene")  &&  ! NStr::IsBlank(val)) {
        retval = true;
        if ( ! gene.IsSetLocus() ) {
            gene.SetLocus(val);
        } else if (gene.GetLocus() != val) {
            CGene_ref::TSyn::const_iterator syn_it = 
                find(gene.GetSyn().begin(), gene.GetSyn().end(), val);
            if (syn_it == gene.GetSyn().end()) {
                gene.SetSyn().push_back(val);
            }            
        }
    }

    return retval;
}


bool CCleanup_imp::x_ParseCodeBreak(CSeq_feat& feat, CCdregion& cds, string str)
{
    unsigned int aa_pos = NStr::Find(str, "aa:");
    unsigned int len = 0;
    unsigned int loc_pos, end_pos;
    char protein_letter = 'X';
    CRef<CSeq_loc> break_loc;
    
    if (aa_pos == string::npos) {
        aa_pos = NStr::Find (str, ",");
        if (aa_pos != string::npos) {
            aa_pos = NStr::Find (str, ":", aa_pos);
        }
        if (aa_pos != string::npos) {
            aa_pos ++;
        }
    } else {
        aa_pos += 3;
    }

    if (aa_pos != string::npos) {    
        while (aa_pos < str.length() && isspace (str.c_str()[aa_pos])) {
            aa_pos++;
        }
        while (aa_pos + len < str.length() && isalpha (str.c_str()[aa_pos + len])) {
            len++;
        }
        if (len != 0) {    
            protein_letter = ValidAminoAcid(str.substr(aa_pos, len));
        }
    }
    
    loc_pos = NStr::Find (str, "(pos:");
    if (loc_pos == string::npos) {
        return false;
    }
    loc_pos += 5;
    while (loc_pos < str.length() && isspace (str.c_str()[loc_pos])) {
        loc_pos++;
    }
    end_pos = NStr::Find (str, ",", loc_pos);
    if (end_pos == string::npos) {
        break_loc = ReadLocFromText (str.substr(loc_pos), feat.GetLocation().GetId(), m_Scope);
    } else {
        break_loc = ReadLocFromText (str.substr(loc_pos, end_pos - loc_pos), feat.GetLocation().GetId(), m_Scope);
    }
    
    if (break_loc == NULL 
        || sequence::Compare (*break_loc, feat.GetLocation(), m_Scope) != sequence::eContained
        || (break_loc->IsInt() && sequence::GetLength(*break_loc, m_Scope) != 3)) {
        return false;
    }
    
    // need to build code break object and add it to coding region
    CRef<CCode_break> m_NewCodeBreak(new CCode_break());
    CCode_break::TAa& aa = m_NewCodeBreak->SetAa();
    aa.SetNcbieaa(protein_letter);
    m_NewCodeBreak->SetLoc (*break_loc);

    CCdregion::TCode_break& orig_list = cds.SetCode_break();
    orig_list.push_back(m_NewCodeBreak);
    
    
    return true;
    
}


bool CCleanup_imp::BasicCleanup(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual)
{
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();
    
    // transl_except qual -> Cdregion.code_break
    if (NStr::EqualNocase(qual, "transl_except")) {
        
        return x_ParseCodeBreak(feat, cds, val);
    }

    // codon_start qual -> Cdregion.frame
    if (NStr::EqualNocase(qual, "codon_start")) {
        CCdregion::TFrame frame = cds.GetFrame();
        CCdregion::TFrame new_frame = CCdregion::TFrame(NStr::StringToNumeric(val));
        if (new_frame == CCdregion::eFrame_one  ||
            new_frame == CCdregion::eFrame_two  ||
            new_frame == CCdregion::eFrame_three) {
            if (frame == CCdregion::eFrame_not_set  ||
                (feat.IsSetPseudo()  &&  feat.GetPseudo()  &&  !feat.IsSetProduct())) {
                cds.SetFrame(new_frame);
            }
            return true;
        }
    }

    // transl_table qual -> Cdregion.code
    if (NStr::EqualNocase(qual, "transl_table")) {
        if (cds.IsSetCode()) {
            const CCdregion::TCode& code = cds.GetCode();
            int transl_table = 1;
            ITERATE (CCdregion::TCode::Tdata, it, code.Get()) {
                if ((*it)->IsId()  &&  (*it)->GetId() != 0) {
                    transl_table = (*it)->GetId();
                    break;
                }
            }
            
            if (NStr::EqualNocase(NStr::UIntToString(transl_table), val)) {
                return true;
            }
        } else {
            int new_val = NStr::StringToNumeric(val);
            if (new_val > 0) {
                CRef<CGenetic_code::C_E> gc(new CGenetic_code::C_E);
                gc->SetId(new_val);
                cds.SetCode().Set().push_back(gc);
                return true;
            }
        }
    }

    /*if (NStr::EqualNocase(qual, "translation")) {
        return TRUE;
    }*/
    return false;
}


bool CCleanup_imp::BasicCleanup(CSeq_feat& feat, CRNA_ref& rna, const CGb_qual& gb_qual)
{
    const string& qual = gb_qual.GetQual();

    bool is_std_name = NStr::EqualNocase(qual, "standard_name");
    if (NStr::EqualNocase(qual, "product")  ||  (is_std_name  &&  ! (m_Mode == eCleanup_EMBL  ||  m_Mode == eCleanup_DDBJ) )) {
        if (rna.IsSetType()) {
            if (rna.GetType() == CRNA_ref::eType_unknown) {
                rna.SetType(CRNA_ref::eType_other);
            }
        } else {
            rna.SetType(CRNA_ref::eType_other);
        }
        _ASSERT(rna.IsSetType());

        CRNA_ref::TType type = rna.GetType();
        
        if (type == CRNA_ref::eType_other  &&  is_std_name) {
            return false;
        }

        if (type == CRNA_ref::eType_tRNA  &&  rna.IsSetExt()  &&  rna.GetExt().IsName()) {
            //!!! const string& name = rna.GetExt().GetName();
        }
    }
    if (NStr::EqualNocase(qual, "anticodon")) {
        if (!rna.IsSetType()) {
            rna.SetType(CRNA_ref::eType_tRNA);
        }
        _ASSERT(rna.IsSetType());
        CRNA_ref::TType type = rna.GetType();
        if (type == CRNA_ref::eType_unknown) {
            rna.SetType(CRNA_ref::eType_tRNA);
        } else if (type != CRNA_ref::eType_tRNA) {
            return false;
        }
        if ( rna.CanGetExt()  &&
             rna.GetExt().Which() == CRNA_ref::C_Ext::e_TRNA ) {
            
            CRef<CSeq_loc> anticodon = ReadLocFromText (gb_qual.GetVal(), feat.GetLocation().GetId(), m_Scope);
            if (anticodon != NULL) {
                rna.SetExt().SetTRNA().SetAnticodon(*anticodon);
                return true;
            }
        }
    }
    return false;
}


bool CCleanup_imp::BasicCleanup(CProt_ref& prot, const CGb_qual& gb_qual)
{    
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();

    if (NStr::EqualNocase(qual, "product")  ||  NStr::EqualNocase(qual, "standard_name")) {
        if (!prot.IsSetName()  ||  NStr::IsBlank(prot.GetName().front())) {
            prot.SetName().push_back(val);
            if (prot.IsSetDesc()) {
                const CProt_ref::TDesc& desc = prot.GetDesc();
                ITERATE (CProt_ref::TName, it, prot.GetName()) {
                    if (NStr::EqualNocase(desc, *it)) {
                        prot.ResetDesc();
                        break;
                    }
                }
            }
            return true;
        }
    } else if (NStr::EqualNocase(qual, "function")) {
        prot.SetActivity().push_back(val);
        return true;
    } else if (NStr::EqualNocase(qual, "EC_number")) {
        prot.SetEc().push_back(val);
        return true;
    }

    return false;
}


static bool s_IsCompoundRptTypeValue( 
    const string& value )
//
//  Format of compound rpt_type values: (value[,value]*)
//
//  These are internal to sequin and are in theory cleaned up before the material
//  is released. However, some compound values have escaped into the wild and have 
//  not been retro-fixed yet (as of 2006-03-17).
//
{
    return ( NStr::StartsWith( value, '(' ) && NStr::EndsWith( value, ')' ) );
};


static void s_ExpandThisQual( 
    CSeq_feat::TQual& quals,        // the list of CGb_qual's.
    CSeq_feat::TQual::iterator& it, // points to the one qual we might expand.
    CSeq_feat::TQual new_quals )    // new quals that will need to be inserted
//
//  Rules for "rpt_type" qualifiers (as of 2006-03-07):
//
//  There can be multiple occurrences of this qualifier, and we need to keep them 
//  all.
//  The value of this qualifier can also be a *list of values* which is *not* 
//  conforming to the ASN.1 and thus needs to be cleaned up. 
//
//  The cleanup entails turning the list of values into multiple occurrences of the 
//  given qualifier, each occurrence taking one of the values in the original 
//  list.
//
{
    CGb_qual& qual = **it;
    string  qual_type = qual.GetQual();
    string& val = qual.SetVal();
    if ( ! s_IsCompoundRptTypeValue( val ) ) {
        //
        //  nothing to do ...
        //
        return;
    }

    //
    //  Generate list of cleaned up values. Fix original qualifier and generate 
    //  list of new qualifiers to be added to the original list:
    //    
    vector< string > newValues;
    string valueList = val.substr(1, val.length() - 2);
    NStr::Tokenize(valueList, ",", newValues);
    
    qual.SetVal( newValues[0] );
    
    for ( size_t i=1; i < newValues.size(); ++i ) {
        CRef< CGb_qual > newQual( new CGb_qual() );
        newQual->SetQual( qual_type );
        newQual->SetVal( newValues[i] );
        new_quals.push_back( newQual ); 
    }
};

void CCleanup_imp::x_ExpandCombinedQuals(CSeq_feat::TQual& quals)
{
    CSeq_feat::TQual    new_quals;
    NON_CONST_ITERATE (CSeq_feat::TQual, it, quals) {
        CGb_qual& gb_qual = **it;
                
        string& qual = gb_qual.SetQual();
        
        if (NStr::EqualNocase(qual, "rpt_type")) {            
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "rpt_unit")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "usedin")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "old_locus_tag")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "compare")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        }
    }
    
    if ( ! new_quals.empty() ) {
        quals.insert(quals.end(), new_quals.begin(), new_quals.end());
    }
}



bool CCleanup_imp::BasicCleanup(CSeq_feat& feat, CGb_qual& gb_qual)
{
    // return true if we should delete this gb_qual.
    
    _ASSERT(feat.IsSetQual()  &&  feat.IsSetData());

    CSeq_feat::TData& data = feat.SetData();
    string& qual = gb_qual.SetQual();
    string& val  = gb_qual.SetVal();

    // 'replace' qualifier
    if (NStr::EqualNocase(qual, "replace")) {
        if (data.IsImp()  &&  data.GetImp().IsSetKey()) {
            CSeq_feat::TData::TImp& imp = feat.SetData().SetImp();
            if (NStr::EqualNocase(imp.GetKey(), "variation")) {
                NStr::ToLower(val);
            }
        }
    }

    if (NStr::EqualNocase(qual, "partial")) {
        feat.SetPartial();
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "evidence")) {
        if (NStr::EqualNocase(val, "experimental")) {
            if (!feat.IsSetExp_ev()  ||  feat.GetExp_ev() != CSeq_feat::eExp_ev_not_experimental) {
                feat.SetExp_ev(CSeq_feat::eExp_ev_experimental);
            }
        } else if (NStr::EqualNocase(val, "not_experimental")) {
            feat.SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
        }
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "exception")) {
        feat.SetExcept(true);
        if (!NStr::IsBlank(val)  &&  !NStr::EqualNocase(val, "true")) {
            if (!feat.IsSetExcept_text()) {
                feat.SetExcept_text(val);
            }
        }
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "note")) {
        if (!feat.IsSetComment()) {
            feat.SetComment(val);
        } else {
            (feat.SetComment() += "; ") += val;
        }
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "db_xref")) {
        string tag, db;
        if (NStr::SplitInTwo(val, ":", db, tag)) {
            CRef<CDbtag> dbp(new CDbtag);
            dbp->SetDb(db);
            dbp->SetTag().SetStr(tag);
            feat.SetDbxref().push_back(dbp);
            return true;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "gdb_xref")) {
        CRef<CDbtag> dbp(new CDbtag);
        dbp->SetDb("GDB");
        dbp->SetTag().SetStr(val);
        feat.SetDbxref().push_back(dbp);
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "pseudo")) {
        feat.SetPseudo(true);
        return true;  // mark qual for deletion
    } else if (data.IsGene()  &&  BasicCleanup(data.SetGene(), gb_qual)) {
        return true;  // mark qual for deletion
    } else if (data.IsCdregion()  &&  BasicCleanup(feat, data.SetCdregion(), gb_qual) ) {
        return true;  // mark qual for deletion
    } else if (data.IsRna()  &&  BasicCleanup(feat, data.SetRna(), gb_qual)) {
        return true;  // mark qual for deletion
    } else if (data.IsProt()  &&  BasicCleanup(data.SetProt(), gb_qual)) {
        return true;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "gene")) {
        if (!NStr::IsBlank(val)) {
            CRef<CSeqFeatXref> xref(new CSeqFeatXref);
            xref->SetData().SetGene().SetLocus(val);
            feat.SetXref().push_back(xref);
            return true;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "codon_start")) {
        if (!data.IsCdregion()) {
            // not legal on anything but CDS, so remove it
            return true;  // mark qual for deletion
        }
    }

    return false;
}


static bool s_IsJustQuotes(const string& str)
{
    ITERATE (string, it, str) {
        if ((*it > ' ')  &&  (*it != '"')  &&  (*it  != '\'')) {
            return false;
        }
    }
    return true;
}


void CCleanup_imp::BasicCleanup(CGb_qual& gbq)
{
    CLEAN_STRING_MEMBER(gbq, Qual);
    if (!gbq.IsSetQual()) {
        gbq.SetQual(kEmptyStr);
    }
    
    CLEAN_STRING_MEMBER(gbq, Val);
    if (gbq.IsSetVal()  &&  s_IsJustQuotes(gbq.GetVal())) {
        gbq.ResetVal();
    }
    if (!gbq.IsSetVal()) {
        gbq.SetVal(kEmptyStr);
    }
    _ASSERT(gbq.IsSetQual()  &&  gbq.IsSetVal());
    
    if (NStr::EqualNocase(gbq.GetQual(), "cons_splice")) {
        x_CleanupConsSplice(gbq);
    } else if (NStr::EqualNocase(gbq.GetQual(), "rpt_unit")) {
        x_CleanupRptUnit(gbq);
    }
}



// Gb_qual cleanup

void CCleanup_imp::x_CleanupConsSplice(CGb_qual& gbq)

{
    string& val = gbq.SetVal();
    
    if (!NStr::StartsWith(val, "(5'site:")) {
        return;
    }
    
    size_t pos = val.find(",3'site:");
    if (pos != NPOS) {
        val.insert(pos + 1, " ");
    }
}


void CCleanup_imp::x_CleanupRptUnit(CGb_qual& gbq)
{
    CGb_qual::TVal& val = gbq.SetVal();
    
    if (NStr::IsBlank(val)) {
        return;
    }
    string s;
    string::const_iterator it = val.begin();
    string::const_iterator end = val.end();
    while (it != end) {
        while (it != end  &&  (*it == '('  ||  *it == ')'  ||  *it == ',')) {
            s += *it++;
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        while (it != end  &&  isdigit((unsigned char)(*it))) {
            s += *it++;
        }
        if (it != end  &&  (*it == '.'  ||  *it == '-')) {
            while (it != end  &&  (*it == '.'  ||  *it == '-')) {
                ++it;
            }
            s += "..";
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        while (it != end  &&  isdigit((unsigned char)(*it))) {
            s += *it++;
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        if (it != end) {
            char c = *it;
            if (c != '('  &&  c != ')'  &&  c != ','  &&  c != '.'  &&
                !isspace((unsigned char) c)  &&  !isdigit((unsigned char) c)) {
                NStr::ToLower(val);
                return;
            }
        }
    }
    val = s;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.4  2006/04/27 18:24:34  rsmith
 * get rid of unused variable in BasicCleanup(rna, gb_qual)
 *
 * Revision 1.3  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.2  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.1  2006/03/29 16:32:23  rsmith
 * Initial commit.
 *
 * Revision 1.3  2006/03/23 18:33:32  rsmith
 * Separate straight BasicCleanup of SFData and GBQuals from more complicated
 * changes to various parts of the Seq_feat.
 *
 * Revision 1.2  2006/03/20 14:21:25  rsmith
 * move Biosource related cleanup to its own file.
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */
