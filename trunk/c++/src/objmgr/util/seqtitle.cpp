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
* Author:  Aaron Ucko
*
* File Description:
*   Obtains or constructs a sequence's title.  (Corresponds to
*   CreateDefLine in the C toolkit.)
*/

#include <serial/iterator.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/util/feature.hpp>
#include <objects/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)

static string s_TitleFromBioSource (const CBioSource&    source,
                                    const string&        suffix = kEmptyStr);


static string s_TitleFromChromosome(const CBioSource&    source,
                                    const CMolInfo&      mol_info);


static string s_TitleFromProtein   (const CBioseq_Handle& handle,
                                          CScope&        scope,
                                          string&        organism);
static string s_TitleFromSegment   (const CBioseq_Handle& handle,
                                          CScope&        scope);
                                          

string GetTitle(const CBioseq_Handle& hnd, TGetTitleFlags flags)
{
    string                    prefix, title, suffix;
    string                    organism;
    CBioseq_Handle::TBioseqCore core        = hnd.GetBioseqCore();
    CConstRef<CTextseq_id>    tsid(NULL);
    CConstRef<CPDB_seq_id>    pdb_id(NULL);
    CConstRef<CPatent_seq_id> pat_id(NULL);
    CConstRef<CDbtag>         general_id(NULL);
    CConstRef<CBioSource>     source(NULL);
    CConstRef<CMolInfo>       mol_info(NULL);
    bool                      third_party = false;
    bool                      is_nc       = false;
    bool                      is_nm       = false;
    bool                      wgs_master  = false;
    CMolInfo::TTech           tech        = CMolInfo::eTech_unknown;
    bool                      htg_tech    = false;
    bool                      use_biosrc  = false;

    iterate (CBioseq::TId, id, core->GetId()) {
        if ( !tsid ) {
            tsid = (*id)->GetTextseq_Id();
        }
        switch ((*id)->Which()) {
        case CSeq_id::e_Other:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        {
            const CTextseq_id& t = *(*id)->GetTextseq_Id();
            if (t.IsSetAccession()) {
                const string& acc = t.GetAccession();
                CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
                if ((type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs
                    &&  NStr::EndsWith(acc, "000000")) {
                    wgs_master = true;
                } else if (type == CSeq_id::eAcc_refseq_chromosome) {
                    is_nc = true;
                } else if (type == CSeq_id::eAcc_refseq_mrna) {
                    is_nm = true;
                }
            }
            break;
        }
        case CSeq_id::e_General:
            general_id = &(*id)->GetGeneral();
            break;
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            third_party = true;
            break;
        case CSeq_id::e_Pdb:
            pdb_id = &(*id)->GetPdb();
            break;
        case CSeq_id::e_Patent:
            pat_id = &(*id)->GetPatent();
            break;
        default:
            break;
        }
    }
    
    {
        CSeqdesc_CI it(hnd, CSeqdesc::e_Source);
        if (it) {
            source = &it->GetSource();
        }
    }
    {
        CSeqdesc_CI it(hnd, CSeqdesc::e_Molinfo);
        if (it) {
            mol_info = &it->GetMolinfo();
            tech = mol_info->GetTech();
        }
    }
    switch (tech) {
    case CMolInfo::eTech_htgs_0:
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
        // manufacture all titles for unfinished HTG sequences
        flags |= fGetTitle_Reconstruct;
        // fall through
    case CMolInfo::eTech_htgs_3:
        htg_tech = true;
        // fall through
    case CMolInfo::eTech_est:
    case CMolInfo::eTech_sts:
    case CMolInfo::eTech_survey:
    case CMolInfo::eTech_wgs:
        use_biosrc = true;
    default:
        break;
    }

    if (!(flags & fGetTitle_Reconstruct)) {
        // Ignore parents' titles for non-PDB proteins.
        if (core->GetInst().GetMol() == CSeq_inst::eMol_aa
            &&  pdb_id.IsNull()) {
            // Sun Workshop compiler does not call destructors of objects
            // created in for-loop initializers in case we use break to exit the loop
            // (08-apr-2002)
            CTypeConstIterator<CSeqdesc> it = ConstBegin(*core);
            for (; it;  ++it) {
                if (it->IsTitle()) {
                    title = it->GetTitle();
                    BREAK(it);
                }
            }
        } else {
            {
                CSeqdesc_CI it(hnd, CSeqdesc::e_Title);
                if (it) {
                    title = it->GetTitle();
                }
            }
        }
    }

    if (title.empty()  &&  use_biosrc  &&  source.NotEmpty()) {
        if (tech == CMolInfo::eTech_wgs  &&  !wgs_master
            &&  general_id.NotEmpty()  &&  general_id->GetTag().IsStr()) {
            title = s_TitleFromBioSource(*source,
                                         general_id->GetTag().GetStr());
        } else {
            title = s_TitleFromBioSource(*source);
        }
        flags &= ~fGetTitle_Organism;
    }

    if (title.empty()  &&  is_nc  &&  source.NotEmpty()) {
        switch (mol_info->GetBiomol()) {
        case CMolInfo::eBiomol_genomic:
        case CMolInfo::eBiomol_other_genetic:
            title = s_TitleFromChromosome(*source, *mol_info);
            if (!title.empty()) {
                flags &= ~fGetTitle_Organism;
            }
            break;
        }
    } else if (title.empty()  &&  is_nm  &&  source.NotEmpty()) {
        unsigned int         genes = 0, cdregions = 0, prots = 0;
        CConstRef<CSeq_feat> gene(0),   cdregion(0);
        for (CFeat_CI it(hnd, 0, 0, CSeqFeatData::e_not_set);
             it;  ++it) {
            switch (it->GetData().Which()) {
            case CSeqFeatData::e_Gene:
                ++genes;
                gene.Reset(&it->GetMappedFeature());
                break;
            case CSeqFeatData::e_Cdregion:
                ++cdregions;
                cdregion.Reset(&it->GetMappedFeature());
                break;
            case CSeqFeatData::e_Prot:
                ++prots;
                break;
            default:
                break;
            }
        }
        if (genes == 1  &&  cdregions == 1  // &&  prots >= 1
            &&  source->GetOrg().IsSetTaxname()) {
            title = source->GetOrg().GetTaxname() + ' ';
            feature::GetLabel(*cdregion, &title, feature::eContent,
                              &hnd.GetScope());
            title += " (";
            feature::GetLabel(*gene, &title, feature::eContent,
                              &hnd.GetScope());
            title += "), mRNA";
        }
    }

    // originally further down, but moved up to match the C version
    while (NStr::EndsWith(title, ".")  ||  NStr::EndsWith(title, " ")) {
        title.erase(title.end() - 1);
    }

    if (title.empty()  &&  pdb_id.NotEmpty()) {
        CSeqdesc_CI it(hnd, CSeqdesc::e_Pdb);
        for (;  it;  ++it) {
            if ( !it->GetPdb().GetCompound().empty() ) {
                if (isprint(pdb_id->GetChain())) {
                    title = string("Chain ") + (char)pdb_id->GetChain() + ", ";
                }
                title += it->GetPdb().GetCompound().front();
                BREAK(it);
            }
        }
    }

    if (title.empty()  &&  pat_id.NotEmpty()) {
        title = "Sequence " + NStr::IntToString(pat_id->GetSeqid())
            + " from Patent " + pat_id->GetCit().GetCountry()
            + ' ' + pat_id->GetCit().GetId().GetNumber();
    }

    if (title.empty()  &&  core->GetInst().GetMol() == CSeq_inst::eMol_aa) {
        title = s_TitleFromProtein(hnd, hnd.GetScope(), organism);
        if ( !title.empty() ) {
            flags |= fGetTitle_Organism;
        }
    }

    if (title.empty()  &&  !htg_tech
        &&  core->GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
        title = s_TitleFromSegment(hnd, hnd.GetScope());
    }

    if (title.empty()  &&  !htg_tech  &&  source.NotEmpty()) {
        title = s_TitleFromBioSource(*source);
        if (title.empty()) {
            title = "No definition line found";
        }
    }

    if (third_party  &&  !title.empty()
        &&  !NStr::StartsWith(title, "TPA: ", NStr::eNocase)) {
        prefix += "TPA: ";
    }

    switch (tech) {
    case CMolInfo::eTech_htgs_0:
        if (title.find("LOW-PASS") == NPOS) {
            suffix = ", LOW-PASS SEQUENCE SAMPLING";
        }
        break;
    case CMolInfo::eTech_htgs_1:
    case CMolInfo::eTech_htgs_2:
    {
        bool is_draft  = false;
        bool cancelled = false;
        CSeqdesc_CI gb(hnd, CSeqdesc::e_Genbank);
        for (;  gb;  ++gb) {
            iterate (CGB_block::TKeywords, it,
                     gb->GetGenbank().GetKeywords()) {
                if (NStr::Compare(*it, "HTGS_DRAFT", NStr::eNocase) == 0) {
                    is_draft = true;
                    break;
                } else if (NStr::Compare(*it, "HTGS_CANCELLED", NStr::eNocase)
                           == 0) {
                    cancelled = true;
                    break;
                }
            }
            BREAK(gb);
        }
        if (is_draft  &&  title.find("WORKING DRAFT") == NPOS) {
            suffix = ", WORKING DRAFT SEQUENCE";
        } else if (!is_draft  &&  !cancelled
                   &&  title.find("SEQUENCING IN") == NPOS) {
            suffix = ", *** SEQUENCING IN PROGRESS ***";
        }
        
        string un;
        if (tech == CMolInfo::eTech_htgs_1) {
            un = "un";
        }
        if (core->GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
            // We need the full bioseq here...
            const CBioseq& seq = hnd.GetBioseq();
            unsigned int pieces = 1;
            iterate (CDelta_ext::Tdata, it,
                     seq.GetInst().GetExt().GetDelta().Get()) {
                switch ((*it)->Which()) {
                case CDelta_seq::e_Loc:
                    if ( (*it)->GetLoc().IsNull() ) {
                        pieces++;
                    }
                    break;
                case CDelta_seq::e_Literal:
                    if ( !(*it)->GetLiteral().IsSetSeq_data() ) {
                        pieces++;
                    }
                    break;
                default:
                    break;
                }
            }
            if (pieces == 1) {
                suffix += (", 1 " + un + "ordered piece");
            } else {
                suffix += (", " + NStr::IntToString(pieces)
                           + ' ' + un + "ordered pieces");
            }
        } else {
            suffix += ", in " + un + "ordered pieces";
        }
        break;
    }
    case CMolInfo::eTech_htgs_3:
        if (title.find("complete sequence") == NPOS) {
            suffix = ", complete sequence";
        }
        break;

    case CMolInfo::eTech_est:
        if (title.find("mRNA sequence") == NPOS) {
            suffix = ", MRNA sequence";
        }
        break;

    case CMolInfo::eTech_sts:
        if (title.find("sequence tagged site") == NPOS) {
            suffix = ", sequence tagged site";
        }
        break;

    case CMolInfo::eTech_survey:
        if (title.find("genomic survey sequence") == NPOS) {
            suffix = ", genomic survey sequence";
        }
        break;

    case CMolInfo::eTech_wgs:
        if (wgs_master) {
            if (title.find("whole genome shotgun sequencing project") == NPOS){
                suffix = ", whole genome shotgun sequencing project";
            }            
        } else {
            if (title.find("whole genome shotgun sequence") == NPOS) {
                suffix = ", whole genome shotgun sequence";
            }
        }
        break;
    }

    if (flags & fGetTitle_Organism) {
        CConstRef<COrg_ref> org;
        if (source) {
            org = &source->GetOrg();
        } else {
            CSeqdesc_CI it(hnd, CSeqdesc::e_Org);
            for (;  it;  ++it) {
                org = &it->GetOrg();
                BREAK(it);
            }
        }

        if (organism.empty()  &&  org.NotEmpty()  &&  org->IsSetTaxname()) {
            organism = org->GetTaxname();
        }
        if ( !organism.empty()  &&  title.find(organism) == NPOS) {
            suffix += " [" + organism + ']';
        }
    }

    return prefix + title + suffix;
}


static string s_TitleFromBioSource(const CBioSource& source,
                                   const string&     suffix)
{
    string          name, chromosome, clone, map_, strain, sfx;
    const COrg_ref& org = source.GetOrg();

    if (org.IsSetTaxname()) {
        name = org.GetTaxname();
    }

    if (source.IsSetSubtype()) {
        iterate (CBioSource::TSubtype, it, source.GetSubtype()) {
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                chromosome = " chromosome " + (*it)->GetName();
                break;
            case CSubSource::eSubtype_clone:
                clone = " clone " + (*it)->GetName();
                break;
            case CSubSource::eSubtype_map:
                map_ = " map " + (*it)->GetName();
                break;
            }
        }
    }

    if (org.IsSetOrgname()  &&  org.GetOrgname().IsSetMod()) {
        iterate (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            if ((*it)->GetSubtype() == COrgMod::eSubtype_strain
                &&  !NStr::EndsWith(name, 
                (*it)->GetSubname(), NStr::eNocase)) {
                strain = " strain " + (*it)->GetSubname();
            }
        }
    }

    if (suffix.size() > 0) {
        sfx = ' ' + suffix;
    }

    return name + chromosome + clone + map_ + strain + sfx;
}


