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
* Author:  Igor Filippov
*
* File Description:
*   Simple library to correct reference allele in Variation object
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <iostream>
#include <algorithm>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/streamiter.hpp>
#include <util/compress/stream_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <util/line_reader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/rwstream.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/variation/variation_utils.hpp>

bool x_CheckForFirstNTSeq(const CVariation_inst& inst)
{
    if(    inst.IsSetDelta() 
        && !inst.GetDelta().empty() 
        && inst.GetDelta().front()->IsSetSeq() 
        && inst.GetDelta().front()->GetSeq().IsLiteral()
        && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
        && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna() 
        )
        return true;

    return false;
}

void CVariationUtilities::CorrectRefAllele(CRef<CVariation>& v, CScope& scope)
{
    string old_ref;
    string new_ref;

    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it, v->SetData().SetSet().SetVariations()) {
        CVariation& var = **var_it;
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it2, var.SetData().SetSet().SetVariations()) {
            CVariation& var2 = **var_it2;
            if(!var2.IsSetData() || !var2.GetData().IsInstance())
                continue;

            CVariation_inst& inst = var2.SetData().SetInstance();
            if(x_CheckForFirstNTSeq(inst) && inst.GetType() == CVariation_inst::eType_identity) 
            {
                old_ref = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
                new_ref = GetRefAlleleFromVP(v->SetData().SetSet().SetVariations().front()->SetPlacements().front(), scope, old_ref.length());
                inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(new_ref);
            }
        }
    }

    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(new_ref);
    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetLength(new_ref.length());
    FixAlleles(v,old_ref,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CRef<CSeq_annot>& annot, CScope& scope)
{
    if (!annot->IsSetData() || !annot->GetData().IsFtable())
        NCBI_THROW(CException, eUnknown, "Ftable is not set in input Seq-annot");
    for (CSeq_annot::TData::TFtable::iterator feat = annot->SetData().SetFtable().begin(); feat != annot->SetData().SetFtable().end(); ++feat)
    {
        CVariation_ref& vr = (*feat)->SetData().SetVariation();
        string new_ref = GetAlleleFromLoc((*feat)->SetLocation(),scope);
        CorrectRefAllele(vr,new_ref);
    }
}


void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, const CSeq_loc& loc, CScope& scope)
{
    string new_ref = GetAlleleFromLoc(loc,scope);
    CorrectRefAllele(vr,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, const string& new_ref)
{
    string old_ref;
    if (!vr.IsSetData() || !vr.GetData().IsSet() || !vr.GetData().GetSet().IsSetVariations())
        NCBI_THROW(CException, eUnknown, "Set of Variation-inst is not set in input Seq-annot");

    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vr.SetData().SetSet().SetVariations()) {
        CVariation_ref& vref = **vref_it;
        if(!vref.IsSetData() || !vref.GetData().IsInstance())
            continue;

        CVariation_inst& inst = vref.SetData().SetInstance();
        if(x_CheckForFirstNTSeq(inst) && inst.GetType() == CVariation_inst::eType_identity) {
            old_ref  = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
            inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(new_ref);
        }
    }
    if (old_ref.empty())
        NCBI_THROW(CException, eUnknown, "Old reference allele not found in input Seq-annot");

    FixAlleles(vr,old_ref,new_ref);
}

void CVariationUtilities::FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) 
{
    if (old_ref == new_ref) return;
    CVariation_inst::TType type = CVariation_inst::eType_snv;
    bool add_old_ref = true;
    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vr.SetData().SetSet().SetVariations()) {
        CVariation_ref& vref = **vref_it;
        if(!vref.IsSetData() || !vref.GetData().IsInstance())
            continue;

        CVariation_inst& inst = vref.SetData().SetInstance();
        if(x_CheckForFirstNTSeq(inst) && inst.GetType() != CVariation_inst::eType_identity) {
            string allele = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
            type = inst.SetType();
            if (old_ref  == allele) {
                add_old_ref = false;
            }
            if (new_ref == allele)
            {
                inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(old_ref);
                add_old_ref = false;
            }
        }

    }
    if (add_old_ref)
    {
        CRef<CVariation_inst> inst(new CVariation_inst);
        CRef<CDelta_item> item(new CDelta_item);
        inst->SetType(type);
        CSeq_literal literal;
        literal.SetLength(old_ref.size());
        CSeq_data data;
        data.SetIupacna().Set(old_ref);
        literal.SetSeq_data().Assign(data);
        item->SetSeq().SetLiteral().Assign(literal);
        inst->SetDelta().push_back(item);
        CRef<CVariation_ref> leaf(new CVariation_ref);
        leaf->SetData().SetInstance().Assign(*inst);
        vr.SetData().SetSet().SetVariations().push_back(leaf);
    }
}

string CVariationUtilities::GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length) 
{
    string new_ref;
    if(   (vp->IsSetStart_offset() && vp->GetStart_offset() != 0)
          || (vp->IsSetStop_offset()  && vp->GetStop_offset()  != 0))
        NCBI_THROW(CException, eUnknown, "Can't get sequence for an offset-based location");
    else if(length > MAX_LEN) 
        NCBI_THROW(CException, eUnknown, "Sequence is longer than the cutoff threshold");
     else 
     {
         new_ref = GetAlleleFromLoc(vp->GetLoc(),scope);
         if (new_ref.empty())
             NCBI_THROW(CException, eUnknown, "Empty residue in reference");
         for (unsigned int i=0; i<new_ref.size();i++)
             if(new_ref[i] != 'A' && new_ref[i] != 'C' && new_ref[i] != 'G' && new_ref[i] != 'T') 
                 NCBI_THROW(CException, eUnknown, "Ambiguous residues in reference");
     }
    return new_ref;
}

string CVariationUtilities::GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope)
{
    string new_ref;
    if(sequence::GetLength(loc, NULL) > 0) 
    {
        try 
            {
                CSeqVector v(loc, scope, CBioseq_Handle::eCoding_Iupac);
                if(v.IsProtein()) 
                    NCBI_THROW(CException, eUnknown, "Not an NA sequence");
                v.GetSeqData(v.begin(), v.end(), new_ref);
            } catch(CException& e) 
            {
                string loc_label;   
                loc.GetLabel(&loc_label);
                NCBI_RETHROW_SAME(e, "Can't get literal for " + loc_label);
            }
    }
    return new_ref;
}
 

