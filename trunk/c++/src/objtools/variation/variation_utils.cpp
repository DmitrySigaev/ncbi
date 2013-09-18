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

void CVariationUtilities::CorrectRefAllele(CRef<CVariation>& v, CScope& scope)
{
    string old_ref = v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(); 
    string new_ref = GetRefAlleleFromVP(v->SetData().SetSet().SetVariations().front()->SetPlacements().front(), scope);
    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(new_ref);
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
        CorrectRefAllele(vr,scope,new_ref);
    }
}

void CVariationUtilities::CorrectRefAllele(CRef<CVariation_ref>& var, CScope& scope)
{
    CVariation_ref& vr = *var;
    if (!vr.IsSetLocation())
        NCBI_THROW(CException, eUnknown, "Location is not set in input Seq-annot");
    string new_ref = GetAlleleFromLoc(vr.GetLocation(),scope);
    CorrectRefAllele(vr,scope,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, CScope& scope, const string& new_ref)
{
    string old_ref;
    if (!vr.IsSetData() || !vr.GetData().IsSet() || !vr.GetData().GetSet().IsSetVariations())
        NCBI_THROW(CException, eUnknown, "Set of Variation-inst is not set in input Seq-annot");
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if (!(*inst)->IsSetData() || !(*inst)->GetData().IsInstance())
            NCBI_THROW(CException, eUnknown, "Variation-inst is not set in input Seq-annot");
        if ((*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() && CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference &&
            (*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            old_ref  = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
            (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(new_ref);
        }
    }
    if (old_ref.empty())
        NCBI_THROW(CException, eUnknown, "Old reference allele not found in input Seq-annot");
    FixAlleles(vr,old_ref,new_ref);
}

void CVariationUtilities::FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) 
{
    if (old_ref == new_ref) return;
    int type = CVariation_inst::eType_snv;
    bool add_old_ref = true;
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            string a = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
            if (old_ref  == a) add_old_ref = false;
            if (!((*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() && CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference))
            {
                type = (*inst)->SetData().SetInstance().SetType();
                if (new_ref == a)
                {
                    (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(old_ref);
                    add_old_ref = false;
                }
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

// Taken from variation_util2.cpp
string CVariationUtilities::GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope)
{
    string new_ref;
    TSeqPos length = vp->GetSeq().GetLength();
    
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
    for (CVariation::TData::TSet::TVariations::iterator v1 = v->SetData().SetSet().SetVariations().begin(); v1 != v->SetData().SetSet().SetVariations().end(); ++v1)
        if ((*v1)->IsSetPlacements())
        {
            CRef<CVariantPlacement> vp1 =  (*v1)->SetPlacements().front(); // should only be a single placement for each second level variation at this point
            bool found_ref = false;
            int type = CVariation_inst::eType_snv;
            CVariation::TData::TSet::TVariations::iterator var2 = (*v1)->SetData().SetSet().SetVariations().begin();
            while ( var2 != (*v1)->SetData().SetSet().SetVariations().end() )
            {
                bool remove_var = false;
                if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                     && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                     && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                     && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
                {
                    string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                    type = (*var2)->SetData().SetInstance().SetType(); // Here is hoping that type is the same for all leaves at this placement
                    if (!a.empty())
                    {
                        if (a == new_ref)
                        {
                            remove_var = true;
                            found_ref = true;
                        }
                        if (a == old_ref)
                            old_ref.clear();
                    }
                }
                if (remove_var)
                    var2 = (*v1)->SetData().SetSet().SetVariations().erase(var2);
                else
                    ++var2;
            }

            if (!new_ref.empty())
            {
                if (new_ref == old_ref)
                    old_ref.clear();
                //else if (!found_ref)
                //    NCBI_THROW(CException, eUnknown, "New reference allele is not found in the set of existing alleles " + new_ref);
            }

            if (!old_ref.empty())
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
                (*v1)->SetData().SetSet().SetVariations().push_back(leaf);
            }
        }
}

// Variation Normalization
#ifdef SEQVEC_CACHE
CSeqVector CVariationNormalization_base_cache::m_seq_vec;
#else
string CVariationNormalization_base_cache::m_Sequence;
#endif
string CVariationNormalization_base_cache::m_Accession;
int CVariationNormalization_base_cache::m_Type;         


void CVariationNormalization_base_cache::x_rotate_left(string &v)
{
    // simple rotation to the left
    std::rotate(v.begin(), v.begin() + 1, v.end());
}

void CVariationNormalization_base_cache::x_rotate_right(string &v)
{
    // simple rotation to the right
    std::rotate(v.rbegin(), v.rbegin() + 1, v.rend());
}

void CVariationNormalization_base_cache::x_PrefetchSequence(CScope &scope, CRef<CSeq_id> seq_id)
{
    string accession;
    seq_id->GetLabel(&accession);
    if (accession != m_Accession)
    {
        m_Accession = accession;
        const CBioseq_Handle& bsh = scope.GetBioseqHandle( *seq_id );
#ifdef SEQVEC_CACHE
        m_seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
#else
        m_Sequence.clear();     
        CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        seq_vec.GetSeqData(0, seq_vec.size(), m_Sequence);
#endif
    }
}

string CVariationNormalization_base_cache::x_GetSeq(int pos, int length)
{
#ifdef SEQVEC_CACHE
    string seq;
    m_seq_vec.GetSeqData(pos, pos+length, seq);
    return seq;
#else
    _ASSERT(!m_Sequence.empty());   
    return m_Sequence.substr(pos,length);
#endif
}

int CVariationNormalization_base_cache::x_GetSeqSize()
{
#ifdef SEQVEC_CACHE
    return m_seq_vec.size();
#else
    return m_Sequence.size();
#endif
} 

template<class T>
void CVariationNormalization_base<T>::x_Shift(CRef<CVariation>& v, CScope &scope)
{
    if (v->IsSetPlacements())
        for (CVariation::TPlacements::iterator vp1 =  v->SetPlacements().begin(); vp1 != v->SetPlacements().end(); ++vp1)
        {
            int pos_left=-1,pos_right=-1;
          
            if ((*vp1)->IsSetLoc())
            {
                CRef<CSeq_id> seq_id(new CSeq_id);
                if ((*vp1)->GetLoc().IsPnt() && (*vp1)->GetLoc().GetPnt().IsSetPoint() && (*vp1)->GetLoc().GetPnt().IsSetId())
                {
                    seq_id->Assign((*vp1)->GetLoc().GetPnt().GetId());
                    pos_left = (*vp1)->GetLoc().GetPnt().GetPoint();
                    pos_right = pos_left;
                }
                else if ((*vp1)->GetLoc().IsInt() && (*vp1)->GetLoc().GetInt().IsSetId())
                {
                    seq_id->Assign((*vp1)->GetLoc().GetInt().GetId());
                    pos_left = (*vp1)->GetLoc().GetInt().GetFrom();
                    pos_right = (*vp1)->GetLoc().GetInt().GetTo();
                }
                else
                    NCBI_THROW(CException, eUnknown, "Placement is neither point nor interval");
                string ref;
                if ((*vp1)->IsSetSeq() && (*vp1)->GetSeq().IsSetSeq_data() && (*vp1)->GetSeq().GetSeq_data().IsIupacna())
                    ref = (*vp1)->SetSeq().SetSeq_data().SetIupacna().Set();

                int new_pos_left = -1;
                int new_pos_right = -1;
                bool is_deletion = false;
                CSeq_literal *refref = NULL;

                x_PrefetchSequence(scope,seq_id);
                if (v->IsSetData() && v->SetData().IsInstance())
                    x_ProcessInstance(v->SetData().SetInstance(),(*vp1)->SetLoc(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right);
                else if (v->IsSetData() && v->GetData().IsSet() && v->GetData().GetSet().IsSetVariations())
                    for ( CVariation::TData::TSet::TVariations::iterator var2 = v->SetData().SetSet().SetVariations().begin(); var2 != v->SetData().SetSet().SetVariations().end();  ++var2 )
                        if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance())
                            x_ProcessInstance((*var2)->SetData().SetInstance(),(*vp1)->SetLoc(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right);

                if (!ref.empty() && is_deletion)
                {
                    bool found = x_ProcessShift(ref, pos_left,pos_right);
                    if (found)
                    {
                        pos_right += ref.size()-1;
                        if (new_pos_left == -1)
                            new_pos_left = pos_left;
                        else if (new_pos_left != pos_left)
                            NCBI_THROW(CException, eUnknown, "Left position is ambiguous due to different leaf alleles");
                        if (new_pos_right == -1)
                            new_pos_right = pos_right;
                        else if (new_pos_right != pos_right)
                            NCBI_THROW(CException, eUnknown, "Right position is ambiguous due to different leaf alleles");
                        x_ModifyLocation((*vp1)->SetLoc(),(*vp1)->SetSeq(),ref,pos_left,pos_right);
                    }
                }
            }
        }
}

template<class T>
void CVariationNormalization_base<T>::x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  CSeq_literal *&refref, string &ref, int &pos_left, int &pos_right, int &new_pos_left, int &new_pos_right)
{
    int type = inst.SetType(); 
    if (type != CVariation_inst::eType_identity)
        m_Type = type;
    if (type == CVariation_inst::eType_del)
        is_deletion = true;
    if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
        && inst.GetDelta().front()->GetSeq().IsLiteral() && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
        && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
    {
        string a = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
        if (type == CVariation_inst::eType_identity && inst.IsSetObservation() && inst.GetObservation() == CVariation_inst::eObservation_reference)
        {
            ref = a;
            refref = &inst.SetDelta().front()->SetSeq().SetLiteral();
        }
        if (!a.empty() && type == CVariation_inst::eType_ins) 
        {
            bool found = x_ProcessShift(a, pos_left,pos_right);
            if (found)
            {
                pos_right += a.size();
                if (new_pos_left == -1)
                    new_pos_left = pos_left;
                else if (new_pos_left != pos_left)
                    NCBI_THROW(CException, eUnknown, "Left position is ambiguous due to different leaf alleles");
                if (new_pos_right == -1)
                    new_pos_right = pos_right;
                else if (new_pos_right != pos_right)
                    NCBI_THROW(CException, eUnknown, "Right position is ambiguous due to different leaf alleles");
                x_ModifyLocation(loc,inst.SetDelta().front()->SetSeq().SetLiteral(),a,pos_left,pos_right);
            }
        }
    }
}


template<class T>
void CVariationNormalization_base<T>::x_Shift(CRef<CSeq_annot>& annot, CScope &scope)
{
    if (!annot->IsSetData() || !annot->GetData().IsFtable())
        NCBI_THROW(CException, eUnknown, "Ftable is not set in input Seq-annot");
    for (CSeq_annot::TData::TFtable::iterator feat = annot->SetData().SetFtable().begin(); feat != annot->SetData().SetFtable().end(); ++feat)
    {
        int pos_left=-1,pos_right=-1;
        if ((*feat)->IsSetLocation())
            {
                CRef<CSeq_id> seq_id(new CSeq_id);
                if ((*feat)->GetLocation().IsPnt() && (*feat)->GetLocation().GetPnt().IsSetPoint() && (*feat)->GetLocation().GetPnt().IsSetId())
                {
                    seq_id->Assign((*feat)->GetLocation().GetPnt().GetId());
                    pos_left = (*feat)->GetLocation().GetPnt().GetPoint();
                    pos_right = pos_left;
                }
                else if ((*feat)->GetLocation().IsInt() && (*feat)->GetLocation().GetInt().IsSetId())
                {
                    seq_id->Assign((*feat)->GetLocation().GetInt().GetId());
                    pos_left = (*feat)->GetLocation().GetInt().GetFrom();
                    pos_right = (*feat)->GetLocation().GetInt().GetTo();
                }
                else
                    NCBI_THROW(CException, eUnknown, "Placement is neither point nor interval");
                int new_pos_left = -1;
                int new_pos_right = -1;
                bool is_deletion = false;
                string ref;
                CSeq_literal *refref = NULL;
                x_PrefetchSequence(scope,seq_id);
                // cout << "Sequence: " <<x_GetSeq(5621,3) << endl;
                CVariation_ref& vr = (*feat)->SetData().SetVariation();
                if (vr.IsSetData() && vr.GetData().IsInstance())
                    x_ProcessInstance(vr.SetData().SetInstance(),(*feat)->SetLocation(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right);
                else if (vr.IsSetData() && vr.GetData().IsSet())
                    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
                        if ( (*inst)->IsSetData() && (*inst)->SetData().IsInstance())
                            x_ProcessInstance((*inst)->SetData().SetInstance(),(*feat)->SetLocation(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right);
                
                if (!ref.empty() && is_deletion)
                {
                    bool found = x_ProcessShift(ref, pos_left,pos_right);
                    if (found)
                    {
                        pos_right += ref.size()-1;
                        if (new_pos_left == -1)
                            new_pos_left = pos_left;
                        else if (new_pos_left != pos_left)
                            NCBI_THROW(CException, eUnknown, "Left position is ambiguous due to different leaf alleles");
                        if (new_pos_right == -1)
                            new_pos_right = pos_right;
                        else if (new_pos_right != pos_right)
                            NCBI_THROW(CException, eUnknown, "Right position is ambiguous due to different leaf alleles");
                        x_ModifyLocation((*feat)->SetLocation(),*refref,ref,pos_left,pos_right);
                    }
                }

            }
    }
}

bool CVariationNormalizationLeft::x_ProcessShift(string &a, int &pos,int &pos_right)
{
    bool found = false;
    if (pos+a.size() < x_GetSeqSize())
        for (unsigned int i=0; i<a.size(); i++)
        {
            string b = x_GetSeq(pos,a.size());
            if (a == b) 
            {
                found = true;
                break;
            }
            x_rotate_left(a);
        }
    if (m_Type == CVariation_inst::eType_ins)
    {
        if (!found && pos > a.size())
        {
            pos -= a.size();
            if (pos+a.size() < x_GetSeqSize())
                for (unsigned int i=0; i<a.size(); i++)
                {
                    string b = x_GetSeq(pos,a.size());
                    if (a == b) 
                    {
                        found = true;
                        break;
                    }
                    x_rotate_left(a);
                }
        }
    }
    if (!found) return false;
                            
    bool found_left = false;
    while (pos >= 0 && pos+a.size() < x_GetSeqSize())
    {
        pos--; 
        x_rotate_right(a);
        string b = x_GetSeq(pos,a.size());
        if (a != b) 
        {
            pos++;
            x_rotate_left(a);
            found_left = true;
            break;
        }
    }
    return found_left;                         
}

void CVariationNormalizationLeft::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos, int pos_right)
{
    if (loc.IsInt())
    {
        CSeq_point pnt;
        pnt.SetPoint(pos);
        if (loc.GetInt().IsSetStrand())
            pnt.SetStrand( loc.GetInt().GetStrand() );
        pnt.SetId().Assign(loc.GetInt().GetId());
        loc.SetPnt().Assign(pnt);
    }
    else
        loc.SetPnt().SetPoint(pos); 
    literal.SetSeq_data().SetIupacna().Set(a);
}

bool CVariationNormalizationRight::x_ProcessShift(string &a, int &pos_left, int &pos)
{
    bool found = false;
    pos = pos - a.size() + 1;
    if (pos+a.size() < x_GetSeqSize())
        for (unsigned int i=0; i<a.size(); i++)
        {
            string b = x_GetSeq(pos,a.size());
            if (a == b) 
            {
                found = true;
                break;
            }
            x_rotate_left(a);
        }
    if (m_Type == CVariation_inst::eType_ins)
    {
        if (!found && pos > a.size())
        {
            pos -= a.size();
            if (pos+a.size() < x_GetSeqSize())
                for (unsigned int i=0; i<a.size(); i++)
                {
                    string b = x_GetSeq(pos,a.size());
                    if (a == b) 
                    {
                        found = true;
                        break;
                    }
                    x_rotate_left(a);
                }
        }
    }
    if (!found) return false;
                            
    bool found_right = false;
    while (pos+a.size() < x_GetSeqSize())
    {
        pos++; 
        x_rotate_left(a);
        string b = x_GetSeq(pos,a.size());
        if (a != b) 
        {
            pos--;
            x_rotate_right(a);
            found_right = true;
            break;
        }
    }
    return found_right;                         
}

void CVariationNormalizationRight::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos) // identical to the left point
{
    if (loc.IsInt())
    {
        CSeq_point pnt;
        pnt.SetPoint(pos);
        if (loc.GetInt().IsSetStrand())
            pnt.SetStrand( loc.GetInt().GetStrand() );
        pnt.SetId().Assign(loc.GetInt().GetId());
        loc.SetPnt().Assign(pnt);
    }
    else
        loc.SetPnt().SetPoint(pos); 
    literal.SetSeq_data().SetIupacna().Set(a);
}

void CVariationNormalizationInt::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) 
{
    if (pos_left == pos_right)
    {
        if (loc.IsPnt())
            loc.SetPnt().SetPoint(pos_left);
        else
        {
            CSeq_point pnt;
            pnt.SetPoint(pos_left);
            if (loc.GetInt().IsSetStrand())
                pnt.SetStrand( loc.GetInt().GetStrand() );
            pnt.SetId().Assign(loc.GetInt().GetId());
            loc.SetPnt().Assign(pnt);
        }
    }
    else
    {
        if (loc.IsInt())
        {
            loc.SetInt().SetFrom(pos_left);
            loc.SetInt().SetTo(pos_right);
        }
        else
        {
            CSeq_interval interval;
            interval.SetFrom(pos_left);
            interval.SetTo(pos_right);
            if (loc.GetPnt().IsSetStrand())
                interval.SetStrand( loc.GetPnt().GetStrand() );
            interval.SetId().Assign(loc.GetPnt().GetId());
            loc.SetInt().Assign(interval);
        }
    }
    literal.SetSeq_data().SetIupacna().Set(a);
}

bool CVariationNormalizationInt::x_ProcessShift(string &a, int &pos_left, int &pos_right)
{
    string a_left = a;
    bool found_left = CVariationNormalizationLeft::x_ProcessShift(a_left, pos_left, pos_right);
    bool found_right = CVariationNormalizationRight::x_ProcessShift(a, pos_left, pos_right);
    a = a_left;
    return found_left | found_right;
}

void CVariationNormalizationInt2::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) 
{
    CVariationNormalizationInt::x_ModifyLocation(loc,literal,a,pos_left,pos_right);
}

bool CVariationNormalizationInt2::x_ProcessShift(string &a, int &pos_left, int &pos_right) // The same as the first Int but the right margin is taken at the end of the sequence
{
    string a_left = a;
    bool found_left = CVariationNormalizationLeft::x_ProcessShift(a_left, pos_left, pos_right);
    bool found_right = CVariationNormalizationRight::x_ProcessShift(a, pos_left, pos_right);
    pos_right += a.size()-1;
    a = a_left;
    return found_left | found_right;
}



bool CVariationNormalizationLeftInt::x_ProcessShift(string &a, int &pos_left, int &pos_right)
{
    int orig_pos_left = pos_left;
    string orig_a = a;
    bool r = CVariationNormalizationLeft::x_ProcessShift(a,pos_left,pos_right);
    if (m_Type == CVariation_inst::eType_ins)
    {
        if (!r)
        {
            pos_left = orig_pos_left;
            a = orig_a;
        }
        r = true;
    }
    pos_right = pos_left;
    return r;
}

void CVariationNormalizationLeftInt::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right)
{
    if (m_Type == CVariation_inst::eType_ins && pos_left > 0)
    {
        if (loc.IsPnt())
        {
            CSeq_interval interval;
            interval.SetFrom(pos_left-1);
            interval.SetTo(pos_left);
            if (loc.GetPnt().IsSetStrand())
                interval.SetStrand( loc.GetPnt().GetStrand() );
            interval.SetId().Assign(loc.GetPnt().GetId());
            loc.SetInt().Assign(interval);
        }
        else
        {
            loc.SetInt().SetFrom(pos_left-1);
            loc.SetInt().SetTo(pos_left); 
        }
        literal.SetSeq_data().SetIupacna().Set(a);
    }
    else  if (m_Type == CVariation_inst::eType_del)
    {
        if (a.size() == 1)
        {
            if (loc.IsPnt())
                loc.SetPnt().SetPoint(pos_left);
            else
            {
                CSeq_point pnt;
                pnt.SetPoint(pos_left);
                if (loc.GetInt().IsSetStrand())
                    pnt.SetStrand( loc.GetInt().GetStrand() );
                pnt.SetId().Assign(loc.GetInt().GetId());
                loc.SetPnt().Assign(pnt);
            }
        }
        else
        {
            if (loc.IsPnt())
            {
                CSeq_interval interval;
                interval.SetFrom(pos_left);
                interval.SetTo(pos_left+a.size()-1);
                if (loc.GetPnt().IsSetStrand())
                    interval.SetStrand( loc.GetPnt().GetStrand() );
                interval.SetId().Assign(loc.GetPnt().GetId());
                loc.SetInt().Assign(interval);
            }
            else
            {
                loc.SetInt().SetFrom(pos_left);
                loc.SetInt().SetTo(pos_left+a.size()-1); 
            }
        }
        literal.SetSeq_data().SetIupacna().Set(a);
    }
    else
        CVariationNormalizationLeft::x_ModifyLocation(loc,literal,a, pos_left, pos_right);
}




void CVariationNormalization::AlterToVCFVar(CRef<CVariation>& var, CScope &scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CRef<CSeq_annot>& var, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToHGVSVar(CRef<CVariation>& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::AlterToHGVSVar(CRef<CSeq_annot>& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CRef<CVariation>& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CRef<CSeq_annot>& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars2(CRef<CVariation>& var, CScope &scope)
{
    CVariationNormalizationInt2::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars2(CRef<CSeq_annot>& var, CScope &scope)
{
    CVariationNormalizationInt2::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CRef<CVariation>& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CRef<CSeq_annot>& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeVariation(CRef<CVariation>& var, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(var,scope); break;
    case eDbSnp2 : NormalizeAmbiguousVars2(var,scope); break;
    case eHGVS : AlterToHGVSVar(var,scope); break;
    case eVCF : AlterToVCFVar(var,scope); break;
    case eVarLoc : AlterToVarLoc(var,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

void CVariationNormalization::NormalizeVariation(CRef<CSeq_annot>& var, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(var,scope); break;
    case eDbSnp2 : NormalizeAmbiguousVars2(var,scope); break;
    case eHGVS : AlterToHGVSVar(var,scope); break;
    case eVCF : AlterToVCFVar(var,scope); break;
    case eVarLoc : AlterToVarLoc(var,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

