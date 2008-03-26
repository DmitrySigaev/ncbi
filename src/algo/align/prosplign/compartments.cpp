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
 * Author:   Boris Kiryutin, Vyacheslav Chetvernin
 *
 * File Description:  Get protein compartments from BLAST hits
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>

#include <algo/align/prosplign/compartments.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include <algo/align/util/hit_comparator.hpp>
#include <algo/align/util/compartment_finder.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);

typedef CSplign::THit     THit;
typedef CSplign::THitRef  THitRef;
typedef CSplign::THitRefs THitRefs;

int CountQueryCoverage(THitRefs& hitrefs)
{
    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eQueryMin);
    stable_sort(hitrefs.begin(), hitrefs.end(), sorter);

    int len = 0;
    TSeqPos stmin = hitrefs.front()->GetQueryMin();
    TSeqPos enmax = hitrefs.front()->GetQueryMax();

    ITERATE(THitRefs, it, hitrefs) {
        if((*it)->GetQueryMin() <= enmax) {//overlaping hits
            enmax = max((*it)->GetQueryMax(), enmax);
        } else {//next chain starts
            len += enmax - stmin + 1;
            stmin = (*it)->GetQueryMin();
            enmax = (*it)->GetQueryMax();
        }
    }
    len += enmax - stmin + 1;

    _ASSERT( len>0 );
    return len;
}

void RemoveOverlaps(THitRefs& hitrefs)
{
    THitRefs hits_new;
    CHitFilter<THit>::s_RunGreedy(hitrefs.begin(), hitrefs.end(), 
                                  &hits_new, 1, 0);
    hitrefs.erase(remove_if(hitrefs.begin(), hitrefs.end(), CHitFilter<THit>::s_PNullRef), 
                  hitrefs.end());
    copy(hits_new.begin(), hits_new.end(), back_inserter(hitrefs));
}

double TotalScore(THitRefs& hitrefs)
{
    double result = 0;
    ITERATE(THitRefs, i, hitrefs) {
        result += (*i)->GetScore();
    }
    return result;
}

CRef<CScore> IntScore(const string& id, int value)
{
    CRef<CScore> result(new CScore);
    result->SetId().SetStr(id);
    result->SetValue().SetInt(value);
    return result;
}

CRef<CScore> RealScore(const string& id, double value)
{
    CRef<CScore> result(new CScore);
    result->SetId().SetStr(id);
    result->SetValue().SetReal(value);
    return result;
}

CRef<CSeq_annot> MakeCompartment(THitRefs& hitrefs, const THitRefs& orig_hitrefs)
{
    _ASSERT( !hitrefs.empty() );

    CRef<CSeq_align> seq_align(new CSeq_align);
    CSeq_align& compartment = *seq_align;
    
    compartment.SetType(CSeq_align::eType_partial);
    CSeq_align::TSegs::TStd& std_segs = compartment.SetSegs().SetStd();
    ITERATE (THitRefs, h, hitrefs) {

        bool strand = (*h)->GetSubjStrand();
        TSeqPos subj_min = (*h)->GetSubjMin();
        TSeqPos subj_max = (*h)->GetSubjMax();
        TSeqPos qry_min = (*h)->GetQueryMin()/3;
        TSeqPos qry_max = (*h)->GetQueryMax()/3;
        double pct_identity = 0;
        double bit_score = 0;

        ITERATE(THitRefs, oh, orig_hitrefs) {
            if ((*oh)->GetSubjStrand() == strand &&
                (*oh)->GetSubjMin() <= subj_min &&
                subj_max <= (*oh)->GetSubjMax() &&
                (*oh)->GetQueryMin() <= qry_min &&
                qry_max <= (*oh)->GetQueryMax() &&
                pct_identity < (*oh)->GetIdentity()) {
                pct_identity = (*oh)->GetIdentity();
                bit_score = fabs((*oh)->GetScore())*(subj_max-subj_min+1)/((*oh)->GetSubjMax()-(*oh)->GetSubjMin()+1);
            }
        }

        CRef<CStd_seg> std_seg(new CStd_seg);

        CRef<CSeq_id> qry_id(new CSeq_id);
        qry_id->Assign(*(*h)->GetQueryId());
        CRef<CSeq_loc> qry_loc(new CSeq_loc(*qry_id,qry_min,qry_max,eNa_strand_plus));
        std_seg->SetLoc().push_back(qry_loc);

        CRef<CSeq_id> subj_id(new CSeq_id);
        subj_id->Assign(*(*h)->GetSubjId());
        CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id,subj_min,subj_max,strand?eNa_strand_plus:eNa_strand_minus));
        std_seg->SetLoc().push_back(subj_loc);

        std_seg->SetScores().push_back(RealScore("pct_identity",pct_identity*100));
        std_seg->SetScores().push_back(RealScore("bit_score",bit_score));

        std_segs.push_back(std_seg);
    }

    CRef<CSeq_annot> result(new CSeq_annot);
    result->SetData().SetAlign().push_back(seq_align);

    return result;
}