void CVariationUtilities::FixAlleles(CRef<CVariation> v, string old_ref, string new_ref) 
{
    if (old_ref == new_ref) return;
    ERR_POST(Trace << "old: " << old_ref << " new: " << new_ref);
    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it, v->SetData().SetSet().SetVariations()) {
        bool add_old_ref = true;
        CVariation_inst::TType type = CVariation_inst::eType_snv;

        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it2, (*var_it)->SetData().SetSet().SetVariations()) {
            CVariation& var2 = **var_it2;
            if(!var2.IsSetData() || !var2.GetData().IsInstance())
                continue;

            CVariation_inst& inst = var2.SetData().SetInstance();
            if(x_CheckForFirstNTSeq(inst) && inst.GetType() != CVariation_inst::eType_identity) {

                string allele = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();

                ERR_POST(Trace << "And the allele is: " << allele);

                type = inst.SetType();
                if (old_ref  == allele) {
                    add_old_ref = false;
                }
                if (new_ref == allele)
                {
                    inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(old_ref);
                    add_old_ref = false;
                }
            }
        }

        if (add_old_ref)
        {
            CRef<CVariation_inst> inst(new CVariation_inst);
            CRef<CDelta_item> item(new CDelta_item);
            inst->SetType(type);
            CSeq_literal literal;
            literal.SetLength(old_ref.size());
            CSeq_data data;
            data.SetIupacna().Set(old_ref);
            literal.SetSeq_data().Assign(data);
            item->SetSeq().SetLiteral().Assign(literal);
            inst->SetDelta().push_back(item);
            CRef<CVariation> leaf(new CVariation);
            leaf->SetData().SetInstance().Assign(*inst);
            (*var_it)->SetData().SetSet().SetVariations().push_back(leaf);
        }
    }
}

bool CVariationUtilities::IsReferenceCorrect(const CSeq_feat& feat, string& wrong_ref, string& correct_ref, CScope& scope)
{
    wrong_ref.clear();
    correct_ref.clear();
    bool found_allele_in_inst = false;
    if (feat.IsSetData() && feat.GetData().IsVariation() && feat.IsSetLocation())
    {
        const CSeq_loc& loc = feat.GetLocation(); 
        correct_ref = GetAlleleFromLoc(loc,scope);
   
        const CVariation_ref& vr = feat.GetData().GetVariation();
        if (!vr.IsSetData() || !vr.GetData().IsSet() || !vr.GetData().GetSet().IsSetVariations())
            NCBI_THROW(CException, eUnknown, "Set of Variation-inst is not set in input Seq-feat");

        ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vr.GetData().GetSet().GetVariations()) {
            const CVariation_ref& vref = **vref_it;
            if(!vref.IsSetData() || !vref.GetData().IsInstance())
                continue;

            const CVariation_inst& inst = vref.GetData().GetInstance();
            if(x_CheckForFirstNTSeq(inst) && inst.GetType() == CVariation_inst::eType_identity) {
                wrong_ref  = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get(); 
                found_allele_in_inst = true;
            }
        }
        
    }
    else
        NCBI_THROW(CException, eUnknown, "Feature is not a Variation or Location is not set");
    
    if (correct_ref.empty())
        NCBI_THROW(CException, eUnknown, "Cannot access correct reference allele at given location");
    
    if (found_allele_in_inst && wrong_ref.empty())
        NCBI_THROW(CException, eUnknown, "Old reference allele not found in input Seq-feat");
    
    if (!found_allele_in_inst)
        return true;
    else
        return (wrong_ref == correct_ref);
}

CVariation_inst::TType
    CVariationUtilities::GetVariationType(const CVariation_ref& vr)
{
    if(!vr.IsSetData()) {
        return CVariation_inst::eType_unknown;
    }

    set<int> types;
    switch(vr.GetData().Which())
    {
    case  CVariation_ref::C_Data::e_Instance : return vr.GetData().GetInstance().GetType(); break;
    case  CVariation_ref::C_Data::e_Set : 
        ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vr.GetData().GetSet().GetVariations()) {
            const CVariation_ref& vref = **vref_it;
            if ( vref.IsSetData() 
                && vref.GetData().IsInstance() 
                && vref.GetData().GetInstance().GetType() != CVariation_inst::eType_identity)

                    types.insert( vref.GetData().GetInstance().GetType() );
        }
        break;
    default: break;            
    }

    if (types.empty())
        return  CVariation_inst::eType_unknown;
    if (types.size() == 1)
        return *types.begin();
    
    return  CVariation_inst::eType_other;
}

CVariation_inst::TType
    CVariationUtilities::GetVariationType(const CVariation& var)
{
    LOG_POST(Trace << MSerial_AsnText << var);
    if(!var.IsSetData()) {
        return CVariation_inst::eType_unknown;
    }

    set<CVariation_inst::TType> types;
    switch(var.GetData().Which()) {
    case CVariation::C_Data::e_Instance:
        types.insert(var.GetData().GetInstance().GetType());
        break;
    case CVariation::C_Data::e_Set:
        if(!var.GetData().GetSet().IsSetVariations())
            break;

        //Iterate through the alleles
        ITERATE(CVariation::TData::TSet::TVariations, var_it, var.GetData().GetSet().GetVariations()) {
            const CVariation& var2 = **var_it;
            CVariation_inst::TType type = GetVariationType(var2);
            if(type != CVariation_inst::eType_identity)
                types.insert(type);
        }
        break;
    default: 
        break;
    }

    if (types.empty())
        return  CVariation_inst::eType_unknown;
    if (types.size() == 1)
        return *types.begin();

    return  CVariation_inst::eType_other;
}

