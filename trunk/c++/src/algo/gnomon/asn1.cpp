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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 * conversion to/from ASN1
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/asn1.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include "gnomon_seq.hpp"

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)
USING_SCOPE(ncbi::objects);

struct SModelData {
    SModelData(const CAlignModel& model, const CEResidueVec& contig_seq);

    const CAlignModel& model;
    CEResidueVec mrna_seq;
    CRef<CSeq_id> mrna_sid;
    CRef<CSeq_id> prot_sid;

    bool is_ncrna;
};

SModelData::SModelData(const CAlignModel& m, const CEResidueVec& contig_seq) : model(m)
{
    m.GetAlignMap().EditedSequence(contig_seq, mrna_seq, true);

    string model_tag = "hmm." + NStr::IntToString(model.ID());
    prot_sid = new CSeq_id(CSeq_id::e_Local, model_tag + ".p");  
    mrna_sid = new CSeq_id(CSeq_id::e_Local, model_tag + ".m");

    is_ncrna = m.ReadingFrame().Empty();
}


class CAnnotationASN1::CImplementationData {
public:
    CImplementationData(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc);

    void AddModel(const CAlignModel& model);
    CRef<CSeq_entry> main_seq_entry;

private:
    CRef<CSeq_entry> CreateModelProducts(SModelData& model);
    CRef<CSeq_feat> create_gene_feature(const SModelData& md) const;
    void update_gene_feature(CSeq_feat& gene, CSeq_feat& mrna_feat) const;
    CRef<CSeq_entry>  create_prot_seq_entry(const SModelData& md, const CSeq_entry& mrna_seq_entry, const CSeq_feat& cdregion_feature) const;
    CRef<CSeq_entry> create_mrna_seq_entry(SModelData& md, CRef<CSeq_feat> cdregion_feature);
    CRef<CSeq_feat> create_mrna_feature(SModelData& md);
    enum EWhere { eOnGenome, eOnMrna };
    CRef<CSeq_feat> create_cdregion_feature(SModelData& md,EWhere onWhat);
    CRef<CSeq_feat> create_5prime_stop_feature(SModelData& md) const;
    CRef<CSeq_align> model2spliced_seq_align(SModelData& md);
    CRef<CSpliced_exon> spliced_exon (const CModelExon& e, EStrand strand) const;
    CRef<CSeq_loc> create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits = TSignedSeqRange::GetWhole());
    void DumpEvidence(SModelData& md);
    void DumpUnusedChains();

    string contig_name;
    CRef<CSeq_id> contig_sid;
    const CResidueVec& contig_seq;
    CDoubleStrandSeq  contig_ds_seq;

    CBioseq_set::TSeq_set* nucprots;
    CSeq_annot::C_Data::TFtable* feature_table;
    CSeq_annot::C_Data::TAlign*  model_alignments;


    typedef map<int,CRef<CSeq_feat> > TGeneMap;
    TGeneMap genes;
    IEvidence& evidence;

    friend class CAnnotationASN1;
};

CAnnotationASN1::CImplementationData::CImplementationData(const string& a_contig_name, const CResidueVec& seq, IEvidence& evdnc) :
    main_seq_entry(new CSeq_entry),
    contig_name(a_contig_name),
    contig_sid(CreateSeqid(a_contig_name)), contig_seq(seq),
    evidence(evdnc)
{
    Convert(contig_seq, contig_ds_seq);

    CBioseq_set& bioseq_set = main_seq_entry->SetSet();

    nucprots = &bioseq_set.SetSeq_set();

    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    seq_annot->AddName("Gnomon models");
    seq_annot->SetTitle("Gnomon models");
    bioseq_set.SetAnnot().push_back(seq_annot);
    feature_table = &seq_annot->SetData().SetFtable();

    seq_annot.Reset(new CSeq_annot);
    bioseq_set.SetAnnot().push_back(seq_annot);
    model_alignments = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Model Alignments");
    seq_annot->SetTitle("Model Alignments");
}

CAnnotationASN1::CAnnotationASN1(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc) :
    m_data(new CImplementationData(contig_name, seq, evdnc))
{
}

CAnnotationASN1::~CAnnotationASN1()
{
}

CRef<CSeq_entry> CAnnotationASN1::GetASN1() const
{
    m_data->DumpUnusedChains();
    return m_data->main_seq_entry;
}


void CAnnotationASN1::AddModel(const CAlignModel& model)
{
    m_data->AddModel(model);
}