static string x_TitleFromChromosome(const CBioSource& source,
                                    const CMolInfo&   mol_info)
{
    string name, chromosome, segment, plasmid_name, orgnl;
    string seq_tag, gen_tag;
    bool   is_plasmid = false, is_virus = false;

    if (source.GetOrg().IsSetTaxname()) {
        name = source.GetOrg().GetTaxname();
    } else {
        return kEmptyStr;
    }

    string lc_name = name;
    NStr::ToLower(lc_name);

    if (lc_name.find("virus") != NPOS) {
        is_virus = true;
    }

    if (source.IsSetSubtype()) {
        iterate (CBioSource::TSubtype, it, source.GetSubtype()) {
            switch ((*it)->GetSubtype()) {
            case CSubSource::eSubtype_chromosome:
                chromosome = (*it)->GetName();
                break;
            case CSubSource::eSubtype_segment:
                segment = (*it)->GetName();
                break;
            case CSubSource::eSubtype_plasmid_name:
            {
                plasmid_name = (*it)->GetName();
                string lc_plasmid = plasmid_name;
                NStr::ToLower(lc_plasmid);
                if (lc_plasmid.find("plasmid") == NPOS
                    &&  lc_plasmid.find("element") == NPOS) {
                    plasmid_name = "plasmid " + plasmid_name;
                }
                break;
            }
            }
        }
    }

    switch (source.GetGenome()) {
        // unknown, genomic
    case CBioSource::eGenome_chloroplast:  orgnl = "chloroplast";   break;
    case CBioSource::eGenome_chromoplast:  orgnl = "chromoplast";   break;
    case CBioSource::eGenome_kinetoplast:  orgnl = "kinetoplast";   break;
    case CBioSource::eGenome_mitochondrion:
        orgnl = plasmid_name.empty() ? "mitochondrion" : "mitochondrial";
        break;
    case CBioSource::eGenome_plastid:      orgnl = "plastid";       break;
    case CBioSource::eGenome_macronuclear: orgnl = "macronuclear";  break;
    case CBioSource::eGenome_extrachrom:   orgnl = "extrachrom";    break;
    case CBioSource::eGenome_plasmid:
        orgnl = "plasmid";
        is_plasmid = true;
        break;
        // transposon, insertion-seq
    case CBioSource::eGenome_cyanelle:     orgnl = "cyanelle";      break;
    case CBioSource::eGenome_proviral:
        if (!is_virus) {
            orgnl = plasmid_name.empty() ? "provirus" : "proviral";
        }
        break;
    case CBioSource::eGenome_virion:
        if (!is_virus) {
            orgnl = "virion";
        }
        break;
    case CBioSource::eGenome_nucleomorph:  orgnl = "nucleomorph";   break;
    case CBioSource::eGenome_apicoplast:   orgnl = "apicoplast";    break;
    case CBioSource::eGenome_leucoplast:   orgnl = "leucoplast";    break;
    case CBioSource::eGenome_proplastid:   orgnl = "protoplast";    break;
        // endogenous-virus
    }

    switch (mol_info.GetCompleteness()) {
    case CMolInfo::eCompleteness_partial:
    case CMolInfo::eCompleteness_no_left:
    case CMolInfo::eCompleteness_no_right:
    case CMolInfo::eCompleteness_no_ends:
        seq_tag = ", partial sequence";
        gen_tag = ", genome";
        break;
    default:
        seq_tag = ", complete sequence";
        gen_tag = ", complete genome";
        break;
    }

    if (lc_name.find("plasmid") != NPOS) {
        return name + seq_tag;        
    } else if (is_plasmid) {
        if (plasmid_name.empty()) {
            return name + " unnamed plasmid" + seq_tag;
        } else {
            return name + ' ' + plasmid_name + seq_tag;
        }
    } else if ( !plasmid_name.empty() ) {
        if (orgnl.empty()) {
            return name + ' ' + plasmid_name + seq_tag;
        } else {
            return name + ' ' + orgnl + ' ' + plasmid_name + seq_tag;
        }
    } else if ( !orgnl.empty() ) {
        return name + ' ' + orgnl + gen_tag;
    } else if ( !segment.empty() ) {
        if (segment.find("DNA") == NPOS  &&  segment.find("RNA") == NPOS
            &&  segment.find("segment") == NPOS
            &&  segment.find("Segment") == NPOS) {
            return name + " segment " + segment + seq_tag;
        } else {
            return name + ' ' + segment + seq_tag;
        }
    } else if ( !chromosome.empty() ) {
        return name + " chromosome " + chromosome + seq_tag;
    } else {
        return name + gen_tag;
    }
}