void CVariationUtilities::GetVariationRefAlt(const CVariation_ref& vr, string &ref,  vector<string> &alt)
{
    ref.clear();
    alt.clear();
    switch (vr.GetData().Which())
    {
    case  CVariation_Base::C_Data::e_Set :
        ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vr.GetData().GetSet().GetVariations()) {
            const CVariation_ref& vref = **vref_it;
            if(!vref.IsSetData() || !vref.GetData().IsInstance())
                continue;

            const CVariation_inst& inst = vref.GetData().GetInstance();
            if(x_CheckForFirstNTSeq(inst) ) {
                string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                if (!a.empty())
                {
                    if (inst.GetType() == CVariation_inst::eType_identity )
                        ref = a;
                    else if (inst.GetType() != CVariation_inst::eType_del) 
                        alt.push_back(a);
                }
            }
        }
        break;
    case  CVariation_Base::C_Data::e_Instance :
    {
        const CVariation_inst &inst = vr.GetData().GetInstance();
        const CVariation_inst::TType type = inst.GetType();
        if (x_CheckForFirstNTSeq(inst)) {
            string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
            if (!a.empty())
            {
                if (type == CVariation_inst::eType_identity )
                    ref = a;
                else if (type != CVariation_inst::eType_del) 
                    alt.push_back(a);
            }
        }
        break;
    }
    default : break;
    }           
}


void CVariationUtilities::GetVariationRefAlt(const CVariation& v, string &ref,  vector<string> &alt)
{
    ref.clear();
    alt.clear();
    switch (v.GetData().Which())
    {
    case  CVariation_Base::C_Data::e_Set :
        ITERATE(CVariation::TData::TSet::TVariations, var2, v.GetData().GetSet().GetVariations())
        {
            if ( (*var2)->IsSetData() && (*var2)->GetData().IsInstance())
            {
                const CVariation_inst &inst = (*var2)->GetData().GetInstance();
                const CVariation_inst::TType type = inst.GetType();
                if (x_CheckForFirstNTSeq(inst)) {
                    string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    if (!a.empty())
                    {
                        if (type == CVariation_inst::eType_identity )
                            ref = a;
                        else if (type != CVariation_inst::eType_del) 
                            alt.push_back(a);
                    }
                }
            }
        }
        break;
    case  CVariation_Base::C_Data::e_Instance :
        {
            const CVariation_inst &inst = v.GetData().GetInstance();
            const CVariation_inst::TType type = inst.GetType();
            if(x_CheckForFirstNTSeq(inst)) {
                string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                if (!a.empty())
                {
                    if (type == CVariation_inst::eType_identity )
                        ref = a;
                    else if (type != CVariation_inst::eType_del) 
                        alt.push_back(a);
                }
            }
            break;
        }
    default : break;
    }           
}


string CVariationUtilities::GetCommonRepeatUnit(const CVariation_ref& vr)
{
    string ref;
    vector<string> alts;
    set<string> crus;
    CVariationUtilities::GetVariationRefAlt(vr,ref,alts);
    ITERATE(vector<string>, alt_it, alts) {
        crus.insert(RepeatedSubstring(*alt_it));
    }
    if(crus.size() != 1)
        return kEmptyStr;
    return *crus.begin();
}


string CVariationUtilities::GetCommonRepeatUnit(const CVariation& v)
{
    string ref;
    vector<string> alts;
    set<string> crus;
    CVariationUtilities::GetVariationRefAlt(v,ref,alts);
    ITERATE(vector<string>, alt_it, alts) {
        crus.insert(RepeatedSubstring(*alt_it));
    }
    if(crus.size() != 1)
        return kEmptyStr;
    return *crus.begin();
}


bool CVariationUtilities::x_isBaseRepeatingUnit(
    const string& candidate, const string& target) 
{
    if(target.length() % candidate.length() != 0)
        return false;


    int factor = target.length() / candidate.length();
    string test_candidate;
    for(int jj=0; jj<factor ; jj++) {
        test_candidate += candidate;
    }

    if(test_candidate == target) 
        return true;
    return false;
}


string CVariationUtilities::RepeatedSubstring(const string &str) 
{
    for(vector<string>::size_type ii=1; ii <= str.length() / 2 ; ++ii) {

        if( str.length() % ii != 0 ) continue;
        string candidate = str.substr(0,ii);

        //If this canddate is a base repeating unit of the target str,
        //then this is what we are looking for
        if(x_isBaseRepeatingUnit(candidate, str))
            return candidate;
    }

    return str;
}


// Variation Normalization
CCache<string,CRef<CSeqVector> > CSeqVectorCache::m_cache(CCACHE_SIZE);


CRef<CSeqVector> CSeqVectorCache::PrefetchSequence(CScope &scope, const CSeq_id &seq_id, ENa_strand strand)
{
    string accession;
    seq_id.GetLabel(&accession);

    //this is the magical thread safe line that 
    // makes sure to get a reference count on the object in the cache
    // and now the actual object in memory can not be deleted
    // until that object's counter is down to zero.
    // and that will not happen until the caller's CRef is destroyed.
    LOG_POST(Trace << "Try to get from cache for accession: " << accession );
    CRef<CSeqVector> seqvec_ref = m_cache.Get(accession);
    LOG_POST(Trace << "Got CRef for acc : " << accession );
    if ( !seqvec_ref || seqvec_ref->empty() ) 
    {
        LOG_POST(Trace << "Acc was empty or null: " << accession );
        const CBioseq_Handle& bsh = scope.GetBioseqHandle( seq_id );
        LOG_POST(Trace << "Got BioseqHandle, now get SeqVecRef: " << accession );
        seqvec_ref = Ref(new CSeqVector(bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,strand)));
        LOG_POST(Trace << "Add it to the cache: " << accession );
        m_cache.Add(accession, seqvec_ref);
        LOG_POST(Trace << "Added to cache: " << accession );
    }
    LOG_POST(Trace << "Return the seqvec ref for : " << accession );
    return seqvec_ref;
}

string CSeqVectorCache::GetSeq(int pos, int length, CSeqVector &seqvec)
{
    _ASSERT(!seqvec.empty());   

    string seq;
    seqvec.GetSeqData(pos, pos+length, seq);
    return seq;
}

TSeqPos CSeqVectorCache::GetSeqSize(CSeqVector &seqvec)
{
    return seqvec.size();
} 

template<class T>
void CVariationNormalization_base<T>::RotateLeft(string &v)
{
    // simple rotation to the left
    std::rotate(v.begin(), v.begin() + 1, v.end());
}