CRef<CSeq_entry> CAnnotationASN1::CImplementationData::CreateModelProducts(SModelData& md)
{
    CRef<CSeq_entry> model_products(new CSeq_entry);
    nucprots->push_back(model_products);
    model_products->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);

    
    CRef<CSeq_feat> cdregion_feature;
    if (!md.is_ncrna)
        cdregion_feature = create_cdregion_feature(md,eOnMrna);
    CRef<CSeq_entry> mrna_seq_entry = create_mrna_seq_entry(md, cdregion_feature);
    model_products->SetSet().SetSeq_set().push_back(mrna_seq_entry);

    if (!md.is_ncrna)
        model_products->SetSet().SetSeq_set().push_back(create_prot_seq_entry(md, *mrna_seq_entry, *cdregion_feature));

    return mrna_seq_entry;
}


void CAnnotationASN1::CImplementationData::AddModel(const CAlignModel& model)
{
    SModelData md(model, contig_ds_seq[ePlus]);

    CRef<CSeq_entry> mrna_seq_entry = CreateModelProducts(md);

    model_alignments->push_back(
                                mrna_seq_entry->GetSeq().GetInst().GetHist().GetAssembly().front()
                                );


    CRef<CSeq_feat> mrna_feat = create_mrna_feature(md);
    feature_table->push_back(mrna_feat);

    if (!md.is_ncrna) {
        CRef<CSeq_feat> cds_feat;
        cds_feat = create_cdregion_feature(md,eOnGenome);
        feature_table->push_back(cds_feat);

        CRef< CSeqFeatXref > cdsxref( new CSeqFeatXref() );
        cdsxref->SetId(*cds_feat->SetIds().front());
        //     cdsxref->SetData().SetCdregion();
        
        CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
        mrnaxref->SetId(*mrna_feat->SetIds().front());
        //     mrnaxref->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        
        cds_feat->SetXref().push_back(mrnaxref);
        mrna_feat->SetXref().push_back(cdsxref);

    }
    if (model.GeneID()) {
        TGeneMap::iterator gene = genes.find(model.GeneID());
        if (gene == genes.end()) {
            CRef<CSeq_feat> gene_feature = create_gene_feature(md);
            gene = genes.insert(make_pair(model.GeneID(), gene_feature)).first;
            feature_table->push_back(gene_feature);
        }
        update_gene_feature(*gene->second, *mrna_feat);
    }

}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_cdregion_feature(SModelData& md, EWhere where)
{
    const CGeneModel& model = md.model;
    EStrand strand = model.Strand();

    CRef<CSeq_feat> cdregion_feature(new CSeq_feat);  

    if (where==eOnGenome) {
        CRef<CObject_id> obj_id( new CObject_id() );
        obj_id->SetStr("cds." + NStr::IntToString(model.ID()));
        CRef<CFeat_id> feat_id( new CFeat_id() );
        feat_id->SetLocal(*obj_id);
        cdregion_feature->SetIds().push_back(feat_id);
    }

    CRef<CGenetic_code::C_E> cdrcode(new CGenetic_code::C_E);
    cdrcode->SetId(1);
    cdregion_feature->SetData().SetCdregion().SetCode().Set().push_back(cdrcode);

   if(model.PStop() && (where==eOnMrna || model.FrameShifts().empty())) {
        CCdregion::TCode_break& code_breaks = cdregion_feature->SetData().SetCdregion().SetCode_break();
        ITERATE(CCDSInfo::TPStops,s,model.GetCdsInfo().PStops()) {
            CRef< CCode_break > code_break(new CCode_break);
            CRef<CSeq_loc> pstop;

            TSeqPos from = s->GetFrom();
            TSeqPos to = s->GetTo();
            switch (where) {
            case eOnMrna:
                from = model.GetAlignMap().MapOrigToEdited(from);
                to = model.GetAlignMap().MapOrigToEdited(to);
                
                if (strand==eMinus)
                    swap(from,to);
                _ASSERT(from+2==to);
                pstop.Reset(new CSeq_loc(*md.mrna_sid, from, to, eNa_strand_plus));
                break;
            case eOnGenome:
                _ASSERT(model.GetAlignMap().FShiftedLen(from,to)==3);
                pstop = create_packed_int_seqloc(model,*s);
                break;
            }

            code_break->SetLoc(*pstop);
            CRef<CCode_break::C_Aa> aa(new CCode_break::C_Aa);
            aa->SetNcbieaa('X');
            code_break->SetAa(*aa);
            code_breaks.push_back(code_break);
        }
    }

    cdregion_feature->SetProduct().SetWhole(*md.prot_sid);

    int start, stop, altstart;
    if (strand==ePlus) {
        altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetFrom());
        start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetFrom());
        stop = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetTo());
    } else {
        altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetTo());
        start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetTo());
        stop = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetFrom());
    }

    int frame = 0;
    if(!model.HasStart()) {
        _ASSERT(altstart == 0);
        frame = start%3;
        start = 0;
    }
    CCdregion::EFrame ncbi_frame = CCdregion::eFrame_one;
    if(frame == 1) ncbi_frame = CCdregion::eFrame_two;
    else if(frame == 2) ncbi_frame = CCdregion::eFrame_three; 
    cdregion_feature->SetData().SetCdregion().SetFrame(ncbi_frame);

    CRef<CSeq_loc> cdregion_location;
    switch (where) {
    case eOnMrna:
        cdregion_location.Reset(new CSeq_loc(*md.mrna_sid, start, stop, eNa_strand_plus));

        if (0 < altstart && altstart != start)
            cdregion_location->SetInt().SetFuzz_from().SetAlt().push_back(altstart);

        break;
    case eOnGenome:
        cdregion_location = create_packed_int_seqloc(model,model.RealCdsLimits());

        if (strand==ePlus) {
            altstart = model.MaxCdsLimits().GetFrom();
            start = model.RealCdsLimits().GetFrom();
        } else {
            altstart = model.MaxCdsLimits().GetTo();
            start = model.RealCdsLimits().GetTo();
        }

        if(!model.FrameShifts().empty()) {
            cdregion_feature->SetExcept(true);
            cdregion_feature->SetExcept_text("unclassified translation discrepancy");
        }

        if (model.Status() & CGeneModel::ePseudo) {
            cdregion_feature->SetPseudo(true);
        }
 
        break;
    }

    if (!model.HasStart())
        cdregion_location->SetPartialStart(true,eExtreme_Biological);
    if (!model.HasStop())
        cdregion_location->SetPartialStop(true,eExtreme_Biological);
    cdregion_feature->SetLocation(*cdregion_location);

    if (!model.HasStart() || !model.HasStop() || !model.Continuous())
        cdregion_feature->SetPartial(true);

    return cdregion_feature;
}


CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_5prime_stop_feature(SModelData& md) const
{
    const CGeneModel& model = md.model;


    int frame = -1;
    TIVec starts[3], stops[3];

    FindStartsStops(model, contig_ds_seq[model.Strand()], md.mrna_seq, model.GetAlignMap(), starts, stops, frame);
    _ASSERT( starts[frame].size()>0 && stops[frame].size()>0 );

    TSignedSeqPos stop_5prime = stops[frame][0];
    if (FindUpstreamStop(stops[frame],starts[frame][0],stop_5prime) && stop_5prime>=0) {
        CRef<CSeq_feat> stop_5prime_feature(new CSeq_feat);
        stop_5prime_feature->SetData().SetImp().SetKey("misc_feature");
        stop_5prime_feature->SetData().SetImp().SetDescr("5' stop");
        CRef<CSeq_loc> stop_5prime_location(new CSeq_loc(*md.mrna_sid, stop_5prime, stop_5prime+2, eNa_strand_plus));
        stop_5prime_feature->SetLocation(*stop_5prime_location);
        return stop_5prime_feature;
    } else
        return CRef<CSeq_feat>();
}

CRef< CUser_object > create_ModelEvidence_user_object()
{
    CRef< CUser_object > user_obj(new CUser_object);
    CRef< CObject_id > type(new CObject_id);
    type->SetStr("ModelEvidence");
    user_obj->SetType(*type);

    return user_obj;
}

CRef<CSeq_loc> CAnnotationASN1::CImplementationData::create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits)
{
    CRef<CSeq_loc> seq_loc(new CSeq_loc);
    CPacked_seqint& packed_int = seq_loc->SetPacked_int();
    ENa_strand strand = model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus; 

	for (size_t i=0; i < model.Exons().size(); ++i) {
		const CModelExon* e = &model.Exons()[i];
        TSignedSeqRange interval_range = e->Limits() & limits;
        if (interval_range.Empty())
            continue;
        CRef<CSeq_interval> interval(new CSeq_interval(*contig_sid, interval_range.GetFrom(),interval_range.GetTo(), strand));

		if (!e->m_fsplice && 0 < i) {
                    interval->SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
		}
		if (!e->m_ssplice && i < model.Exons().size()-1) {
                    interval->SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
		}

        packed_int.AddInterval(*interval);
    }
    return seq_loc->Merge(CSeq_loc::fSort, NULL);
}