static string s_TitleFromChromosome(const CBioSource& source,
                                    const CMolInfo&   mol_info)
{
    string result = x_TitleFromChromosome(source, mol_info);
    result = NStr::Replace(result, "Plasmid", "plasmid");
    result = NStr::Replace(result, "Element", "element");
    if (!result.empty()) {
        result[0] = toupper(result[0]);
    }
    return result;
}


static CConstRef<CSeq_feat> s_FindLongestFeature(const CSeq_loc& location,
                                                 CScope& scope,
                                                 CSeqFeatData::E_Choice type,
                                                 CFeat_CI::EFeat_Location lt
                                                   = CFeat_CI::e_Location)
{
    CConstRef<CSeq_feat> result;
    TSeqPos best_length = 0;
    CFeat_CI it(scope, location, type, CAnnot_CI::eOverlap_Intervals,
                CFeat_CI::eResolve_TSE, lt);
    for (;  it;  ++it) {
        if (it->GetLocation().IsWhole()) {
            // kludge; length only works on a Seq-loc of type "whole"
            // if its Seq-id points to an object manager, which may not
            // be the case here.
            result = &it->GetMappedFeature();
            BREAK(it);
        } else if (GetLength(it->GetLocation(), &scope) > best_length) {
            best_length = GetLength(it->GetLocation(), &scope);
            result = &it->GetMappedFeature();
        }
    }
    return result;
}