template<class T>
void CVariationNormalization_base<T>::RotateRight(string &v)
{
    // simple rotation to the right
    std::rotate(v.rbegin(), v.rbegin() + 1, v.rend());
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CVariation& variation, CScope &scope)
{
    if (variation.IsSetPlacements() 
        || !variation.IsSetData() 
        || !variation.GetData().IsSet() 
        || !variation.GetData().GetSet().IsSetVariations()) 
        return;
        
    //Each Variation object iterated here will either:
    // 1. Contains 1 Variation object (with its placement), 
    //          and its data is a single instance 
    // 2. Contains 1 Variation object (with its placement),
    //          and its data is a set of instances
    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it, variation.SetData().SetSet().SetVariations()) {
        CVariation& var = **var_it;

        ERR_POST(Trace << "Outside Variation data set: " << MSerial_AsnText << var);

        if( !var.IsSetData() )
            continue;
        
        const CVariation_inst::TType& type = CVariationUtilities::GetVariationType(var);
        
        //Only insertion and deletion can shift
        //All insertions and deletions are set type
        if(    type != CVariation_inst::eType_ins 
            && type != CVariation_inst::eType_del) {
                continue;
        }

        //Correct SeqLoc interval *around* the insertion
        // to point *after* the insertion.
        // Placements are stored in the first variation in the Set.
        CVariantPlacement& vp = *var.SetPlacements().front(); 
        CSeq_loc& loc = vp.SetLoc();

        SEndPosition sep(loc.GetStart(eExtreme_Positional),
            loc.GetStop(eExtreme_Positional));


        // If variant is already left shifted, convert to left-shifted form
        if(CVariationNormalization::isFullyShifted(var))
        {
            string ref;
            vector<string> alts;
            CVariationUtilities::GetVariationRefAlt(var, ref,  alts);
            TSeqPos deletion_size = ref.size();
            ResetFullyShifted(var, loc, sep, type, deletion_size);
            ERR_POST(Trace << "Reset full shifted var" << MSerial_AsnText << var);
        }


        if(type == CVariation_inst::eType_ins
            && loc.IsInt()) {
                x_ConvertIntervalToPoint(loc,
                    loc.GetStop(eExtreme_Positional));
                sep.left = loc.GetStop(eExtreme_Positional);
        }


        const CSeq_id& seq_id = *vp.GetLoc().GetId();
        ENa_strand strand = eNa_strand_unknown;
        if (loc.IsSetStrand())
            strand = loc.GetStrand();
        
        CRef<CSeqVector> seqvec = PrefetchSequence(scope,seq_id,strand);

        if(type == CVariation_inst::eType_ins) {
            
            string common_repeat_unit_allele = 
                CVariationUtilities::GetCommonRepeatUnit(var);

            ERR_POST(Trace << "Common Repeat Unit: " << common_repeat_unit_allele);

            //There are multiple alleles here.  Need to work through
            // the set.
            if( common_repeat_unit_allele == kEmptyStr)
            {
                continue;
            }

            int rotation_counter=0;
            //This alters SeqLoc and the compact'ed allele
            if(!ProcessShift(common_repeat_unit_allele, sep, 
                *seqvec, rotation_counter, CVariation_inst::eType_ins)) {
                    continue;
            }


            ERR_POST(Trace << "This particular allele shifted to the left: " << 
                rotation_counter << " and the common repeat unit is now: " << 
                common_repeat_unit_allele);

            //allele size doesn't matter for insertions, only deletions
            const int allele_size = 0;
            ModifyLocation(loc, sep, CVariation_inst::eType_ins, allele_size);

            SetShiftFlag(var);


            //alleles are sometimes here, or can be instance
            switch(var.GetData().Which()) {
            case CVariation::TData::C_Data::e_Instance:
                {
                    CVariation_inst& inst = var.SetData().SetInstance();

                    string allele = inst.GetDelta().front()->GetSeq()
                        .GetLiteral().GetSeq_data().GetIupacna().Get();

                    for(int ii=0; ii<rotation_counter; ++ii) {
                        Rotate(allele);
                    }

                    inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
                }

                break;
            case CVariation::TData::C_Data::e_Set:
                {
                    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it2, var.SetData().SetSet().SetVariations()) {
                        CVariation& var2 = **var_it2;
                        if(!var2.IsSetData() || !var2.GetData().IsInstance())
                            continue;

                        CVariation_inst& inst = var2.SetData().SetInstance();

                        string allele = inst.GetDelta().front()->GetSeq()
                            .GetLiteral().GetSeq_data().GetIupacna().Get();

                        for(int ii=0; ii<rotation_counter; ++ii) {
                            Rotate(allele);
                        }

                        inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
                    }
                }
                break;
            default: break;
            }

        } else if(type == CVariation_inst::eType_del) {
            //Should be exactly two variation objects for deletions:
            //  the ref and the alt.
            if(var.GetData().GetSet().GetVariations().size() != 2) {
                ERR_POST(Error << "Malformed deletion Variation Ref with more than 2 'alleles'");
                return;
            }

            string ref_allele;
            CRef<CSeq_literal> ref_literal;
            NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it, var.SetData().SetSet().SetVariations()) {
                CVariation& var = **var_it;
                if(!var.IsSetData() || !var.SetData().IsInstance())
                    continue;

                CVariation_inst& inst = var.SetData().SetInstance();
                if(x_CheckForFirstNTSeq(inst) && inst.GetType() == CVariation_inst::eType_identity ) {
                    ref_allele  = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    ref_literal = Ref(&inst.SetDelta().front()->SetSeq().SetLiteral());
                }
            }


            int rotation_counter = 0;
            const bool found = ProcessShift(ref_allele, 
                sep, 
                *seqvec, rotation_counter, type);
            if(!found) 
                return;


            //Set the reference allele now that it is rotated
            ref_literal->SetSeq_data().SetIupacna().Set(ref_allele);

            //Alter the SeqLoc
            ModifyLocation(loc,
                sep, CVariation_inst::eType_del,
                ref_allele.size());

            SetShiftFlag(var);

        }

    }
}