void ExpandSupport(const CSupportInfoSet& src, CSupportInfoSet& dst, IEvidence& evidence)
{
    ITERATE(CSupportInfoSet, s, src) {
        dst.insert(*s);

        const CAlignModel* m = evidence.GetModel(s->GetId());
        if (m && (m->Type()&(CAlignModel::eChain|CAlignModel::eGnomon))!=0) {
            _ASSERT( !s->IsCore() );
            ExpandSupport(m->Support(), dst, evidence);
        }
    }
}

void CAnnotationASN1::CImplementationData::DumpEvidence(SModelData& md)
{
    const CGeneModel& model = md.model;
    evidence.GetModel(model.ID()); // complete chains might not get marked as used otherwise

    if (model.Support().empty())
        return;
    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    main_seq_entry->SetSet().SetAnnot().push_back(seq_annot);
    CSeq_annot::C_Data::TAlign* aligns = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Evidence for "+md.mrna_sid->GetSeqIdString());
    seq_annot->SetTitle("Evidence for "+md.mrna_sid->GetSeqIdString());
    
    
    ITERATE(CSupportInfoSet, s, model.Support()) {
        int id = s->GetId();
        CRef<CSeq_align> a = evidence.GetSeq_align(id);
        if (a.NotEmpty()) {
            aligns->push_back(a);
            continue;
        }

        const CAlignModel* m = evidence.GetModel(id);
        if (m == NULL)
            continue;

        SModelData smd(*m, contig_ds_seq[ePlus]);

        if (m->Type()&CGeneModel::eChain) {
            CreateModelProducts(smd);
            DumpEvidence(smd);
        } else {
            smd.mrna_sid.Reset( FindBestChoice(m->GetTargetIds(), CSeq_id::Score) ); 
        }
        aligns->push_back(model2spliced_seq_align(smd));
    }
}