static string s_TitleFromProtein(const CBioseq_Handle& handle, CScope& scope,
                                 string& organism)
{
    CConstRef<CProt_ref> prot;
    CConstRef<CSeq_loc>  cds_loc;
    CConstRef<CGene_ref> gene;
    CBioseq_Handle::TBioseqCore  core = handle.GetBioseqCore();
    string               result;

    CSeq_loc everywhere;
    everywhere.SetWhole().Assign(*core->GetId().front());

    {{
        CConstRef<CSeq_feat> prot_feat
            = s_FindLongestFeature(everywhere, scope, CSeqFeatData::e_Prot);
        if (prot_feat) {
            prot = &prot_feat->GetData().GetProt();
        }
    }}

    {{
        CConstRef<CSeq_feat> cds_feat
            = s_FindLongestFeature(everywhere, scope, CSeqFeatData::e_Cdregion,
                                   CFeat_CI::e_Product);
        if (cds_feat) {
            cds_loc = &cds_feat->GetLocation();
        }
    }}

    if (cds_loc) {
        CConstRef<CSeq_feat> gene_feat
            = s_FindLongestFeature(*cds_loc, scope, CSeqFeatData::e_Gene);
        if (gene_feat) {
            gene = &gene_feat->GetData().GetGene();
        }
    }

    if (prot.NotEmpty()  &&  prot->IsSetName()  &&  !prot->GetName().empty()) {
        bool first = true;
        iterate (CProt_ref::TName, it, prot->GetName()) {
            if (!first) {
                result += "; ";
            }
            result += *it;
            first = false;
        }
    } else if (prot.NotEmpty()  &&  prot->IsSetDesc()
               &&  !prot->GetDesc().empty()) {
        result = prot->GetDesc();
    } else if (gene) {
        string gene_name;
        if (gene->IsSetLocus()  &&  !gene->GetLocus().empty()) {
            gene_name = gene->GetLocus();
        } else if (gene->IsSetSyn()  &&  !gene->GetSyn().empty()) {
            gene_name = *gene->GetSyn().begin();
        } else if (gene->IsSetDesc()  &&  !gene->GetDesc().empty()) {
            gene_name = gene->GetDesc();
        }
        if ( !gene_name.empty() ) {
            result = gene_name + " gene product";
        }
    } else {
        result = "unnamed protein product";
    }

    {{ // Find organism name 
        CConstRef<COrg_ref> org;
        {{
            CSeqdesc_CI it(handle, CSeqdesc::e_Source);
            for (;  it;  ++it) {
                org = &it->GetSource().GetOrg();
                BREAK(it);
            }
        }}
        if (org.Empty()  &&  cds_loc.NotEmpty()) {
            CTypeConstIterator<CSeq_id> id = ConstBegin(*cds_loc);
            for (; id; ++id) {
                CBioseq_Handle na_handle = scope.GetBioseqHandle(*id);
                if (!na_handle) {
                    continue;
                }
                CSeqdesc_CI it(na_handle, CSeqdesc::e_Source);
                for (;  it;  ++it) {
                    org = &it->GetSource().GetOrg();
                    BREAK(it);
                }
                if (org) BREAK(id);
            }
        }
        if (org.NotEmpty()  &&  org->IsSetTaxname()) {
            organism = org->GetTaxname();
        }
    }}

    return result;
}