template<class T>
void CVariationNormalization_base<T>::x_Shift(CSeq_annot& annot, CScope &scope)
{
	if (!annot.IsSetData() || !annot.GetData().IsFtable())
		NCBI_THROW(CException, eUnknown, "Ftable is not set in input Seq-annot");

    NON_CONST_ITERATE(CSeq_annot::TData::TFtable, feat_it, 
        annot.SetData().SetFtable()) {
        x_Shift(**feat_it, scope);
    }
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CSeq_feat& feat, CScope &scope)
{
	LOG_POST(Trace << "This Feat: " << MSerial_AsnText << feat << Endm);
	if (!feat.IsSetLocation()           || 
		!feat.IsSetData()               || 
		!feat.GetData().IsVariation()   || 
		!feat.GetData().GetVariation().IsSetData()) return;
    CVariation_ref& vref = feat.SetData().SetVariation();
    CVariation_inst::TType type = CVariationUtilities::GetVariationType(vref);

    //only insertion and deletion can shift
    if( type != CVariation_inst::eType_ins 
        && type != CVariation_inst::eType_del) {
            return;
    }

    CSeq_loc& featloc = feat.SetLocation();
    //Correct SeqLoc interval *around* the insertion
    // to point *after* the insertion.

    SEndPosition sep(
        featloc.GetStart(eExtreme_Positional),
        featloc.GetStop(eExtreme_Positional));


    // If variant is already left shifted, convert to left-shifted form
    if(CVariationNormalization::isFullyShifted(feat))
    {
        string ref;
        vector<string> alts;
        CVariationUtilities::GetVariationRefAlt(vref, ref,  alts);
        TSeqPos deletion_size = ref.size();
        ResetFullyShifted(feat, featloc, sep, type, deletion_size);
        ERR_POST(Trace << "Reset full shifted feat" << MSerial_AsnText << feat);
    }

    // HGVS parser produces interval insertion around the insertion site
    // Convert to standard form of ins-before a point.
    if(type == CVariation_inst::eType_ins
        && featloc.IsInt()) {

            x_ConvertIntervalToPoint(featloc,
                featloc.GetStop(eExtreme_Positional));

            sep.left = featloc.GetStop(eExtreme_Positional);
    }



	const CSeq_id &seq_id = *feat.GetLocation().GetId();
	
	ENa_strand strand = eNa_strand_unknown;
	if (feat.GetLocation().IsSetStrand())
		strand = feat.GetLocation().GetStrand();

	CRef<CSeqVector> seqvec ;
	try {
		seqvec = PrefetchSequence(scope,seq_id,strand);
		ERR_POST(Trace << "Prefetch success for : " << seq_id.AsFastaString());
	} catch(CException& e) {
		ERR_POST(Error << "Prefetch failed for " << seq_id.AsFastaString() );
		ERR_POST(Error << e.ReportAll());
		return;
	}
    
    if(type == CVariation_inst::eType_ins) {
        //insertion can be set or instance (see test cases 1.asn and 11.asn)
        string common_repeat_unit_allele = 
            CVariationUtilities::GetCommonRepeatUnit(vref);

        //Insertions are sets (identity + ins-before insertion)

        //Is each allele contain a common repeat unit?
        if( common_repeat_unit_allele == kEmptyStr )
        {
            LOG_POST(Trace << "No common repeat unit among alts");
            return;
        }

        //This alters SeqLoc and the compact'ed allele
        int rotation_counter=0;
        if(!ProcessShift(common_repeat_unit_allele, sep, 
            *seqvec, rotation_counter, CVariation_inst::eType_ins))
            return;

        ERR_POST(Trace << "This particular allele shifted to the left: " << 
            rotation_counter << " and the common repeat unit is now: " << 
            common_repeat_unit_allele);

        //allele size doesn't matter for insertions, only deletions
        const int allele_size = 0;
        ModifyLocation(featloc, sep, CVariation_inst::eType_ins, allele_size);
        
        SetShiftFlag(feat);
        

        //Apply rotation to each allele, in either set or instances:
        switch(vref.GetData().Which()) {
        case CVariation_ref::C_Data::e_Instance:
            {
                CVariation_inst& inst = vref.SetData().SetInstance();
                string allele = inst.GetDelta().front()->GetSeq()
                    .GetLiteral().GetSeq_data().GetIupacna().Get();

                for(int ii=0; ii<rotation_counter; ++ii) {
                    Rotate(allele);
                    LOG_POST(Trace << "Rotate allele : " << ii << " : " << allele);
                }

                inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
            }
            break;
        case CVariation_ref::C_Data::e_Set:
            {
                NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vref.SetData().SetSet().SetVariations())
                {
                    CVariation_ref& vref2 = **vref_it;

                    if(!vref2.IsSetData() || !vref2.GetData().IsInstance())
                        continue;

                    CVariation_inst& inst = vref2.SetData().SetInstance();

                    string allele = inst.GetDelta().front()->GetSeq()
                        .GetLiteral().GetSeq_data().GetIupacna().Get();

                    for(int ii=0; ii<rotation_counter; ++ii) {
                        Rotate(allele);
                        LOG_POST(Trace << "Rotate allele : " << ii << " : " << allele);
                    }

                    inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(allele);
                }
            }
            break;
        default: break;
        }
    } else if(type == CVariation_inst::eType_del) {
        //Should be exactly two variation objects for deletions:
        //  the ref and the alt.
        if(vref.GetData().GetSet().GetVariations().size() != 2) {
            ERR_POST(Error << "Malformed deletion Variation Ref with more than 2 'alleles'");
            return;
        }

        string ref_allele;
        CRef<CSeq_literal> ref_literal;
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, vref_it, vref.SetData().SetSet().SetVariations()) {
            CVariation_ref& vref = **vref_it;
            if(!vref.IsSetData() || !vref.SetData().IsInstance())
                continue;

            CVariation_inst& inst = vref.SetData().SetInstance();
            if(x_CheckForFirstNTSeq(inst) && inst.GetType() == CVariation_inst::eType_identity ) {
                ref_allele  = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                ref_literal = Ref(&inst.SetDelta().front()->SetSeq().SetLiteral());
            }
        }

        //why no common repeat unit?
        // a. nothing common about it.le
        int rotation_counter = 0;
        const bool found = ProcessShift(ref_allele, 
            sep, *seqvec, rotation_counter, type);

        if(!found) 
            return;

        //Set the reference allele now that it is rotated
        ref_literal->SetSeq_data().SetIupacna().Set(ref_allele);
        
        //Alter the SeqLoc
        ModifyLocation(featloc, sep,
            CVariation_inst::eType_del, ref_allele.size());

        SetShiftFlag(feat);
	}

    LOG_POST(Trace << "Shifted SeqFeat: " << MSerial_AsnText << feat);
}