void CAnnotationASN1::CImplementationData::DumpUnusedChains()
{
    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    main_seq_entry->SetSet().SetAnnot().push_back(seq_annot);
    CSeq_annot::C_Data::TAlign* aligns = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Unused chains");
    seq_annot->SetTitle("Unused chains");

    auto_ptr<IEvidence::iterator> it( evidence.GetUnusedModels(contig_name) );
    const CAlignModel* m;
    while ((m = it->GetNext()) != NULL) {
        if ((m->Type()&CAlignModel::eChain) == 0)
            continue;
        
        SModelData smd(*m, contig_ds_seq[ePlus]);

        aligns->push_back(model2spliced_seq_align(smd));

        CreateModelProducts(smd);
        DumpEvidence(smd);
    }
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_mrna_feature(SModelData& md)
{
    const CGeneModel& model = md.model;

    CRef<CSeq_feat> mrna_feature(new CSeq_feat);

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetStr("rna." + NStr::IntToString(model.ID()));
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    mrna_feature->SetIds().push_back(feat_id);

    mrna_feature->SetData().SetRna().SetType(md.is_ncrna ? CRNA_ref::eType_ncRNA : CRNA_ref::eType_mRNA);
    mrna_feature->SetProduct().SetWhole(*md.mrna_sid);

    mrna_feature->SetLocation(*create_packed_int_seqloc(model));
                
    if(!model.HasStart() || !model.HasStop() || !model.Continuous())
        mrna_feature->SetPartial(true);

    if(model.HasStart())
        mrna_feature->SetLocation().SetPartialStart(false,eExtreme_Biological);
    if(model.HasStop())
        mrna_feature->SetLocation().SetPartialStop(false,eExtreme_Biological);

    if(!model.FrameShifts().empty()) {
        mrna_feature->SetExcept(true);
        mrna_feature->SetExcept_text("unclassified transcription discrepancy");
    }
//     if(model.PStop()) {
//         mrna_feature->SetExcept(true);
//         if (mrna_feature->IsSetExcept_text() && mrna_feature->GetExcept_text().size()>0)
//             mrna_feature->SetExcept_text() += ", ";
//         else
//             mrna_feature->SetExcept_text(kEmptyStr);
//         mrna_feature->SetExcept_text() += "internal stop(s)";
//     }

    if (model.Status() & CGeneModel::ePseudo) {
        mrna_feature->SetPseudo(true);
    }

    DumpEvidence(md);

    CRef< CUser_object > user_obj = create_ModelEvidence_user_object();
    mrna_feature->SetExts().push_back(user_obj);
    user_obj->AddField("Method",CGeneModel::TypeToString(model.Type()));
    if (!model.Support().empty()) {
        CRef<CUser_field> support_field(new CUser_field());
        user_obj->SetData().push_back(support_field);
        support_field->SetLabel().SetStr("Support");
        vector<string> chains;
        vector<string> cores;
        vector<string> proteins;
        vector<string> mrnas;
        vector<string> ests;
        vector<string> unknown;

        CSupportInfoSet support;

        ExpandSupport(model.Support(), support, evidence);

        ITERATE(CSupportInfoSet, s, support) {
            
            int id = s->GetId();
            
            const CAlignModel* m = evidence.GetModel(id);

            int type = m ?  m->Type() : 0;

            string accession;
            if (m == NULL || (m->Type()&CGeneModel::eChain)) {
                accession = NStr::IntToString(id);
            } else {
                accession = m->TargetAccession();
            }

            if (s->IsCore())
                cores.push_back(accession);

            if (type&CGeneModel::eChain)
                chains.push_back(accession);
            else if (type&CGeneModel::eProt)
                proteins.push_back(accession);
            else if (type&CGeneModel::emRNA)
                mrnas.push_back(accession);
            else if (type&CGeneModel::eEST)
                ests.push_back(accession);
            else
                unknown.push_back(accession);
        }
        if (!chains.empty()) {
            support_field->AddField("Chains",chains);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(chains.size());
        }
        if (!cores.empty()) {
            support_field->AddField("Core",cores);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(cores.size());
        }
        if (!proteins.empty()) {
            sort(proteins.begin(),proteins.end());
            support_field->AddField("Proteins",proteins);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(proteins.size());
        }
        if (!mrnas.empty()) {
            sort(mrnas.begin(),mrnas.end());
            support_field->AddField("mRNAs",mrnas);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(mrnas.size());
        }
        if (!ests.empty()) {
            sort(ests.begin(),ests.end());
            support_field->AddField("ESTs",ests);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(ests.size());
        }
        if (!unknown.empty()) {
            support_field->AddField("unknown",unknown);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(unknown.size());
        }
    }

    if(!model.ProteinHit().empty())
        user_obj->AddField("BestTargetProteinHit",model.ProteinHit());
    if(model.Status()&CGeneModel::eFullSupCDS)
        user_obj->AddField("CDS support",string("full"));
    return mrna_feature;
}



CRef<CSeq_entry>  CAnnotationASN1::CImplementationData::create_prot_seq_entry(const SModelData& md, const CSeq_entry& mrna_seq_entry, const CSeq_feat& cdregion_feature) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> sprot(new CSeq_entry);
    sprot->SetSeq().SetId().push_back(md.prot_sid);

    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    desc->SetMolinfo().SetCompleteness(
	model.Continuous()?
	    (model.HasStart()?
		(model.HasStop()?
		    CMolInfo::eCompleteness_complete:
                    CMolInfo::eCompleteness_has_left):
                (model.HasStop()?
                    CMolInfo::eCompleteness_has_right:
                    CMolInfo::eCompleteness_no_ends)):
	    CMolInfo::eCompleteness_partial
	);
    sprot->SetSeq().SetDescr().Set().push_back(desc);


    string strprot;
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CScope scope(*obj_mgr);
    scope.AddTopLevelSeqEntry(mrna_seq_entry);
    CCdregion_translate::TranslateCdregion(strprot, cdregion_feature, scope, false);

    CSeq_inst& seq_inst = sprot->SetSeq().SetInst();
    seq_inst.SetRepr(CSeq_inst::eRepr_raw);
    seq_inst.SetMol(CSeq_inst::eMol_aa);
    seq_inst.SetLength(strprot.size());

    if (model.Continuous()) {
        CRef<CSeq_data> dprot(new CSeq_data(strprot, CSeq_data::e_Ncbieaa));
        sprot->SetSeq().SetInst().SetSeq_data(*dprot);
    } else {

        // copy bases from coding region location
        string bases;
        CSeqVector seqv(cdregion_feature.GetLocation(), scope, CBioseq_Handle::eCoding_Ncbi);
        seqv.GetSeqData(0, seqv.size(), bases);

        const CCdregion& cdr = cdregion_feature.GetData().GetCdregion();
        if (cdr.IsSetFrame ()) {
            int offset = 0;

            switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                offset = 1;
                break;
            case CCdregion::eFrame_three :
                offset = 2;
                break;
            default :
                break;
            }
            if (offset > 0)
                bases.erase(0, offset);
        }

        vector<string> tokens;
        vector<SIZE_TYPE> token_pos;
        const string null_char(1, '\0');
        NStr::Tokenize( bases, null_char, tokens, NStr::eMergeDelims, &token_pos);

        size_t b = 0;
        size_t e = 0;
        for( size_t i = 0; i < tokens.size(); ++i) {
            if (i>0) {
                _ASSERT( token_pos[i]%3 == 0 );
                e = token_pos[i]/3;
                seq_inst.SetExt().SetDelta().AddLiteral(e-b);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                b = e;
            }
            _ASSERT( tokens[i].size()%3 == 0 || i == tokens.size()-1 );
            e = b + tokens[i].size()/3;
            seq_inst.SetExt().SetDelta().AddLiteral(strprot.substr(b,e-b),CSeq_inst::eMol_aa);
            b = e;
        }
        _ASSERT( b == strprot.size() + (md.model.HasStop() ? 1 : 0) );
    }
    return sprot;
}