static string s_TitleFromSegment(const CBioseq_Handle& handle, CScope& scope)
{
    string              organism, product, locus;
    string              completeness = "complete";
    CBioseq_Handle::TBioseqCore core = handle.GetBioseqCore();

    CSeq_loc everywhere;
    everywhere.SetMix().Set() = core->GetInst().GetExt().GetSeg();

    {
        CSeqdesc_CI it(handle, CSeqdesc::e_Source);
        for (;  it;  ++it) {
            if (it->GetSource().GetOrg().IsSetTaxname()) {
                organism = it->GetSource().GetOrg().GetTaxname();
                BREAK(it);
            }
        }
    }

    if (organism.empty()) {
        organism = "Unknown";
    }

    CFeat_CI it(scope, everywhere, CSeqFeatData::e_Cdregion);
    for (; it;  ++it) {
        if ( !it->IsSetProduct() ) {
            continue;
        }
        const CSeq_loc&      product_loc = it->GetProduct();

        if (it->IsSetPartial()) {
            completeness = "partial";
        }

        CConstRef<CSeq_feat> prot_feat
            = s_FindLongestFeature(product_loc, scope, CSeqFeatData::e_Prot);
        if (product.empty()  &&  prot_feat.NotEmpty()
            &&  prot_feat->GetData().GetProt().IsSetName()) {
            product = *prot_feat->GetData().GetProt().GetName().begin();
        }
        
        CConstRef<CSeq_feat> gene_feat
            = s_FindLongestFeature(it->GetLocation(), scope,
                                   CSeqFeatData::e_Gene);
        if (locus.empty()  &&  gene_feat.NotEmpty()) {
            if (gene_feat->GetData().GetGene().IsSetLocus()) {
                locus = gene_feat->GetData().GetGene().GetLocus();
            } else if (gene_feat->GetData().GetGene().IsSetSyn()) {
                locus = *gene_feat->GetData().GetGene().GetSyn().begin();
            }
        }

        BREAK(it);
    }

    string result = organism;
    if ( !product.empty() ) {
        result += ' ' + product;
    }
    if ( !locus.empty() ) {
        result += " (" + locus + ')';
    }
    if ( !product.empty()  ||  !locus.empty() ) {
        result += " gene, " + completeness + " cds";
    }
    return NStr::TruncateSpaces(result);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.16  2003/02/10 18:33:48  ucko
* Don't append organism names that are already present.
*
* Revision 1.15  2003/02/10 15:54:01  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.14  2003/01/30 20:01:38  ucko
* Move dot-stripping code up to match the C Toolkit's (arguably broken)
* behavior; extend it to handle spaces too.
*
* Revision 1.13  2002/12/24 16:11:29  ucko
* Simplify slightly now that CFeat_CI takes a const handle.
*
* Revision 1.12  2002/12/20 21:47:07  ucko
* Add an argument of type CFeat_CI::EFeat_Location to
* s_FindLongestFeature, and use it for coding regions in s_TitleFromProtein.
*
* Revision 1.11  2002/12/02 16:22:14  ucko
* s_TitleFromProtein: fall back to "unnamed protein product" if nothing
* better is present.
*
* Revision 1.10  2002/11/18 19:48:44  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.9  2002/11/15 17:39:59  ucko
* Make better titles for NM sequences
*
* Revision 1.8  2002/11/08 19:43:38  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.7  2002/08/27 15:25:20  ucko
* Use CSeq_id::IdentifyAccession to improve accession-based tests.
*
* Revision 1.6  2002/08/22 15:36:14  ucko
* Fix stupid MSVC build error by adding yet another redundant .NotEmpty().
*
* Revision 1.5  2002/08/21 15:30:00  ucko
* s_TitleFromProtein: when looking for the organism name, start with the
* actual product, and deal with references to absent sequences.
*
* Revision 1.4  2002/06/28 18:39:20  ucko
* htgs_cancelled keyword suppresses sequencing in progress phrase in defline
*
* Revision 1.3  2002/06/07 16:13:01  ucko
* Move everything into the "sequence" namespace.
*
* Revision 1.2  2002/06/07 13:22:09  clausen
* Removed commented out include for object_manager
*
* Revision 1.1  2002/06/06 18:49:45  clausen
* Initial version
*
* ***  Revisions below are from src/objects/objmgr/bioseq_handle2.cpp ***
* Revision 1.13  2002/05/14 18:36:55  ucko
* More HTG title fixes: avoid "1 ... pieces"; phase 3 = "complete sequence"
*
* Revision 1.12  2002/05/08 19:26:53  ucko
* More changes from C version: give more info about WGS sequences,
* fix count of segments to use #gaps + 1 rather than # non-gap pieces.
*
* Revision 1.11  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.10  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.9  2002/04/22 19:16:13  ucko
* Fixed problem that could occur when no title descriptors were present.
*
* Revision 1.8  2002/04/10 20:59:41  gouriano
* moved construction of iterators out of "for" loop initialization:
* Sun Workshop compiler does not call destructors of such objects
* in case we use break to exit the loop
*
* Revision 1.7  2002/03/21 22:22:46  ucko
* Adjust today's fixes for new objmgr API
*
* Revision 1.6  2002/03/21 20:37:16  ucko
* Pull in full bioseq when counting HTG pieces [also in objmgr]
*
* Revision 1.5  2002/03/21 20:09:06  ucko
* Look at parents' title descriptors in some cases.
* Incorporate recent changes from the C version (CreateDefLineEx):
*  * honor wgs (whole genome shotgun) technology.
*  * don't add strain if already present in organism name.
* [Also fixed in old objmgr.]
*
* Revision 1.4  2002/03/21 17:01:20  ucko
* Fix stupid bug in s_FindLongestFeature (also fixed in old objmgr).
*
* Revision 1.3  2002/03/19 19:16:28  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
*
* Revision 1.4  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/16 18:56:28  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
* ===========================================================================
*/