/// Only invoked through full-shift
template<class T>
bool CVariationNormalization_base<T>::x_IsShiftable(const CSeq_loc &loc, 
    const string &allele, CScope &scope, CVariation_inst::TType type)
{
    if (type != CVariation_inst::eType_ins && type != CVariation_inst::eType_del)
        return false;

    if (allele.empty())
        return false;

    const CSeq_id &seq_id = *loc.GetId();

    SEndPosition end_positions(
        loc.GetStart(eExtreme_Positional),
        loc.GetStop(eExtreme_Positional));


    ENa_strand strand = eNa_strand_unknown;
    if (loc.IsSetStrand())
        strand = loc.GetStrand();
    CRef<CSeqVector> seqvec = PrefetchSequence(scope,seq_id,strand);


    int rotation_counter=0; //unused
    bool isShifted = false;
    if (type == CVariation_inst::eType_del)
    {
        string tmp_allele = allele;
        isShifted = ProcessShift(tmp_allele, 
            end_positions,
            *seqvec, rotation_counter, type);
    }
    else
    {  
        //insertion type
        string compact = CVariationUtilities::RepeatedSubstring(allele);
        isShifted = ProcessShift(compact,
            end_positions,
            *seqvec, rotation_counter, type);
    }

    return isShifted;
}

template<class T>
void CVariationNormalization_base<T>::x_ConvertIntervalToPoint(CSeq_loc &loc, int pos)
{
    CRef<CSeq_point> pnt(new CSeq_point);
    pnt->SetPoint(pos);
    if (loc.GetInt().IsSetStrand())
        pnt->SetStrand( loc.GetInt().GetStrand() );
    pnt->SetId().Assign(loc.GetInt().GetId());
    loc.SetPnt().Assign(*pnt);
}


template<class T>
void CVariationNormalization_base<T>::x_ConvertPointToInterval(CSeq_loc &loc, 
    int pos_left, int pos_right)
{
    CRef<CSeq_interval> interval(new CSeq_interval);
    interval->SetFrom(pos_left);
    interval->SetTo(pos_right);
    if (loc.GetPnt().IsSetStrand())
        interval->SetStrand( loc.GetPnt().GetStrand() );
    interval->SetId().Assign(loc.GetPnt().GetId());
    loc.SetInt().Assign(*interval);
}


/***********************************************************
 Does the last nt in compact match the last 5' end of the insert?
 INSERTIONS:
 NNNNNA ----- GNNNNNN <-- Name the A here 'five_prime_last_nt'
 ins:   NNNNA
 Rotate, while true.
 Position is the 'G' above *iff* the variant is normalized to SeqPnt.
 DELETIONS:
 NNNNNGGGCNNNNNNN
 del   GG
 NNNNNG--CNNNNNNN should become: (G here is five_prime_last_nt)
 NNNNN--GCNNNNNNN
 Position is the first deleted nt.
 compact_allele is the removed allele
 Rotate while true.
**************************************************************/
bool CVariationNormalizationLeft::ProcessShift(
    string& common_repeat_unit_allele, 
    SEndPosition& sep,
    CSeqVector& seqvec, 
    int& rotation_counter, 
    const CVariation_inst::TType type ) //both deletion and insertion are treated the same here)
{
    if(common_repeat_unit_allele.empty())
        return false;

    ERR_POST(Trace << "Initial pos: (" << sep.left << "," << sep.right <<")");
    //For both insertion and deletion, we want to review
    // the context just upstream of the event.
    string five_prime_last_nt;
    if (sep.left-1 >=0 && sep.left < GetSeqSize(seqvec))
        five_prime_last_nt = GetSeq(sep.left-1,1,seqvec);
    
    ERR_POST(Trace << "Compare at pos-1: " << sep.left-1 << " the nt " 
        << five_prime_last_nt << " to the back of compact: " << common_repeat_unit_allele);

    bool did_rotate = false;
    while( !five_prime_last_nt.empty()
        && (*common_repeat_unit_allele.rbegin() == *five_prime_last_nt.begin()) ) {
        
        ERR_POST(Trace << "We had a match, therefore rotate: " << 
            *common_repeat_unit_allele.rbegin() << " off (" <<
            *five_prime_last_nt.begin() << " @ left pos: " <<
            sep.left-1 <<")");

        did_rotate = true;
        rotation_counter++;
        Rotate(common_repeat_unit_allele);
        sep.left--;
        sep.right--;

        ERR_POST(Trace << "Rotation info: " << rotation_counter << " : " << common_repeat_unit_allele);


        if (sep.left-1 <0 || sep.left >= GetSeqSize(seqvec)) {
            //beyond end of sequence
            break;
        }

        five_prime_last_nt = GetSeq(sep.left-1,1,seqvec);

        ERR_POST(Trace << "Will: " << 
            *common_repeat_unit_allele.rbegin() << " and " <<
            *five_prime_last_nt.begin() << " match?  Taken from position n-1: " <<
            sep.left-1);

    }

    ERR_POST(Trace << "We have moved to position: " << sep.left 
        << " and allele: " << common_repeat_unit_allele);

    return did_rotate;
}