CRef<CSeq_entry> CAnnotationASN1::CImplementationData::create_mrna_seq_entry(SModelData& md, CRef<CSeq_feat> cdregion_feature)
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> smrna(new CSeq_entry);
    smrna->SetSeq().SetId().push_back(md.mrna_sid);

    CRef<CSeqdesc> mdes(new CSeqdesc);
    smrna->SetSeq().SetDescr().Set().push_back(mdes);
    mdes->SetMolinfo().SetBiomol(md.is_ncrna ? CMolInfo::eBiomol_ncRNA : CMolInfo::eBiomol_mRNA);

    mdes->SetMolinfo().SetCompleteness(
        model.Continuous()?
            (model.HasStart()?
                (model.HasStop()?
                    CMolInfo::eCompleteness_unknown:
                    CMolInfo::eCompleteness_no_right):
                (model.HasStop()?
                    CMolInfo::eCompleteness_no_left:
                    CMolInfo::eCompleteness_no_ends)):
            CMolInfo::eCompleteness_partial
        );

    CResidueVec mrna_seq_vec;
    model.GetAlignMap().EditedSequence(contig_seq, mrna_seq_vec, true);
    string mrna_seq_str((char*)&mrna_seq_vec[0],mrna_seq_vec.size());

    CSeq_inst& seq_inst = smrna->SetSeq().SetInst();
    seq_inst.SetRepr(CSeq_inst::eRepr_raw);
    seq_inst.SetMol(CSeq_inst::eMol_rna);
    seq_inst.SetLength(mrna_seq_str.size());

    if (model.Continuous()) {
        CRef<CSeq_data> dmrna(new CSeq_data(mrna_seq_str, CSeq_data::e_Iupacna));
        seq_inst.SetSeq_data(*dmrna);
    } else {
        TSeqPos b = 0;
        TSeqPos e = 0;

        if (model.Strand()==ePlus) {
            for (size_t i=0; i< model.Exons().size()-1; ++i) {
                const CModelExon& e1= model.Exons()[i];
                const CModelExon& e2= model.Exons()[i+1];
                if (e1.m_ssplice && e2.m_fsplice)
                    continue;
                e = model.GetAlignMap().MapOrigToEdited(e1.GetTo())+1;
                seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b,e-b),CSeq_inst::eMol_rna);
                b =  model.GetAlignMap().MapOrigToEdited(e2.GetFrom());

                seq_inst.SetExt().SetDelta().AddLiteral(b-e);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
        } else {
            for (size_t i=model.Exons().size()-1; i > 0; --i) {
                const CModelExon& e1= model.Exons()[i-1];
                const CModelExon& e2= model.Exons()[i];
                if (e1.m_ssplice && e2.m_fsplice)
                    continue;
                e = model.GetAlignMap().MapOrigToEdited(e2.GetFrom())+1;
                seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b,e-b),CSeq_inst::eMol_rna);
                b =  model.GetAlignMap().MapOrigToEdited(e1.GetTo());

                seq_inst.SetExt().SetDelta().AddLiteral(b-e);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
        }
        seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b),CSeq_inst::eMol_rna);
    }

    smrna->SetSeq().SetInst().SetHist().SetAssembly().push_back(model2spliced_seq_align(md));

    if (!cdregion_feature.Empty()) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        smrna->SetSeq().SetAnnot().push_back(annot);
    
        annot->SetData().SetFtable().push_back(cdregion_feature);

        if (!model.Open5primeEnd()) {
            CRef<CSeq_feat> stop = create_5prime_stop_feature(md);
            if (stop.NotEmpty()) 
                annot->SetData().SetFtable().push_back(stop);
        }
    }
    return smrna;
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_gene_feature(const SModelData& md) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_feat> gene_feature(new CSeq_feat);

    string gene_id_str = "gene." + NStr::IntToString(model.GeneID());

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetStr(gene_id_str);
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    gene_feature->SetIds().push_back(feat_id);
    gene_feature->SetData().SetGene().SetDesc(gene_id_str);

    if (model.Status() & CGeneModel::ePseudo) {
        gene_feature->SetData().SetGene().SetPseudo(true);

        int frameshifts = model.FrameShifts().size();
        int pstops = model.GetCdsInfo().PStops().size();
        _ASSERT( frameshifts+pstops > 0 );
        string frameshift_comment;
        string pstop_comment;
        if (frameshifts==1)
            frameshift_comment = "remove a frameshift";
        else if (frameshifts>1)
            frameshift_comment = "remove frameshifts";
        if (pstops==1)
            pstop_comment = "prevent a premature stop codon";
        else if (pstops>1)
            pstop_comment = "prevent premature stop codons";
        if (frameshifts>0 && pstops>0)
            pstop_comment = " and "+pstop_comment;
            
        gene_feature->SetComment("The sequence of the transcript was modified to "+frameshift_comment+pstop_comment+" represented in this assembly.");
    }
    //    .SetLocus_tag("...");

    CRef<CSeq_loc> gene_location(new CSeq_loc());
    gene_feature->SetLocation(*gene_location);

    return gene_feature;
}