TCompartments SelectCompartmentsHits(const THitRefs& orig_hitrefs, CCompartOptions compart_options)
{
    TCompartments results;
    if (orig_hitrefs.empty())
        return results;

    THitRefs hitrefs;

    ITERATE(THitRefs, it, orig_hitrefs) {
        THitRef hitref(new THit(**it));
        if (!hitref->GetQueryStrand())
            NCBI_THROW(CProSplignException, eFormat, "minus strand on protein in BLAST hit");
        hitref->SetQueryMax((hitref->GetQueryMax()+1)*3-1);
        hitref->SetQueryMin(hitref->GetQueryMin()*3);

        if (compart_options.m_ByCoverage)
            hitref->SetIdentity(0.9999);

        hitrefs.push_back(hitref);
    }

    //count 'pseudo' length	
    int len = CountQueryCoverage(hitrefs);

    CCompartmentAccessor<THit> comps ( hitrefs.begin(), hitrefs.end(),
                                       int(compart_options.m_CompartmentPenalty * len),
                                       int(compart_options.m_MinCompartmentIdty * len),
                                       int(compart_options.m_MinSingleCompartmentIdty * len));

    THitRefs comphits;
    if(comps.GetFirst(comphits)) {
        size_t i = 0;
        do {
            RemoveOverlaps(comphits);

            CRef<CSeq_annot> compartment = MakeCompartment(comphits,orig_hitrefs);

            CRef<CUser_object> uo(new CUser_object);
            uo->SetType().SetStr("Compart Scores");
            uo->AddField("bit_score", TotalScore(comphits));
            uo->AddField("num_covered_aa", CountQueryCoverage(comphits)/3);
            compartment->AddUserObject( *uo );

            const THit::TCoord* boxPtr = comps.GetBox(i);

            CRef<CSeq_id> qry_id(new CSeq_id);
            qry_id->Assign(*hitrefs.front()->GetQueryId());
            CRef<CAnnotdesc> align(new CAnnotdesc);
            align->SetAlign().SetAlign_type(CAlign_def::eAlign_type_ref);
            align->SetAlign().SetIds().push_back( qry_id );
            compartment->SetDesc().Set().push_back( align );


            CRef<CSeq_id> subj_id(new CSeq_id);
            subj_id->Assign(*hitrefs.front()->GetSubjId());
            CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id,boxPtr[2],boxPtr[3],comps.GetStrand(i)?eNa_strand_plus:eNa_strand_minus));
            CRef<CAnnotdesc> region(new CAnnotdesc);
            region->SetRegion(*subj_loc);
            compartment->SetDesc().Set().push_back(region);

            results.push_back(compartment);
            ++i;
        } while (comps.GetNext(comphits));
    }

    return results;
}

TCompartmentStructs MakeCompartments(const TCompartments& compartments, CCompartOptions compart_options)
{
    TCompartmentStructs results;

    ITERATE(TCompartments, i, compartments) {
        const CSeq_annot& comp = **i;
        const CSeq_loc* subj_loc = NULL;
        int covered_aa = 0;
        double score = 0;
        ITERATE (CAnnot_descr::Tdata, desc_it, comp.GetDesc().Get()) {
            const CAnnotdesc& desc = **desc_it;
            if (desc.IsRegion()) {
                subj_loc = &desc.GetRegion();
            } else if (desc.IsUser() && desc.GetUser().GetType().IsStr() && desc.GetUser().GetType().GetStr()=="Compart Scores") {
                covered_aa = desc.GetUser().GetField("num_covered_aa").GetData().GetInt();
                score = desc.GetUser().GetField("bit_score").GetData().GetReal();
            }
        }
        if (subj_loc)
            results.push_back(SCompartment(subj_loc->GetStart(eExtreme_Positional),
                                           subj_loc->GetStop(eExtreme_Positional),
                                           subj_loc->GetStrand()!=eNa_strand_minus,
                                           covered_aa, score));
    }
    sort(results.begin(),results.end());

    const int max_extent = compart_options.m_MaxExtent;

    int prev = 0;

    for (size_t i=0; i<results.size(); ++i) {
        SCompartment& comp = results[i];
        prev = (i==0 || results[i-1].strand != comp.strand) ?
            -comp.from :
            prev;
        int next = (i==results.size()-1 || results[i+1].strand != comp.strand) ?
            comp.to+2*max_extent+2 :
            results[i+1].from;
        comp.from = max(comp.from-max_extent,(prev+comp.from)/2+1);
        prev = comp.to;
        comp.to = min(comp.to+max_extent,(comp.to+next)/2-1);
    }

    return results;
}

TCompartmentStructs MakeCompartments(const CSplign::THitRefs& hitrefs, CCompartOptions compart_options)
{
    return MakeCompartments(SelectCompartmentsHits(hitrefs, compart_options), compart_options);
}


void CCompartOptions::SetupArgDescriptions(CArgDescriptions* argdescr)
{
    argdescr->AddDefaultKey
        ("max_extent",
         "max_extent",
         "Max genomic extent to look for exons beyond compartment ends.",
         CArgDescriptions::eInteger,
         NStr::IntToString(CCompartOptions::default_MaxExtent) );

    argdescr->AddDefaultKey
        ("compartment_penalty",
         "double",
         "Penalty to open a new compartment "
         "(compartment identification parameter). "
         "Multiple compartments will only be identified if "
         "they have at least this level of coverage.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_CompartmentPenalty, 2));
    
    argdescr->AddDefaultKey
        ("min_compartment_idty",
         "double",
         "Minimal compartment identity for multiple compartments",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_MinCompartmentIdty, 2));
    
    argdescr->AddDefaultKey
        ("min_singleton_idty",
         "double",
         "Minimal compartment identity for single compartment",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_MinSingleCompartmentIdty, 2));
    
    argdescr->AddDefaultKey
        ("by_coverage",
         "flag",
         "Ignore hit identity. Set all to 99.99%",
         CArgDescriptions::eBoolean,
         CCompartOptions::default_ByCoverage?"T":"F");
    
}

CCompartOptions::CCompartOptions(const CArgs& args) :
    m_CompartmentPenalty(args["compartment_penalty"].AsDouble()),
    m_MinCompartmentIdty(args["min_compartment_idty"].AsDouble()),
    m_MinSingleCompartmentIdty(args["min_singleton_idty"].AsDouble()),
    m_MaxExtent(args["max_extent"].AsInteger()),
    m_ByCoverage(args["by_coverage"].AsBoolean())
{
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