bool CVariationNormalizationRight::ProcessShift(
    string& compact_allele, SEndPosition& sep,
    CSeqVector& seqvec, 
    int& rotation_counter, 
    const CVariation_inst::TType type)
{
    ERR_POST(Trace << "Initial pos: (" << sep.left << "," << sep.right <<")");
    
    if(compact_allele.empty())
        return false;

    //For both insertion and deletion, we want to review
    // the context just upstream of the event.
    string three_prime_first_nt;
    if(type == CVariation_inst::eType_ins) {
        if (sep.left >=0 && sep.left+1 < GetSeqSize(seqvec))
            three_prime_first_nt = GetSeq(sep.left,1,seqvec);
    } else if(type == CVariation_inst::eType_del) {
        if (sep.right+1 >=0 && sep.right+2 < GetSeqSize(seqvec))
            three_prime_first_nt = GetSeq(sep.right+1,1,seqvec);
    } else {
        ERR_POST(Error << "Neither insertion nor deletion.  Rather:" << type
            << Endm);
        return false;
    }

    ERR_POST(Trace << "Compare at left pos: " << sep.left << " and right pos " 
        << sep.right << " nt " << three_prime_first_nt << 
        " to the front of compact: " << *compact_allele.begin());

    bool did_rotate = false;
    while( !three_prime_first_nt.empty() 
        && (*compact_allele.begin() == *three_prime_first_nt.begin())  ) {
        ERR_POST(Trace << "Match and rotate: " << 
            *compact_allele.begin() << " " <<
            *three_prime_first_nt.begin() << " @ right pos: " <<
            sep.right);

        did_rotate = true;
        rotation_counter++;
        Rotate(compact_allele);
        sep.right++;
        sep.left++;
        //copy of above code
        if(type == CVariation_inst::eType_ins) {
            if (sep.left >=0 && sep.left+1 < GetSeqSize(seqvec))
                three_prime_first_nt = GetSeq(sep.left,1,seqvec);
        } else if(type == CVariation_inst::eType_del) {
            if (sep.right+1 >=0 && sep.right+2 < GetSeqSize(seqvec))
                three_prime_first_nt = GetSeq(sep.right+1,1,seqvec);
        }
    }

    ERR_POST(Trace << "We have moved to position: " << sep.left 
        << " and right position: " << sep.right << " and allele: " << compact_allele);

    return did_rotate;
}


void CVariationNormalizationLeft::ModifyLocation(CSeq_loc &loc, 
    SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size)
{
    if(type == CVariation_inst::eType_del && sep.left != sep.right) {
        if (loc.IsInt()) {
            loc.SetInt().SetFrom(sep.left);
            loc.SetInt().SetTo(sep.right);
        } else {
            x_ConvertPointToInterval(loc,sep.left,sep.right);
        }
    } else {
        // all other types (just insertion) + single point deletions
        if (loc.IsInt())
            x_ConvertIntervalToPoint(loc,sep.left);
        else
            loc.SetPnt().SetPoint(sep.left); 
    }
}

void CVariationNormalizationRight::ModifyLocation(CSeq_loc &loc, 
    SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size) // identical to the left point
{
    if(type == CVariation_inst::eType_del && sep.left != sep.right) {
        if (loc.IsInt()) {
            loc.SetInt().SetFrom(sep.left);
            loc.SetInt().SetTo(sep.right);
        } else {
            x_ConvertPointToInterval(loc,sep.left,sep.right);
        }
    } else {
        // all other types (just insertion) + single point deletions
        if (loc.IsInt())
            x_ConvertIntervalToPoint(loc,sep.right);
        else
            loc.SetPnt().SetPoint(sep.right); 
    }
}

void CVariationNormalizationInt::ModifyLocation(CSeq_loc &loc, 
    SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size ) 
{
    if (sep.left == sep.right)
    {
        if (loc.IsPnt())
            loc.SetPnt().SetPoint(sep.left );
        else
            x_ConvertIntervalToPoint(loc, sep.left );
    }
    else
    {
        if (loc.IsInt())
        {
            loc.SetInt().SetFrom(sep.left );
            loc.SetInt().SetTo(sep.right);
        }
        else
            x_ConvertPointToInterval(loc, sep.left , sep.right);
    }

}

void CVariationNormalizationLeftInt::ModifyLocation( CSeq_loc &loc, 
    SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size )
{
    if (type == CVariation_inst::eType_ins && sep.left > 0)
    {
        if (loc.IsPnt())
        {
            x_ConvertPointToInterval(loc, sep.left-1, sep.left);
        }
        else
        {
            loc.SetInt().SetFrom(sep.left-1);
            loc.SetInt().SetTo(sep.left); 
        }
    }
    else  if (type == CVariation_inst::eType_del)
    {
        if (deletion_size == 1)
        {
            if (loc.IsPnt())
                loc.SetPnt().SetPoint(sep.left);
            else
                x_ConvertIntervalToPoint(loc,sep.left);
        }
        else
        {
            if (loc.IsPnt())
                x_ConvertPointToInterval(loc,sep.left,sep.left+deletion_size-1);
            else
            {
                loc.SetInt().SetFrom(sep.left);
                loc.SetInt().SetTo(sep.left+deletion_size-1); 
            }
        }

    }
    else
        CVariationNormalizationLeft::ModifyLocation(loc,sep,type,deletion_size);
}


void CVariationNormalizationInt::Rotate(string& v)
{
    RotateRight(v);
}

void CVariationNormalizationLeftInt::Rotate(string& v)
{
    RotateRight(v);
}

void CVariationNormalizationLeft::Rotate(string& v)
{
    RotateRight(v);
}

void CVariationNormalizationRight::Rotate(string& v)
{
    RotateLeft(v);
}

void CVariationNormalizationInt::SetShiftFlag(CVariation& var)
{
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType().SetStr("Variation Normalization");
    var.SetExt().push_back(uo);

    uo->AddField("Fully Shifted", true);
}

void CVariationNormalizationInt::SetShiftFlag(CSeq_feat& feat)
{
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType().SetStr("Variation Normalization");
    feat.SetExts().push_back(uo);

    uo->AddField("Fully Shifted", true);
}


bool 
    CVariationNormalization::isFullyShifted(const CVariation& var) 
{
    if( !var.IsSetExt())
        return false;

    ITERATE(CVariation::TExt, ext_it, var.GetExt()) {
        const CUser_object& uo = **ext_it;

        if(!uo.GetType().IsStr() 
            || uo.GetType().GetStr() != "Variation Normalization")
            continue;

        CConstRef<CUser_field> uf = uo.GetFieldRef("Fully Shifted");

        if(   uf.NotEmpty()
            && uf->GetData().IsBool()
            && uf->GetData().IsBool()){

                return true;
        }
    }
    return false;
}