void CAnnotationASN1::CImplementationData::update_gene_feature(CSeq_feat& gene, CSeq_feat& mrna) const
{
    CRef< CSeqFeatXref > genexref( new CSeqFeatXref() );
    genexref->SetId(*gene.SetIds().front());
//     genexref->SetData().SetGene();
    
    CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
    mrnaxref->SetId(*mrna.SetIds().front());
//     mrnaxref->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);

    gene.SetXref().push_back(mrnaxref);
    mrna.SetXref().push_back(genexref);

    gene.SetLocation(*gene.GetLocation().Add(mrna.GetLocation(),CSeq_loc::fMerge_SingleRange,NULL));

    if (mrna.CanGetPartial() && mrna.GetPartial())
        gene.SetPartial(true);
}

CRef<CSpliced_exon> CAnnotationASN1::CImplementationData::spliced_exon (const CModelExon& e, EStrand strand) const
{
    CRef<CSpliced_exon> se(new CSpliced_exon());
    // se->SetProduct_start(...); se->SetProduct_end(...); don't fill in here
    se->SetGenomic_start(e.GetFrom());
    se->SetGenomic_end(e.GetTo());
    if (e.m_fsplice) {
        string bases((char*)&contig_seq[e.GetFrom()-2], 2);
        if (strand==ePlus) {
            se->SetAcceptor_before_exon().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetDonor_after_exon().SetBases(bases);
        }
    }
    if (e.m_ssplice) {
        string bases((char*)&contig_seq[e.GetTo()+1], 2);
        if (strand==ePlus) {
            se->SetDonor_after_exon().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetAcceptor_before_exon().SetBases(bases);
        }
    }
    return se;
}

CRef< CSeq_align > CAnnotationASN1::CImplementationData::model2spliced_seq_align(SModelData& md)
{
    const CAlignModel& model = md.model;

    CRef< CSeq_align > seq_align( new CSeq_align );
    seq_align->SetType(CSeq_align::eType_partial);
    CSpliced_seg& spliced_seg = seq_align->SetSegs().SetSpliced();

    spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    spliced_seg.SetProduct_id(*md.mrna_sid);
    spliced_seg.SetGenomic_id(*contig_sid);
    spliced_seg.SetProduct_strand((model.Status() & CGeneModel::eReversed)==0 ? eNa_strand_plus : eNa_strand_minus);
    spliced_seg.SetGenomic_strand(model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus);

    CSpliced_seg::TExons& exons = spliced_seg.SetExons();

    TInDels indels = model.GetInDels(false);

    TInDels::const_iterator indel_i = indels.begin();
    for (size_t i=0; i < model.Exons().size(); ++i) {
        const CModelExon *e = &model.Exons()[i]; 

        CRef<CSpliced_exon> se = spliced_exon(*e,model.Strand());

        TSignedSeqRange transcript_exon = model.TranscriptExon(i);
        se->SetProduct_start().SetNucpos(transcript_exon.GetFrom());
        se->SetProduct_end().SetNucpos(transcript_exon.GetTo());

        int last_chunk = e->GetFrom();
        while (indel_i != indels.end() && indel_i->Loc() <= e->GetTo()+1) {
            const CInDelInfo& indel = *indel_i;
            _ASSERT( e->GetFrom() <= indel.Loc() );
            
            if (indel.Loc()-last_chunk > 0) {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetMatch(indel.Loc()-last_chunk);
                se->SetParts().push_back(chunk);
                last_chunk = indel.Loc();
            }

            if (indel.IsInsertion()) {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetGenomic_ins(indel.Len());
                se->SetParts().push_back(chunk);

                last_chunk += indel.Len();
            } else {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetProduct_ins(indel.Len());
                se->SetParts().push_back(chunk);
            }
            ++indel_i;
        }
        if (e->GetFrom() <= last_chunk && last_chunk <= e->GetTo()) {
            CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
            chunk->SetMatch(e->GetTo()-last_chunk+1);
            se->SetParts().push_back(chunk);
        }

        exons.push_back(se);
    }
    _ASSERT( indel_i == indels.end() );

    if (model.Strand() == eMinus) {
        //    reverse if minus strand (don't forget to reverse chunks)
        exons.reverse();
        NON_CONST_ITERATE(CSpliced_seg::TExons, exon_i, exons) {
            CSpliced_exon& se = **exon_i;
            if (se.IsSetParts())
                se.SetParts().reverse();
        }
    }

#ifdef _DEBUG
    try {
        seq_align->Validate(true);
    } catch (...) {
        _ASSERT(false);
    }
#endif

    return seq_align;
}

/*
void get_exons_from_seq_loc(CGeneModel& model, const CSeq_loc& loc)
{
    for( CSeq_loc_CI i(loc); i; ++i) {
        model.AddExon(i->GetRange());
        model.back().m_fsplice = i->GetFuzzFrom()==NULL;
        model.back().m_ssplice = i->GetFuzzTo()==NULL;
    }
    model.RecalculateLimits();
}

struct collect_indels : public unary_function<CRef<CDelta_seq>, void>
{
    collect_indels(CGeneModel& m) : model(m), exon_i(m.begin()), next_seg_start(m.Limits().GetFrom()) {}
    void operator() (CRef<CDelta_seq> delta_seq)
    {
        if (delta_seq->IsLiteral()) {
            const CSeq_literal& literal = delta_seq->GetLiteral();
            if (literal.GetLength()==0) return;
            CSeq_data seq_data;
            CSeqportUtil::Convert(literal.GetSeq_data(), &seq_data, CSeq_data::e_Iupacna);
            if (model.Strand() == eMinus)
                ReverseComplement(&seq_data);
            model.FrameShifts().push_back(CInDelInfo(next_seg_start,literal.GetLength(), false, seq_data.GetIupacna()));
        } else {
            const CSeq_interval& loc = delta_seq->GetLoc().GetInt();
            while (exon_i->GetTo() < loc.GetFrom()) {
                if (next_seg_start <= exon_i->GetTo())
                    model.FrameShifts().push_back(CInDelInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
                ++exon_i;
                next_seg_start = exon_i->GetFrom();
            }
            if (next_seg_start < loc.GetFrom())
                model.FrameShifts().push_back(CInDelInfo(next_seg_start,loc.GetFrom()-next_seg_start, true));
            next_seg_start = loc.GetTo();
        }
    }
    void flush()
    {
        for(;;) {
            if (next_seg_start <= exon_i->GetTo())
                model.FrameShifts().push_back(CInDelInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
            if ( ++exon_i == m.end())
                break;
            next_seg_start = exon_i->GetFrom();
        }
    }
    CGeneModel& model;
    CGeneModel::iterator exon_i;
    TSignedSeqPos next_seg_start;
};
*/
END_SCOPE(gnomon)
END_NCBI_SCOPE