bool 
    CVariationNormalization::isFullyShifted(const CSeq_feat& feat) 
{
    if( !feat.IsSetExts())
        return false;

    ITERATE(CVariation::TExt, ext_it, feat.GetExts()) {
        const CUser_object& uo = **ext_it;
        if(!uo.GetType().IsStr() 
            || uo.GetType().GetStr() != "Variation Normalization")
            continue;

        CConstRef<CUser_field> uf = uo.GetFieldRef("Fully Shifted");

        if(   uf.NotEmpty()
           && uf->GetData().IsBool()
           && uf->GetData().IsBool()){
            
               return true;
        }
    }
    return false;
}

bool isVarNormType(const CRef<CUser_object>& uo) {
    if(!uo->GetType().IsStr() 
        || uo->GetType().GetStr() != "Variation Normalization")
        return false;
    return true;
}

template<class T>
void
    CVariationNormalization_base<T>::ResetFullyShifted(CVariation& var,
        CSeq_loc &loc, SEndPosition& sep, const CVariation_inst::TType type, 
        const TSeqPos& deletion_size)
{

    if(type == CVariation_inst::eType_ins) {
        sep.right = sep.left;
    } else if(type == CVariation_inst::eType_del) {
        sep.right = sep.left + deletion_size -1;
    }

    CVariationNormalizationLeft::ModifyLocation(loc, sep, 
        type, deletion_size);

    //delete from list the VArNormTypes
    var.SetExt().remove_if( isVarNormType );
    if(var.GetExt().size() == 0)
        var.ResetExt();

}
template<class T>
void
    CVariationNormalization_base<T>::ResetFullyShifted(CSeq_feat& feat,
    CSeq_loc &loc, SEndPosition& sep, const CVariation_inst::TType type, 
    const TSeqPos& deletion_size)
{

    //must determine deletion size from alt

    if(type == CVariation_inst::eType_ins) {
        sep.right = sep.left;
    } else if(type == CVariation_inst::eType_del) {
        sep.right = sep.left + deletion_size -1;
    }

    CVariationNormalizationLeft::ModifyLocation(loc, sep, 
        type, deletion_size);

    //delete from list the VArNormTypes
    feat.SetExts().remove_if( isVarNormType );
    if(feat.GetExts().size() == 0)
        feat.ResetExts();
}

bool CVariationNormalizationInt::ProcessShift(string &common_repeat_unit_allele, 
    SEndPosition& sep, CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type)
{
    // First, shift left, and find out:
    //  The new left
    //  The rotation counter (and new CRU).
    string common_repeat_unit_allele_tmp = common_repeat_unit_allele;
    
    SEndPosition sep_tmp(sep.left, sep.right);
    bool found_left = CVariationNormalizationLeft::ProcessShift(
        common_repeat_unit_allele, sep_tmp,
        seqvec,rotation_counter,type);
    // Second, shift the original data right, and find out:
    //  The new right
    int ignored_counter=0;
    bool found_right = CVariationNormalizationRight::ProcessShift(
        common_repeat_unit_allele_tmp, sep,
        seqvec,ignored_counter,type);
    
    //Overwrite right shifting influence on left position:
    sep.left = sep_tmp.left;
    LOG_POST(Trace << "Sep: " << sep.left << " " << sep.right);
    return found_left | found_right;
}


bool CVariationNormalizationLeftInt::ProcessShift(string &common_repeat_unit_allele,
    SEndPosition& sep, CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type)
{
    TSeqPos orig_pos_left = sep.left;
    string orig_common_repeat_unit_allele = common_repeat_unit_allele;
    bool shifted = CVariationNormalizationLeft::ProcessShift(
        common_repeat_unit_allele,sep,
        seqvec,rotation_counter,type);
    if (type == CVariation_inst::eType_ins)
    {
        if (!shifted)
        {
            sep.left = orig_pos_left;
            common_repeat_unit_allele = orig_common_repeat_unit_allele;
        }
        //override to true, so that the location will be modified.
        shifted = true;
    }
    sep.right = sep.left ; 
    return shifted;
}





void CVariationNormalization::AlterToVCFVar(CVariation& var, CScope &scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CSeq_annot& var, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(feat,scope);
}

void CVariationNormalization::AlterToHGVSVar(CVariation& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::AlterToHGVSVar(CSeq_annot& annot, CScope& scope)
{
	CVariationNormalizationRight::x_Shift(annot,scope);
}

void CVariationNormalization::AlterToHGVSVar(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(feat,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CVariation& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CSeq_annot& annot, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(annot,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CSeq_feat& feat, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(feat,scope);
}


void CVariationNormalization::AlterToVarLoc(CVariation& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CSeq_annot& annot, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(annot,scope);
}

void CVariationNormalization::AlterToVarLoc(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(feat,scope);
}

bool CVariationNormalization::IsShiftable(const CSeq_loc &loc, 
    const string &allele, CScope &scope, CVariation_inst::TType type)
{
    return CVariationNormalizationInt::x_IsShiftable(loc,allele,scope,type);
}

void CVariationNormalization::NormalizeVariation(CVariation& var, 
    ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(var,scope); break;
    case eHGVS : AlterToHGVSVar(var,scope); break;
    case eVCF : AlterToVCFVar(var,scope); break;
    case eVarLoc : AlterToVarLoc(var,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

void CVariationNormalization::NormalizeVariation(CSeq_annot& annot, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(annot,scope); break;
    case eHGVS : AlterToHGVSVar(annot,scope); break;
    case eVCF : AlterToVCFVar(annot,scope); break;
    case eVarLoc : AlterToVarLoc(annot,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

void CVariationNormalization::NormalizeVariation(CSeq_feat& feat, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(feat,scope); break;
    case eHGVS : AlterToHGVSVar(feat,scope); break;
    case eVCF : AlterToVCFVar(feat,scope); break;
    case eVarLoc : AlterToVarLoc(feat,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

