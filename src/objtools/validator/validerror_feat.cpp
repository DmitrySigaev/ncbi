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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of Seq_feat
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/serialbase.hpp>

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/scope.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/MolInfo.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objects/util/sequence.hpp>

#include <algorithm>
#include <string>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_feat::CValidError_feat(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_feat::~CValidError_feat(void)
{
}


void CValidError_feat::ValidateSeqFeat(const CSeq_feat& feat)
{
    CBioseq_Handle bsh;
    bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( bsh ) {
        m_Imp.ValidateSeqLoc(feat.GetLocation(), bsh.GetBioseq(), "Location", 
            feat);
    }
    
    if ( feat.IsSetProduct() ) {
        ValidateSeqFeatProduct(feat.GetProduct(), feat);
    }
    
    ValidateFeatPartialness(feat);
    
    ValidateExcept(feat);
    
    ValidateSeqFeatData(feat.GetData(), feat);
    
    if (feat.IsSetDbxref ()) {
        m_Imp.ValidateDbxref (feat.GetDbxref (), feat);
    }
    
    if ( feat.IsSetComment() ) {
        ValidateFeatComment(feat.GetComment(), feat);
    }

    if ( feat.IsSetCit() ) {
        ValidateFeatCit(feat.GetCit(), feat);
    }
}


// =============================================================================
//                                     Private
// =============================================================================


// static member initializations
const string s_PlastidTxt[] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "",
  "",
  "plastid",
  "",
  "",
  "",
  "",
  "",
  "cyanelle",
  "",
  "",
  "",
  "apicoplast",
  "leucoplast",
  "proplastid",
  ""
};


static string s_LegalRepeatTypes[] = {
  "tandem", "inverted", "flanking", "terminal",
  "direct", "dispersed", "other"
};


static string s_LegalConsSpliceStrings[] = {
  "(5'site:YES, 3'site:YES)",
  "(5'site:YES, 3'site:NO)",
  "(5'site:YES, 3'site:ABSENT)",
  "(5'site:NO, 3'site:YES)",
  "(5'site:NO, 3'site:NO)",
  "(5'site:NO, 3'site:ABSENT)",
  "(5'site:ABSENT, 3'site:YES)",
  "(5'site:ABSENT, 3'site:NO)",
  "(5'site:ABSENT, 3'site:ABSENT)",
};


// private member functions:

void CValidError_feat::ValidateSeqFeatData
(const CSeqFeatData& data,
 const CSeq_feat& feat)
{
    switch ( data.Which () ) {
    case CSeqFeatData::e_Gene:
        // Validate CGene_ref
        ValidateGene(data.GetGene (), feat);
        break;
    case CSeqFeatData::e_Cdregion:
        // Validate CCdregion
        ValidateCdregion(data.GetCdregion (), feat);
        break;
    case CSeqFeatData::e_Prot:
        // Validate CProt_ref
        ValidateProt(data.GetProt (), feat);
        break;
    case CSeqFeatData::e_Rna:
        // Validate CRNA_ref
        ValidateRna(data.GetRna (), feat);
        break;
    case CSeqFeatData::e_Pub:
        // Validate CPubdesc
        m_Imp.ValidatePubdesc(data.GetPub (), feat);
        break;
    case CSeqFeatData::e_Imp:
        // Validate CPubdesc
        ValidateImp(data.GetImp (), feat);
        break;
    case CSeqFeatData::e_Biosrc:
        // Validate CBioSource
        ValidateFeatBioSource(data.GetBiosrc(), feat);
        break;
    default:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidType,
            "Invalid SeqFeat type [" + 
            NStr::IntToString(data.Which ()) +
            "]", feat);
        break;
    }
}


void CValidError_feat::ValidateSeqFeatProduct
(const CSeq_loc& prod, const CSeq_feat& feat)
{
    CBioseq_Handle bsh;
    bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( bsh ) {
        m_Imp.ValidateSeqLoc(feat.GetProduct(), bsh.GetBioseq(), "Product",
            feat);
    }
    
    if ( IsOneBioseq(prod, m_Scope) ) {
        const CSeq_id& sid = GetId(prod, m_Scope);
    
        switch ( sid.Which() ) {
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            {
                const CTextseq_id* tsid = sid.GetTextseq_Id();
                if ( tsid != NULL ) {
                    if ( !tsid->IsSetAccession()  &&  tsid->IsSetName() ) {
                        if ( m_Imp.IsNucAcc(tsid->GetName()) ) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                "Feature product should not use "
                                "Textseq-id 'name' slot", feat);
                        }
                    }
                }
            }
            break;
            
        default:
            break;
        }
    }
}


bool CValidError_feat::IsPlastid(int genome)
{
    if ( genome == CBioSource::eGenome_chloroplast  ||
         genome == CBioSource::eGenome_chromoplast  ||
         genome == CBioSource::eGenome_plastid      ||
         genome == CBioSource::eGenome_cyanelle     ||
         genome == CBioSource::eGenome_apicoplast   ||
         genome == CBioSource::eGenome_leucoplast   ||
         genome == CBioSource::eGenome_proplastid  ) { 
        return true;
    }

    return false;
}


bool CValidError_feat::IsOverlappingGenePseudo(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp  ) {
        return (grp->IsSetPseudo()  &&  grp->GetPseudo());
    }

    // !!! DEBUG {
    // For testing purposes. Remove when test is done.
    if ( m_Imp.AvoidPerfBottlenecks() ) {
        return false;
    }
    // }

    // check overlapping gene
    CConstRef<CSeq_feat> overlap = GetBestOverlappingFeat(
        feat.GetLocation(),
        CSeqFeatData::e_Gene,
        eOverlap_Contains,
        *m_Scope);
    if ( overlap ) {
        if ( (overlap->IsSetPseudo()  &&  overlap->GetPseudo())  ||
             (overlap->GetData().GetGene().IsSetPseudo()  &&
              overlap->GetData().GetGene().GetPseudo()) ) {
            return true;
        }
    }

    return false;
}


bool CValidError_feat::SuppressCheck(const string& except_text)
{
    static string exceptions[] = {
        "ribosomal slippage",
        "ribosome slippage",
        "artificial frameshift",
        "non-consensus splice site",
        "nonconsensus splice site"
    };

    for ( size_t i = 0; i < sizeof(exceptions) / sizeof(string); ++i ) {
    if ( NStr::FindNoCase(except_text, exceptions[i] ) != string::npos )
         return true;
    }
    return false;
}





unsigned char CValidError_feat::Residue(unsigned char res)
{
    return res == 255 ? '?' : res;
}


int CValidError_feat::CheckForRaggedEnd
(const CSeq_loc& loc, 
 const CCdregion& cdregion)
{
    size_t len = GetLength(loc, m_Scope);
    if ( cdregion.GetFrame() > CCdregion::eFrame_one ) {
        len -= cdregion.GetFrame() - 1;
    }

    int ragged = len % 3;
    if ( ragged > 0 ) {
        len = GetLength(loc, m_Scope);

        CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();
        ITERATE( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
            SRelLoc rl(loc, (*cbr)->GetLoc(), m_Scope);
            CRef<CSeq_loc> rel_loc = rl.Resolve(m_Scope);
            range += rel_loc->GetTotalRange();
        }

        // allowing a partial codon at the end
        TSeqPos codon_length = range.GetLength();
        if ( (codon_length == 0 || codon_length == 1)  && 
            range.GetTo() == len - 1 ) {
            ragged = 0;
        }
    }
    return ragged;
}


string CValidError_feat::MapToNTCoords
(const CSeq_feat& feat,
 const CSeq_loc& product,
 TSeqPos pos)
{
    string result;

    CSeq_point pnt;
    pnt.SetPoint(pos);
    pnt.SetStrand( GetStrand(product, m_Scope) );

    try {
        pnt.SetId().Assign(GetId(product, m_Scope));
    } catch (CNotUnique) {}

    CSeq_loc tmp;
    tmp.SetPnt(pnt);
    CRef<CSeq_loc> loc = ProductToSource(feat, tmp, 0, m_Scope);
    
    loc->GetLabel(&result);

    return result;
}


void CValidError_feat::ValidateFeatPartialness(const CSeq_feat& feat)
{
    unsigned int  partial_prod = eSeqlocPartial_Complete, 
        partial_loc = eSeqlocPartial_Complete;
    static string parterr[2] = { "PartialProduct", "PartialLocation" };
    static string parterrs[4] = {
        "Start does not include first/last residue of sequence",
        "Stop does not include first/last residue of sequence",
        "Internal partial intervals do not include first/last residue of sequence",
        "Improper use of partial (greater than or less than)"
    };

    partial_loc  = SeqLocPartialCheck(feat.GetLocation(), m_Scope );
    if (feat.IsSetProduct ()) {
        partial_prod = SeqLocPartialCheck(feat.GetProduct (), m_Scope );
    }
    
    if ( (partial_loc  != eSeqlocPartial_Complete)  ||
         (partial_prod != eSeqlocPartial_Complete)  ||   
         (feat.IsSetPartial()  &&  feat.GetPartial() == true) ) {

        // a feature on a partial sequence should be partial -- it often isn't
        if ( (!feat.IsSetPartial()  ||  !feat.GetPartial()) &&
            partial_loc != eSeqlocPartial_Complete  &&
            feat.GetLocation ().Which () == CSeq_loc::e_Whole ) {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                "On partial Bioseq, SeqFeat.partial should be TRUE", feat);
        }
        // a partial feature, with complete location, but partial product
        else if ( (feat.IsSetPartial()  &&  feat.GetPartial())  &&
            partial_loc == eSeqlocPartial_Complete  &&
            feat.IsSetProduct () &&
            feat.GetProduct ().Which () == CSeq_loc::e_Whole  &&
            partial_prod != eSeqlocPartial_Complete ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "When SeqFeat.product is a partial Bioseq, SeqFeat.location "
                "should also be partial", feat);
        }
        // gene on segmented set is now 'order', should also be partial
        else if ( feat.GetData ().IsGene ()  &&
            !feat.IsSetProduct ()  &&
            partial_loc == eSeqlocPartial_Internal ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "Gene of 'order' with otherwise complete location should "
                "have partial flag set", feat);
        }
        // inconsistent combination of partial/complete product,location,partial flag
        else if ( (partial_prod == eSeqlocPartial_Complete  &&  feat.IsSetProduct ())  ||
            partial_loc == eSeqlocPartial_Complete  ||
            !feat.GetPartial () ) {
            string str("Inconsistent: ");
            if ( feat.IsSetProduct () ) {
                str += "Product= ";
                if ( partial_prod != eSeqlocPartial_Complete ) {
                    str += "partial, ";
                } else {
                    str += "complete, ";
                }
            }
            str += "Location= ";
            if ( partial_loc != eSeqlocPartial_Complete ) {
                str += "partial, ";
            } else {
                str += "complete, ";
            }
            str += "Feature.partial= ";
            if ( feat.IsSetPartial()  &&  feat.GetPartial() ) {
                str += "TRUE";
            } else {
                str += "FALSE";
            }
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem, str, feat);
        }
        // 5' or 3' partial location giving unclassified partial product
        else if ( (partial_loc & eSeqlocPartial_Start  ||
                   partial_loc & eSeqlocPartial_Stop)  &&
                   partial_prod & eSeqlocPartial_Other &&
                   feat.IsSetPartial()  &&  feat.GetPartial() ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "5' or 3' partial location should not have unclassified "
                "partial location", feat);
        }
        
        // may have other error bits set as well 
        unsigned int partials[2] = { partial_prod, partial_loc };
        for ( int i = 0; i < 2; ++i ) {
            unsigned int errtype = eSeqlocPartial_Nostart;

            for ( int j = 0; j < 4; ++j ) {
                if (partials[i] & errtype) {
                    if ( i == 1  &&  j < 2  &&
                        IsPartialAtSpliceSite(feat.GetLocation(), errtype) ) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ":" + parterrs[j] + 
                            "(but is at consensus splice site)", feat);
                    } else if ( i == 1  &&  j < 2  &&
                        (feat.GetData().Which() == CSeqFeatData::e_Gene  ||
                        feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) &&
                         IsSameAsCDS(feat) ) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j], feat);
                    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 0) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 5' partial is not at start AND is not at consensus splice site",
                            feat); 
                    } else if (feat.GetData().Which() == CSeqFeatData::e_Cdregion && j == 1) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 3' partial is not at stop AND is not at consensus splice site",
                            feat);
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j], feat);
                    }
                }
                errtype <<= 1;
            }
        }
    }
}


void CValidError_feat::ValidateGene(const CGene_ref& gene, const CSeq_feat& feat)
{
    if ( gene.GetLocus().empty()      &&
         gene.GetAllele().empty()     &&
         gene.GetDesc().empty()       &&
         gene.GetMaploc().empty()     &&
         gene.GetDb().empty()         &&
         gene.GetSyn().empty()        &&
         gene.GetLocus_tag().empty()  ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneRefHasNoData,
                  "There is a gene feature where all fields are empty", feat);
    }
    if (gene.IsSetDb ()) {
        m_Imp.ValidateDbxref(gene.GetDb(), feat);
    }
    ValidateGeneXRef(feat);
}


void CValidError_feat::ValidateCdregion (
    const CCdregion& cdregion, 
    const CSeq_feat& feat
) 
{
    
    ITERATE( list< CRef< CGb_qual > >, qual, feat.GetQual () ) {
        if ( (**qual).GetQual() == "codon" ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                "Use the proper genetic code, if available, "
                "or set transl_excepts on specific codons", feat);
            break;
        }
    }

    bool pseudo = feat.GetPseudo()  ||  IsOverlappingGenePseudo(feat);
    if ( !pseudo && !cdregion.GetConflict() ) {
        ValidateCdTrans(feat);
        ValidateSplice(feat, false);
    }
    
    ITERATE( CCdregion::TCode_break, codebreak, cdregion.GetCode_break() ) {
        ECompare comp = sequence::Compare((**codebreak).GetLoc (),
            feat.GetLocation (), m_Scope );
        if ( (comp != eContained) && (comp != eSame))
            PostErr (eDiag_Error, eErr_SEQ_FEAT_Range, 
                "Code-break location not in coding region", feat);
    }

    if ( cdregion.GetOrf () && feat.IsSetProduct () ) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_OrfCdsHasProduct,
            "An ORF coding region should not have a product", feat);
    }

    if ( pseudo && feat.IsSetProduct () ) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PsuedoCdsHasProduct,
            "A pseudo coding region should not have a product", feat);
    }
    
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (feat.GetLocation ());
    if ( bsh ) {
        CSeqdesc_CI diter (bsh, CSeqdesc::e_Source);
        if ( diter ) {
            const CBioSource& src = diter->GetSource();
            
            int biopgencode = src.GetGenCode();
            
            if (cdregion.IsSetCode ()) {
                int cdsgencode = cdregion.GetCode().GetId();
                
                if ( biopgencode != cdsgencode ) {
                    int genome = 0;
                    
                    if ( src.IsSetGenome() ) {
                        genome = src.GetGenome();
                    }
                    
                    if ( IsPlastid(genome) ) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                            "Genetic code conflict between CDS (code " +
                            NStr::IntToString (cdsgencode) +
                            ") and BioSource.genome biological context (" +
                            s_PlastidTxt [genome] + ") (uses code 11)", feat);
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                            "Genetic code conflict between CDS (code " +
                            NStr::IntToString (cdsgencode) +
                            ") and BioSource (code " +
                            NStr::IntToString (biopgencode) + ")", feat);
                    }
                }
            }
        }
    }

    ValidateBothStrands(feat);
    ValidateBadGeneOverlap(feat);
    ValidateBadMRNAOverlap(feat);
    ValidateCommonCDSProduct(feat);
}



void CValidError_feat::ValidateSplice(const CSeq_feat& feat, bool check_all)
{
    // !!! suppress if NCBISubValidate
    //if (GetAppProperty ("NcbiSubutilValidation") != NULL)
    //    return;

    // specific biological exceptions suppress check
    if ( feat.GetExcept()  &&  feat.IsSetExcept_text() ) {
        if ( SuppressCheck(feat.GetExcept_text()) ) {
            return;
        }
    }
        
    size_t num_of_parts = 0;
    ENa_strand  strand = eNa_strand_unknown;

    // !!! The C version treated seq_loc equiv as one whereas the iterator
    // treats it as many. 
    const CSeq_loc& location = feat.GetLocation ();

    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++num_of_parts;
        if ( num_of_parts == 1 ) {  // first part
            strand = citer.GetStrand();
        } else {
            if ( strand != citer.GetStrand() ) {
                return;         //bail on mixed strand
            }
        }
    }

    if ( num_of_parts == 0 ) {
        return;
    }
    if ( !check_all  &&  num_of_parts == 1 ) {
        return;
    }
    
    bool partial_first = location.IsPartialLeft();
    bool partial_last = location.IsPartialRight();

    size_t counter = 0;
    const CSeq_id* last_id = 0;

    CBioseq_Handle bsh;
    size_t seq_len = 0;
    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++counter;

        const CSeq_id& seq_id = citer.GetSeq_id();
        
        if ( last_id == 0 || !last_id->Match(seq_id) ) {
            bsh = m_Scope->GetBioseqHandle(seq_id);
            if ( !bsh ) {
                break;
            }
            seq_len = bsh.GetSeqVector().size();
            last_id = &seq_id;
        }

        TSeqPos acceptor = citer.GetRange().GetFrom();
        TSeqPos donor = citer.GetRange().GetTo();

        TSeqPos start = acceptor;
        TSeqPos stop = donor;

        if ( citer.GetStrand() == eNa_strand_minus ) {
            swap(acceptor, donor);
            stop = seq_len - donor - 1;
            start = seq_len - acceptor - 1;
        }

        // set severity level
        // genomic product set or NT_ contig always relaxes to SEV_WARNING
        EDiagSev sev = eDiag_Warning;
        if ( m_Imp.IsSpliceErr()                   &&
             !(m_Imp.IsGPS() || m_Imp.IsRefSeq())  &&
             !check_all ) {
            sev = eDiag_Error;
        }

        // get the label. if m_SuppressContext flag in true, get the worst label.
        const CBioseq& bsq = bsh.GetBioseq();
        string label;
        bsq.GetLabel(&label, CBioseq::eContent, m_Imp.IsSuppressContext());
        
        CSeqVector seq_vec =
            bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac, citer.GetStrand());
        if ( !m_Imp.CheckSeqVector(seq_vec) ) {
            break;
        }

        // check donor on all but last exon and on sequence
        if ( ((check_all && !partial_last)  ||  counter < num_of_parts)  &&
             (stop < seq_len - 2) ) {
            CSeqVector::TResidue res1 = seq_vec[stop + 1];    
            CSeqVector::TResidue res2 = seq_vec[stop + 2];

            if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                if ( (res1 != 'G')  || (res2 != 'T' ) ) {
                    string msg;
                    if ( (res1 == 'G')  && (res2 == 'C' ) ) { // GC minor splice site
                        sev = eDiag_Info;
                        msg = "Rare splice donor consensus (GC) found instead of "
                              "(GT) after exon ending at position " +
                              NStr::IntToString(donor + 1) + " of " + label;
                    } else {
                        msg = "Splice donor consensus (GT) not found after exon"
                            " ending at position " + 
                            NStr::IntToString(donor + 1) + " of " + label;
                    }
                    PostErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus, msg, feat);
                }
            }
        }

        if ( ((check_all && !partial_first)  ||  counter != 1)  &&
             (start > 1) ) {
            CSeqVector::TResidue res1 = seq_vec[start - 2];
            CSeqVector::TResidue res2 = seq_vec[start - 1];

            if ( IsResidue(res1)  &&  IsResidue(res2) ) {
                if ( (res1 != 'A')  ||  (res2 != 'G') ) {
                    PostErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus,
                        "Splice acceptor consensus (AG) not found before "
                        "exon starting at position " + 
                        NStr::IntToString(acceptor + 1) + " of " + label, feat);
                }
            }
        }
    } // end of for loop
}


void CValidError_feat::ValidateProt(const CProt_ref& prot, const CSerialObject& obj) 
{
    if ( prot.GetProcessed() != CProt_ref::eProcessed_signal_peptide  &&
         prot.GetProcessed() != CProt_ref::eProcessed_transit_peptide ) {
        if ( (prot.GetName().empty()  ||  prot.GetName().front().empty())  &&
             prot.GetDesc().empty()  &&  prot.GetEc().empty()  &&
             prot.GetActivity().empty()  &&  prot.GetDb().empty() ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProtRefHasNoData,
                  "There is a protein feature where all fields are empty", obj);
        }
    }
    if (prot.IsSetDb ()) {
        m_Imp.ValidateDbxref(prot.GetDb(), obj);
    }
}


void CValidError_feat::ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat) 
{
    const CRNA_ref::EType& rna_type = rna.GetType ();

    if ( rna_type == CRNA_ref::eType_mRNA ) {
        bool pseudo = (feat.IsSetPseudo()  &&  feat.GetPseudo())  ||  IsOverlappingGenePseudo(feat);
        
        if ( !pseudo ) {
            ValidateMrnaTrans(feat);      /* transcription check */
            ValidateSplice(feat, false);
        }
        ValidateBothStrands(feat);
        ValidateBadGeneOverlap(feat);
        ValidateCommonMRNAProduct(feat);
    }

    if ( rna.IsSetExt()  &&
         rna.GetExt().Which() == CRNA_ref::C_Ext::e_TRNA ) {
        const CTrna_ext& trna = rna.GetExt ().GetTRNA ();
        if ( trna.IsSetAnticodon () ) {
            ECompare comp = sequence::Compare(trna.GetAnticodon(), 
                                              feat.GetLocation());
            if ( comp != eContained  &&  comp != eSame ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_Range,
                    "Anticodon location not in tRNA", feat);
            }
            if ( GetLength( trna.GetAnticodon (), m_Scope ) != 3 ) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_Range,
                    "Anticodon is not 3 bases in length", feat);
            }
        }
        ValidateTrnaCodons(trna, feat);
    }

    if ( rna_type == CRNA_ref::eType_tRNA ) {
        ITERATE ( list< CRef< CGb_qual > >, gbqual, feat.GetQual () ) {
            if ( NStr::CompareNocase((**gbqual).GetVal (), "anticodon") == 0 ) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed anticodon qualifier in tRNA", feat);
                break;
            }
        }
        /* tRNA with string extension */
        if ( rna.IsSetExt()  &&  
             rna.GetExt().Which () == CRNA_ref::C_Ext::e_Name ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                "Unparsed product qualifier in tRNA", feat);
        }
    }

    if ( rna_type == CRNA_ref::eType_unknown ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RNAtype0,
            "RNA type 0 (unknown) not supported", feat);
    }

    if ( feat.IsSetProduct() ) {
        ValidateRnaProductType(rna, feat);
    }
}


void CValidError_feat::ValidateRnaProductType
(const CRNA_ref& rna,
 const CSeq_feat& feat)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !bsh ) {
        return;
    }
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if ( !di ) {
        return;
    }
    const CMolInfo& mol_info = di->GetMolinfo();
    if ( !mol_info.IsSetBiomol() ) {
        return;
    }
    int biomol = mol_info.GetBiomol();
    
    switch ( rna.GetType() ) {

    case CRNA_ref::eType_mRNA:
        if ( biomol == CMolInfo::eBiomol_mRNA ) {
            return;
        }        
        break;

    case CRNA_ref::eType_tRNA:
        if ( biomol == CMolInfo::eBiomol_tRNA ) {
            return;
        }
        break;

    case CRNA_ref::eType_rRNA:
        if ( biomol == CMolInfo::eBiomol_snRNA ) {
            return;
        }
        break;

    default:
        return;
    }

    PostErr(eDiag_Error, eErr_SEQ_FEAT_RnaProductMismatch,
        "Type of RNA does not match MolInfo of product Bioseq", feat);
}


void CValidError_feat::ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat)
{
    // Make sure AA coding is ncbieaa.
    if ( trna.GetAa().Which() != CTrna_ext::C_Aa::e_Ncbieaa ) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_TrnaCodonWrong,
            "tRNA AA coding does not match ncbieaa", feat);
        return;
    }
    
    // Retrive the Genetic code id for the tRna
    int gcode = 0;
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( !bsh ) {
        gcode = 1;
    } else {
        // need only the closest biosoure.
        CSeqdesc_CI diter(bsh, CSeqdesc::e_Source);
        if ( diter ) {
            gcode = diter->GetSource().GetGenCode();
        }
    }
    
    const string& ncbieaa = CGen_code_table::GetNcbieaa(gcode);
    if ( ncbieaa.length() != 64 ) {
        return;  // !!!  need to issue a warning/error?
    }
    
    ITERATE( CTrna_ext::TCodon, iter, trna.GetCodon() ) {
        if ( *iter < 64 ) {  // 0-63 = codon,  255=no data in cell
            unsigned char taa = ncbieaa[*iter];
            unsigned char  aa = trna.GetAa().GetNcbieaa();
            if ( (aa > 0)  &&  (aa != 255) ) {
                if ( taa != aa ) {
                    EDiagSev sev = (aa == 'U') ? eDiag_Warning : eDiag_Error;
                    PostErr(sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                        "tRNA codon does not match genetic code", feat);
                }
            }
        }
    }
}


void CValidError_feat::ValidateImp(const CImp_feat& imp, const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
    string key = imp.GetKey();

    switch ( subtype ) {
    case CSeqFeatData::eSubtype_exon:
        if ( m_Imp.IsValidateExons() ) {
            ValidateSplice(feat, true);
        }
        break;

    case CSeqFeatData::eSubtype_bad:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
            "Unknown feature key " + key, feat);
        break;

    case CSeqFeatData::eSubtype_virion:
    case CSeqFeatData::eSubtype_mutation:
    case CSeqFeatData::eSubtype_allele:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Feature key " + key + " is no longer legal", feat);
        break;

    case CSeqFeatData::eSubtype_polyA_site:
        {
            CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
            if ( range.GetFrom() != range.GetTo() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PolyAsiteNotPoint,
                    "PolyA_site should be a single point", feat);
            }
        }
        break;

    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_sig_peptide:
    case CSeqFeatData::eSubtype_transit_peptide:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidForType,
            "Peptide processing feature should be converted to the "
            "appropriate protein feature subtype", feat);
        ValidatePeptideOnCodonBoundry(feat, key);
        break;
        
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_tRNA:
    case CSeqFeatData::eSubtype_rRNA:
    case CSeqFeatData::eSubtype_snRNA:
    case CSeqFeatData::eSubtype_scRNA:
    case CSeqFeatData::eSubtype_snoRNA:
    case CSeqFeatData::eSubtype_misc_RNA:
    case CSeqFeatData::eSubtype_precursor_RNA:
    // !!! what about other RNA types (preRNA, otherRNA)?
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
              "RNA feature should be converted to the appropriate RNA feature "
              "subtype, location should be converted manually", feat);
        break;

    case CSeqFeatData::eSubtype_Imp_CDS:
        {
            // impfeat CDS must be pseudo; fail if not
            bool pseudo = feat.GetPseudo()  ||  IsOverlappingGenePseudo(feat);
            if ( !pseudo ) {
                PostErr(eDiag_Info, eErr_SEQ_FEAT_ImpCDSnotPseudo,
                    "ImpFeat CDS should be pseudo", feat);
            }

            ITERATE( CSeq_feat::TQual, gbqual, feat.GetQual() ) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "translation") == 0 ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpCDShasTranslation,
                        "ImpFeat CDS with /translation found", feat);
                }
            }
        }
        break;

    default:
        break;
    }// end of switch statement  
    
    // validate the feature's location
    if ( imp.IsSetLoc() ) {
        const string& imp_loc = imp.GetLoc();
        if ( imp_loc.find("one-of") != string::npos ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                "ImpFeat loc " + imp_loc + 
                " has obsolete 'one-of' text for feature " + key, feat);
        } else if ( feat.GetLocation().IsInt() ) {
            const CSeq_interval& seq_int = feat.GetLocation().GetInt();
            string temp_loc = NStr::IntToString(seq_int.GetFrom() + 1) +
                ".." + NStr::IntToString(seq_int.GetTo() + 1);
            if ( imp_loc != temp_loc ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                    "ImpFeat loc " + imp_loc + " does not equal feature location " +
                    temp_loc + "for feature " + key, feat);
            }
        }
    }

    if ( feat.IsSetQual() ) {
        ValidateImpGbquals(imp, feat);

        // Make sure a feature has its mandatory qualifiers
        ITERATE( CFeatQualAssoc::TGBQualTypeVec,
                 required,
                 CFeatQualAssoc::GetMandatoryGbquals(subtype) ) {
            bool found = false;
            ITERATE( CSeq_feat::TQual, qual, feat.GetQual() ) {
                if ( CGbqualType::GetType(**qual) == *required ) {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                    "Missing qualifier " + CGbqualType::GetString(*required) +
                    " for feature " + key, feat);
            }
        }
    }
}


void CValidError_feat::ValidateImpGbquals
(const CImp_feat& imp,
 const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype ftype = feat.GetData().GetSubtype();
    const string& key = imp.GetKey();

    ITERATE( CSeq_feat::TQual, qual, feat.GetQual() ) {
        const string& qual_str = (*qual)->GetQual();

        if ( qual_str == "gsdb_id" ) {
            continue;
        }

        CGbqualType::EType gbqual = CGbqualType::GetType(qual_str);
        
        if ( gbqual == CGbqualType::e_Bad ) {
            if ( !qual_str.empty() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Unknown qualifier " + qual_str, feat);
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Empty qualifier", feat);
            }
        } else {
            if ( !CFeatQualAssoc::IsLegalGbqual(ftype, gbqual) ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key, feat);
            }

            string val = (*qual)->GetVal();

            bool error = false;
            switch ( gbqual ) {
            case CGbqualType::e_Rpt_type:
                for ( size_t i = 0; 
                      i < sizeof(s_LegalRepeatTypes) / sizeof(string); 
                      ++i ) {
                    if ( val.find(s_LegalRepeatTypes[i]) != string::npos ) {
                        bool left = false, right = false;
                        if ( i > 0 ) {
                            left = val[i-1] == ','  ||  val[i-1] == '(';
                        }
                        if ( i < val.length() - 1 ) {
                            right = val[i+1] == ','  ||  val[i+1] == ')';
                        }
                        if ( left  &&  right ) {
                            error = true;
                        }
                        break;
                    }
                }
                break;

            case CGbqualType::e_Rpt_unit:
                {
                    bool found = false,
                         multiple_rpt_unit = true;

                    for ( size_t i = 0; i < val.length(); ++i ) {
                        if ( val[i] <= ' ' ) {
                            found = true;
                        } else if ( val[i] == '('  ||  val[i] == ')'  ||
                                    val[i] == ','  ||  val[i] == '.'  ||
                                    isdigit(val[i]) ) {
                        } else {
                            multiple_rpt_unit = false;
                        }
                    }
                    if ( found || 
                         (!multiple_rpt_unit && val.length() > 48) ) {
                        error = true;
                    }
                    break;
                }
                
            case CGbqualType::e_Label:
                {
                    for ( size_t i = 0; i < val.length(); ++i ) {
                        if ( isspace(val[i])  ||  isdigit(val[i]) ) {
                            error = true;
                            break;
                        }
                    }
                }                
                break;
                
            case CGbqualType::e_Cons_splice:
                { 
                    error = true;
                    for ( size_t i = 0; 
                          i < sizeof(s_LegalConsSpliceStrings) / sizeof(string); 
                          ++i ) {
                        if ( NStr::CompareNocase(val, s_LegalConsSpliceStrings[i]) == 0 ) {
                            error = false;
                            break;
                        }
                    }
                }
                break;

	    default:
	        break;
            }
            if ( error ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    val + " is not a legal value for qualifier " + qual_str, feat);
            }
        }
    }
}


void CValidError_feat::ValidatePeptideOnCodonBoundry
(const CSeq_feat& feat, 
 const string& key)
{
    const CSeq_loc& loc = feat.GetLocation();

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG

    CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::e_Cdregion,
        eOverlap_Contains,
        *m_Scope);
    if ( !cds ) {
        return;
    }
    const CCdregion& cdr = cds->GetData().GetCdregion();

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) {
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }
        
    if ( !first  ||  !last ) {
        return;
    }

    TSeqPos pos1 = LocationOffset(loc, first.GetSeq_loc(), eOffset_FromStart);
    TSeqPos pos2 = LocationOffset(loc, last.GetSeq_loc(), eOffset_FromEnd);
    TSeqPos mod1 = (pos1 - cdr.GetFrame()) %3;
    TSeqPos mod2 = (pos2 - cdr.GetFrame()) %3;

    if ( loc.IsPartialLeft() ) {
        mod1 = 0;
    }
    if ( loc.IsPartialRight() ) {
        mod2 = 2;
    }

    if ( (mod1 != 0)  &&  (mod2 != 2) ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
            "Start and stop of " + key + " are out of frame with CDS codons",
            feat);
    } else if (mod1 != 0) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame, 
            "Start of " + key + " is out of frame with CDS codons", feat);
    } else if (mod2 != 2) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
            "Stop of " + key + " is out of frame with CDS codons", feat);
    }
}


void CValidError_feat::ValidateMrnaTrans(const CSeq_feat& feat)
{
    static string s_BypassMrnaTransCheck[] = {
        "RNA editing",
        "reasons given in citation",
        "artificial frameshift",
        "unclassified transcription discrepancy",
    };

    if ( feat.GetPseudo() ) {
        return;
    }
    if ( !feat.IsSetProduct() ) {
        return;
    }

    // biological exception
    
    if ( feat.GetExcept()  &&  feat.IsSetExcept_text() ) {
        size_t except_num = sizeof(s_BypassMrnaTransCheck) / sizeof(string);
        for ( size_t i = 0; i < except_num; ++i ) {
            if ( NStr::FindNoCase(feat.GetExcept_text(), 
                s_BypassMrnaTransCheck[i]) != string::npos ) {
                return; 
            }
        }
    }

    CBioseq_Handle nuc = m_Scope->GetBioseqHandle(feat.GetLocation());
    CBioseq_Handle rna = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !nuc  ||  !rna ) {
        return;
    }

    CSeqVector nuc_vec = nuc.GetSequenceView(feat.GetLocation(),
        CBioseq_Handle::eViewConstructed,
        CBioseq_Handle::eCoding_Iupac);
    CSeqVector rna_vec = rna.GetSequenceView(feat.GetProduct(),
        CBioseq_Handle::eViewConstructed,
        CBioseq_Handle::eCoding_Iupac);

    if ( nuc_vec.size() != rna_vec.size() ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_TranscriptLen,
            "Transcript length [" + NStr::IntToString(nuc_vec.size()) + "] " +
            "does not match product length [" + 
            NStr::IntToString(rna_vec.size()) + "]", feat);
    } else if ( nuc_vec.size() > 0 ) {
        size_t mismatches = 0;
        for ( size_t i = 0; i < nuc_vec.size(); ++i ) {
            if ( nuc_vec[i] != rna_vec[i] ) {
                ++mismatches;
            }
        }
        if ( mismatches > 0 ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_TranscriptMismatches,
                "There are " + NStr::IntToString(mismatches) + 
                " mismatches out of " + NStr::IntToString(nuc_vec.size()) +
                " bases between the transcript and product sequence", feat);
        }
    }
}


void CValidError_feat::ValidateCommonMRNAProduct(const CSeq_feat& feat)
{
    if ( feat.GetPseudo()  ||  IsOverlappingGenePseudo(feat) ) {
        return;
    }

    if ( !feat.IsSetProduct() ) {
        return;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !bsh ) {
        const CSeq_id& sid = GetId(feat.GetProduct(), m_Scope);
        if ( sid.IsLocal() ) {
            if ( m_Imp.IsGPS()  ||  m_Imp.IsRefSeq() ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MissingMRNAproduct,
                    "Product Bioseq of mRNA feature is not "
                    "packaged in the record", feat);
            }
        }
    } else {

        // !!! DEBUG {
        if( m_Imp.AvoidPerfBottlenecks() ) {
            return;
        } 
        // } DEBUG

        CFeat_CI mrna(
            bsh, 
            0, 0,
            CSeqFeatData::e_Rna,
            SAnnotSelector::eOverlap_TotalRange,
            CFeat_CI::eResolve_None,
            CFeat_CI::e_Product);
        while ( mrna ) {
            if ( &(mrna->GetOriginalFeature()) != &feat ) {
                    PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleMRNAproducts,
                        "Same product Bioseq from multiple mRNA features", feat);
                    break;
            }
            ++mrna;
        }
    }
}


void CValidError_feat::ValidateBothStrands(const CSeq_feat& feat)
{
    const CSeq_loc& location = feat.GetLocation ();
    for ( CSeq_loc_CI citer (location); citer; ++citer ) {
        if ( citer.GetStrand () == eNa_strand_both ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_BothStrands, 
                "mRNA or CDS may not be on both strands", feat);  
            break;
        }
    }
}


// Precondition: feat is a coding region
void CValidError_feat::ValidateCommonCDSProduct
(const CSeq_feat& feat)
{
    if ( feat.GetPseudo()  ||  IsOverlappingGenePseudo(feat) ) {
        return;
    }
    
    if ( !feat.IsSetProduct() ) {
        return;
    }
    
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if ( cdr.IsSetOrf() ) {
        return;
    }

    CBioseq_Handle prod = m_Scope->GetBioseqHandle(feat.GetProduct());
    if ( !prod ) {
        const CSeq_id* sid = 0;
        try {
            sid = &(GetId(feat.GetProduct(), m_Scope));
        } catch (const CNotUnique&) {}

        // okay to have far RefSeq product, but only if genomic product set
        if ( sid == 0  ||  !sid->IsOther() ) {
            if ( m_Imp.IsGPS() ) {
                return;
            }
        }
        // or just a bioseq
        if ( m_Imp.GetTSE().IsSeq() ) {
            return;
        }

        // or in a standalone Seq-annot
        if ( m_Imp.IsStandaloneAnnot() ) {
            return;
        }

        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultipleCDSproducts,
            "Unable to find product Bioseq from CDS feature", feat);
        return;
    }
    CBioseq_Handle nuc  = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( nuc ) {
        const CSeq_entry* prod_nps = 
            m_Imp.GetAncestor(prod.GetBioseq(), CBioseq_set::eClass_nuc_prot);
        const CSeq_entry* nuc_nps = 
            m_Imp.GetAncestor(nuc.GetBioseq(), CBioseq_set::eClass_nuc_prot);

        if ( (prod_nps != nuc_nps)  &&  (!m_Imp.IsNT())  &&  (!m_Imp.IsGPS()) ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                "Protein product not packaged in nuc-prot set with nucleotide", 
                feat);
        }
    }
    CConstRef<CSeq_feat> sfp = m_Imp.GetCDSGivenProduct(prod.GetBioseq());
    if ( !sfp ) {
        return;
    }
    
    if ( &feat != sfp.GetPointer() ) {
        // if genomic product set, with one cds on contig and one on cdna,
        // do not report.
        if ( m_Imp.IsGPS() ) {
            // feature packaging test will do final contig vs. cdna check
            CBioseq_Handle sfh = m_Scope->GetBioseqHandle(sfp->GetLocation());
            if ( nuc != sfh ) {
                return;
            }
        }
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleCDSproducts, 
            "Same product Bioseq from multiple CDS features", feat);
    }
}


void CValidError_feat::ValidateBadMRNAOverlap(const CSeq_feat& feat)
{
    const CSeq_loc& loc = feat.GetLocation();

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG

    CConstRef<CSeq_feat> mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Simple,
        *m_Scope);
    if ( !mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_CheckIntervals,
        *m_Scope);
    if ( mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Interval,
        *m_Scope);
    if ( !mrna ) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Subset,
        *m_Scope);
    if ( !mrna ) {
        return;
    }

    EDiagSev sev = eDiag_Error;
    if ( m_Imp.IsNC()  ||  m_Imp.IsNT()  ||  feat.GetExcept() ) {
        sev = eDiag_Warning;
    }
    if ( mrna ) {
        PostErr(sev, eErr_SEQ_FEAT_CDSmRNArange,
            "mRNA contains CDS but internal intron-exon boundaries "
            "do not match", feat);
    } else {
        PostErr(sev, eErr_SEQ_FEAT_CDSmRNArange,
            "mRNA overlaps or contains CDS but does not completely "
            "contain intervals", feat);
    }
}


void CValidError_feat::ValidateBadGeneOverlap(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp != 0 ) {
        return;
    }

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG
    
    // !!! can't look for contating feature using GetBestOverlap.
    // This implementation should be changed once the above is supported

    // look for overlapping genes
    CConstRef<CSeq_feat> gene = GetBestOverlappingFeat(
        feat.GetLocation(),
        CSeqFeatData::e_Gene,
        eOverlap_Contains,
        *m_Scope);
    if ( gene ) {
        return;
    }


    // look for intersecting genes
    gene = GetBestOverlappingFeat(
        feat.GetLocation(),
        CSeqFeatData::e_Gene,
        eOverlap_Simple,
        *m_Scope);
    if ( !gene ) {
        return;
    }

    // found an intersecting (but not overlapping) gene
    EDiagSev sev = eDiag_Error;
    if ( m_Imp.IsNC()  ||  m_Imp.IsNT() ) {
        sev = eDiag_Warning;
    }

    if ( feat.GetData().IsCdregion() ) {
        PostErr(sev, eErr_SEQ_FEAT_CDSgeneRange, 
            "gene overlaps CDS but does not completely contain it", feat);
    } else if ( feat.GetData().IsRna() ) {
        PostErr(sev, eErr_SEQ_FEAT_mRNAgeneRange,
            "gene overlaps mRNA but does not completely contain it", feat);
    }
}


static const string s_LegalExceptionStrings[] = {
  "RNA editing",
  "reasons given in citation",
  "ribosomal slippage",
  "ribosome slippage",
  "trans splicing",
  "trans-splicing",
  "alternative processing",
  "alternate processing",
  "artificial frameshift",
  "non-consensus splice site",
  "nonconsensus splice site",
};


static const string s_RefseqExceptionStrings [] = {
  "unclassified transcription discrepancy",
  "unclassified translation discrepancy",
};


void CValidError_feat::ValidateExcept(const CSeq_feat& feat)
{
    if ( feat.IsSetExcept() &&  !feat.GetExcept()  &&
         feat.IsSetExcept_text()  &&  !feat.GetExcept_text().empty() ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
            "Exception text is set, but exception flag is not set", feat);
    }
    if ( feat.IsSetExcept_text()  &&  !feat.GetExcept_text ().empty() ) {
        ValidateExceptText(feat.GetExcept_text(), feat);
    }
}


void CValidError_feat::ValidateExceptText(const string& text, const CSeq_feat& feat)
{
    if ( text.empty() ) return;

    EDiagSev sev = eDiag_Error;
    bool found = false;

    string str;
    string::size_type   begin = 0, end, textlen = text.length();

    const string* except_begin = s_LegalExceptionStrings;
    const string* except_end = 
        &(s_LegalExceptionStrings[sizeof(s_LegalExceptionStrings) / sizeof(string)]);
    const string* refseq_begin = s_RefseqExceptionStrings;
    const string* refseq_end = 
        &(s_RefseqExceptionStrings[sizeof(s_RefseqExceptionStrings) / sizeof(string)]);
    
    while ( begin < textlen ) {
        found = false;
        end = min( text.find_first_of(',', begin), textlen );
        str = NStr::TruncateSpaces( text.substr(begin, end) );
        if ( find(except_begin, except_end, str) != except_end ) {
            found = true;
        }
        if ( !found  &&  (m_Imp.IsGPS() || m_Imp.IsRefSeq()) ) {
            if ( find(refseq_begin, refseq_end, str) != refseq_end ) {
               found = true;
            }
        }
        if ( !found ) {
            if ( m_Imp.IsNC()  ||  m_Imp.IsNT() ) {
                sev = eDiag_Warning;
            }
            PostErr(sev, eErr_SEQ_FEAT_InvalidQualifierValue,
                str + " is not legal exception explanation", feat);
        }
        begin = end + 1;
    }
}


static const string s_BypassCdsTransCheck[] = {
  "RNA editing",
  "reasons given in citation",
  "artificial frameshift",
  "unclassified translation discrepancy",
};



void CValidError_feat::ReportCdTransErrors
(const CSeq_feat& feat,
 bool show_stop,
 bool got_stop, 
 bool no_end,
 int ragged)
{
    if ( show_stop ) {
        if ( !got_stop  && !no_end ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_NoStop, 
                "Missing stop codon", feat);
        } else if ( got_stop  &&  no_end ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                "Got stop codon, but 3'end is labeled partial", feat);
        } else if ( got_stop  &&  !no_end  &&  ragged ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_TransLen, 
                "Coding region extends " + NStr::IntToString(ragged) +
                " base(s) past stop codon", feat);
        }
    }
}


void CValidError_feat::ValidateCdTrans(const CSeq_feat& feat)
{
    bool prot_ok = true;
    int  ragged = 0;

    // biological exception
    size_t except_num = sizeof(s_BypassCdsTransCheck) / sizeof(string);
    if ( feat.GetExcept() && feat.IsSetExcept_text() ) {
        for ( size_t i = 0; i < except_num; ++i ) {
            if ( NStr::FindNoCase(feat.GetExcept_text(), 
                s_BypassCdsTransCheck[i]) != string::npos ) {
                return; 
            }
        }
    }
    
    // pseuogene
    if ( feat.GetPseudo()  ||  IsOverlappingGenePseudo(feat) ) {
        return;
    }

    if ( !feat.IsSetProduct() ) {
        // bail if no product exists. shuold be checked elsewhere.
        return;
    }

    const CCdregion& cdregion = feat.GetData().GetCdregion();
    const CSeq_loc& location = feat.GetLocation();
    const CSeq_loc& product = feat.GetProduct();
    
    int gc = 0;
    if ( cdregion.IsSetCode() ) {
        // We assume that the id is set for all Genetic_code
        gc = cdregion.GetCode().GetId();
    }
    string gccode = NStr::IntToString(gc);

    string transl_prot;   // translated protein
    CBioseq_Handle nuc_handle = m_Scope->GetBioseqHandle(location);
    if ( !nuc_handle ) {
        return;
    }
    try {
        CCdregion_translate::TranslateCdregion(
            transl_prot, 
            nuc_handle, 
            location, 
            cdregion, 
            true,   // include stop codons
            false); // do not remove trailing X/B/Z
    } catch ( const runtime_error& ) {
    }
    if ( transl_prot.empty() ) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_CdTransFail, 
            "Unable to translate", feat);
        return;
    }
    
    bool no_end = false;
    unsigned int part_loc = SeqLocPartialCheck(location, m_Scope);
    unsigned int part_prod = SeqLocPartialCheck(product, m_Scope);
    if ( (part_loc & eSeqlocPartial_Stop)  ||
        (part_prod & eSeqlocPartial_Stop) ) {
        no_end = true;
    } else {    
        // complete stop, so check for ragged end
        ragged = CheckForRaggedEnd(location, cdregion);
    }
    
    // check for code break not on a codon
    ValidateCodeBreakNotOnCodon(feat, location, cdregion);
    
    if ( cdregion.GetFrame() > CCdregion::eFrame_one ) {
        EDiagSev sev = m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning;
        if ( !(part_loc & eSeqlocPartial_Start) ) {
            PostErr(sev, eErr_SEQ_FEAT_PartialProblem, 
                "Suspicious CDS location - frame > 1 but not 5' partial", feat);
        } else if ( (part_loc & eSeqlocPartial_Nostart)  && 
            !IsPartialAtSpliceSite(location, eSeqlocPartial_Nostart) ) {
            PostErr(sev, eErr_SEQ_FEAT_PartialProblem, 
                "Suspicious CDS location - frame > 1 and not at consensus splice site",
                feat);
        }
    }
    
    bool no_beg = false;
    
    size_t stop_count = 0;
    if ( (part_loc & eSeqlocPartial_Start)  ||
         (part_prod & eSeqlocPartial_Start) ) {
        no_beg = true;
    }
    
    bool got_dash = (transl_prot[0] == '-');
    bool got_stop = (transl_prot[transl_prot.length() - 1] == '*');
    
    // count internal stops
    ITERATE( string, it, transl_prot ) {
        if ( *it == '*' ) {
            ++stop_count;
        }
    }
    if ( got_stop ) {
        --stop_count;
    }
    
    if ( stop_count > 0 ) {
        if ( got_dash ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                "Illegal start codon and " + 
                NStr::IntToString(stop_count) +
                " internal stops. Probably wrong genetic code [" + gccode + "]",
                feat);
        } else {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InternalStop, 
                NStr::IntToString(stop_count) + 
                " internal stops. Genetic code [" + gccode + "]", feat);
            prot_ok = false;
            if ( stop_count > 5 ) {
                return;
            }
        }
    } else if ( got_dash ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon, 
            "Illegal start codon used. Wrong genetic code [" +
            gccode + "] or protein should be partial", feat);
    }
    
    bool show_stop = true;

    CBioseq_Handle prot_handle = m_Scope->GetBioseqHandle(product);
    if ( !prot_handle ) {
        if ( transl_prot.length() > 6 ) {
            if ( !(m_Imp.IsNG() || m_Imp.IsNT()) ) {
                EDiagSev sev = eDiag_Error;
                if ( IsDeltaOrFarSeg(location, m_Scope) ) {
                    sev = eDiag_Warning;
                }
                if ( m_Imp.IsNC() ) {
                    sev = eDiag_Warning;

                    if ( nuc_handle.GetTopLevelSeqEntry().IsSeq() ) {
                        sev = eDiag_Info;
                    }
                }
                if ( sev != eDiag_Info ) {
                    PostErr(sev, eErr_SEQ_FEAT_NoProtein, 
                        "No protein Bioseq given", feat);
                }
            }
        }
        ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged);
        return;
    }

    CSeqVector prot_vec = prot_handle.GetSeqVector();
    prot_vec.SetCoding(CSeq_data::e_Ncbieaa);
    size_t prot_len = prot_vec.size(); 
    size_t len = transl_prot.length();

    if ( got_stop  &&  (len == prot_len + 1) ) { // ok, got stop
        --len;
    }

    // ignore terminal 'X' from partial last codon if present
    
    while ( prot_len > 0 ) {
        if ( prot_vec[prot_len - 1] == 'X' ) {  //remove terminal X
            --prot_len;
        } else {
            break;
        }
    }
    
    while ( len > 0 ) {
        if ( transl_prot[len - 1] == 'X' ) {  //remove terminal X
            --len;
        } else {
            break;
        }
    }

    vector<TSeqPos> mismatches;
    if ( len == prot_len )  {                // could be identical
        for ( TSeqPos i = 0; i < len; ++i ) {
            if ( transl_prot[i] != prot_vec[i] ) {
                mismatches.push_back(i);
                prot_ok = false;
            }
        }
    } else {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_TransLen,
            "Given protein length [" + NStr::IntToString(prot_len) + 
            "] does not match translation length [" + 
            NStr::IntToString(len) + "]", feat);
    }
    
    // Mismatch on first residue
    string msg;
    if ( !mismatches.empty() && mismatches.front() == 0 ) {
        if ( feat.GetPartial() && (!no_beg) && (!no_end)) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                "Start of location should probably be partial",
                feat);
        } else if ( transl_prot[mismatches.front()] == '-' ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                "Illegal start codon used. Wrong genetic code [" +
                gccode + "] or protein should be partial", feat);
        }
    }

    char prot_res, transl_res;
    string nuclocstr;
    if ( mismatches.size() > 10 ) {
        // report total number of mismatches and the details of the 
        // first and last.
        nuclocstr = MapToNTCoords(feat, product, mismatches.front());
        prot_res = prot_vec[mismatches.front()];
        transl_res = Residue(transl_prot[mismatches.front()]);
        msg = 
            NStr::IntToString(mismatches.size()) + " mismatches found. " +
            "First mismatch at " + NStr::IntToString(mismatches.front() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if ( !nuclocstr.empty() ) {
            msg += " at " + nuclocstr;
        }
        nuclocstr = MapToNTCoords(feat, product, mismatches.back());
        prot_res = prot_vec[mismatches.back()];
        transl_res = Residue(transl_prot[mismatches.back()]);
        msg += 
            ". Last mismatch at " + NStr::IntToString(mismatches.back() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if ( !nuclocstr.empty() ) {
            msg += " at " + nuclocstr;
        }
        msg += ". Genetic code [" + gccode + "]";
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
    } else {
        // report individual mismatches
        for ( size_t i = 0; i < mismatches.size(); ++i ) {
            nuclocstr = MapToNTCoords(feat, product, mismatches[i]);
            prot_res = prot_vec[mismatches[i]];
            transl_res = Residue(transl_prot[mismatches[i]]);
            msg += 
                "Residue " + NStr::IntToString(mismatches[i] + 1) + 
                " in protein [" + prot_res + "]" +
                " != translation [" + transl_res + "]";
            if ( !nuclocstr.empty() ) {
                msg += " at " + nuclocstr;
            }
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
        }
    }

    if ( feat.GetPartial()  && mismatches.empty() ) {
        if ( !no_beg  && !no_end ) {
            if ( !got_stop ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                    "End of location should probably be partial", feat);
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "This SeqFeat should not be partial", feat);
            }
            show_stop = false;
        }
    }

    ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged);
}


void CValidError_feat::ValidateCodeBreakNotOnCodon
(const CSeq_feat& feat,
 const CSeq_loc& loc, 
 const CCdregion& cdregion)
{
    TSeqPos len = GetLength(loc, m_Scope);

    ITERATE( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
        size_t codon_length = GetLength((*cbr)->GetLoc(), m_Scope);
        TSeqPos from = LocationOffset(loc, (*cbr)->GetLoc(), 
            eOffset_FromStart, m_Scope);
        TSeqPos to = from + codon_length - 1;
        
        // check for code break not on a codon
        if ( codon_length == 3  ||
            ((codon_length == 1 || codon_length == 2)  && 
            to == len - 1) ) {
            size_t start_pos;
            switch ( cdregion.GetFrame() ) {
            case CCdregion::eFrame_two:
                start_pos = 1;
                break;
            case CCdregion::eFrame_three:
                start_pos = 2;
                break;
            default:
                start_pos = 0;
                break;
            }
            if ( (from % 3) != start_pos ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptPhase,
                    "transl_except qual out of frame.", feat);
            }
        }
    }
}


// Check for redundant gene Xref
void CValidError_feat::ValidateGeneXRef(const CSeq_feat& feat)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( !grp  ||  grp->IsSuppressed() ) {
        return;
    }

    // !!! DEBUG {
    if( m_Imp.AvoidPerfBottlenecks() ) {
        return;
    } 
    // } DEBUG
    
    CConstRef<CSeq_feat> overlap = GetBestOverlappingFeat(
        feat.GetLocation(),
        CSeqFeatData::e_Gene,
        eOverlap_Contains,
        *m_Scope);
    if ( !overlap ) {
        return;
    }
        
    const CGene_ref* overlap_xref = overlap->GetGeneXref();
    if ( !overlap_xref ) {
        return;
    }
    
    string label, overlap_label;
    grp->GetLabel(&label);
    overlap_xref->GetLabel(&overlap_label);
    
    if ( NStr::CompareNocase(label, overlap_label) == 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryGeneXref,
            "Unnecessary gene cross-reference " + label, feat);
    }
}


bool CValidError_feat::IsPartialAtSpliceSite
(const CSeq_loc& loc,
 unsigned int tag)
{
    if ( tag != eSeqlocPartial_Nostart && tag != eSeqlocPartial_Nostop ) {
        return false;
    }

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) { // EQUIV_IS_ONE not supported
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }

    if ( first.GetStrand() != last.GetStrand() ) {
        return false;
    }
    CSeq_loc_CI temp = (tag == eSeqlocPartial_Nostart) ? first : last;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(temp.GetSeq_id());
    if ( !bsh ) {
        return false;
    }
    
    TSeqPos acceptor = temp.GetRange().GetFrom();
    TSeqPos donor = temp.GetRange().GetTo();
    TSeqPos start = acceptor;
    TSeqPos stop = donor;

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
        temp.GetStrand());
    if ( !m_Imp.CheckSeqVector(vec) ) {
        return false;
    }
    TSeqPos len = vec.size();

    if ( temp.GetStrand() == eNa_strand_minus ) {
        swap(acceptor, donor);
        stop = len - donor - 1;
        start = len - acceptor - 1;
    }

    bool result = false;

    if ( (tag == eSeqlocPartial_Nostop)  &&  (stop < len - 2) ) {
        CSeqVector::TResidue res1 = vec[stop + 1];    
        CSeqVector::TResidue res2 = vec[stop + 2];

        if ( IsResidue(res1)  &&  IsResidue(res2) ) {
            if ( (res1 == 'G'  &&  res2 == 'T')  || 
                 (res1 == 'G'  &&  res2 == 'C') ) {
                result = true;
            }
        }
    } else if ( (tag == eSeqlocPartial_Nostart)  &&  (start > 1) ) {
        CSeqVector::TResidue res1 = vec[start - 2];    
        CSeqVector::TResidue res2 = vec[start - 1];

        if ( IsResidue(res1)  &&  IsResidue(res2) ) {
            if ( (res1 == 'A')  &&  (res2 == 'G') ) { 
                result = true;
            }
        }
    }

    return result;    
}


void CValidError_feat::ValidateFeatCit
(const CPub_set& cit,
 const CSeq_feat& feat)
{
    if ( !feat.IsSetCit() ) {
        return;
    }

    if ( cit.IsPub() ) {
        ITERATE( CPub_set::TPub, pi, cit.GetPub() ) {
            if ( (*pi)->IsEquiv() ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryCitPubEquiv,
                    "Citation on feature has unexpected internal Pub-equiv",
                    feat);
                return;
            }
        }
    }
}

void CValidError_feat::ValidateFeatComment
(const string& comment,
 const CSeq_feat& feat)
{
    if ( m_Imp.IsSerialNumberInComment(comment) ) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_SerialInComment,
            "Feature comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", feat);
    }
}


void CValidError_feat::ValidateFeatBioSource
(const CBioSource& bsrc,
 const CSeq_feat& feat)
{
    if ( bsrc.IsSetIs_focus() ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_FocusOnBioSourceFeature,
            "Focus must be on BioSource descriptor, not BioSource feature.",
            feat);
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    if ( !bsh ) {
        return;
    }
    CSeqdesc_CI dbsrc_i(bsh, CSeqdesc::e_Source);
    if ( !dbsrc_i ) {
        return;
    }
    
    const COrg_ref& org = bsrc.GetOrg();           
    const CBioSource& dbsrc = dbsrc_i->GetSource();
    const COrg_ref& dorg = dbsrc.GetOrg(); 

    if ( org.IsSetTaxname()  &&  !org.GetTaxname().empty()  &&
            dorg.IsSetTaxname() ) {
        string taxname = org.GetTaxname();
        string dtaxname = dorg.GetTaxname();
        if ( NStr::CompareNocase(taxname, dtaxname) != 0 ) {
            if ( !dbsrc.IsSetIs_focus()  &&  !IsTransgenic(dbsrc) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceNeedsFocus,
                    "BioSource descriptor must have focus or transgenic "
                    "when BioSource feature with different taxname is "
                    "present.", feat);
            }
        }
    }
    m_Imp.ValidateBioSource(bsrc, feat);
}


bool CValidError_feat::IsTransgenic(const CBioSource& bsrc)
{
    if ( bsrc.IsSetSubtype() ) {
        ITERATE( CBioSource::TSubtype, sub, bsrc.GetSubtype() ) {
            if ( (*sub)->GetSubtype() == CSubSource::eSubtype_transgenic ) {
                return true;
            }
        }
    }
    return false;
}


// REQUIRES: feature is either Gene or mRNA
bool CValidError_feat::IsSameAsCDS(const CSeq_feat& feat)
{
    EOverlapType overlap_type;
    if ( feat.GetData().IsGene() ) {
        overlap_type = eOverlap_Simple;
    } else if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
        overlap_type = eOverlap_CheckIntervals;
    } else {
        return false;
    }

    CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
            feat.GetLocation(),
            CSeqFeatData::e_Cdregion,
            overlap_type,
            *m_Scope);
        if ( cds ) {
            if ( TestForOverlap(
                    cds->GetLocation(),
                    feat.GetLocation(),
                    eOverlap_Simple) == 0 ) {
                return true;
            }
        }
    return false;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.25  2003/04/07 14:58:36  shomrat
* Added information to error postings
*
* Revision 1.24  2003/04/04 18:42:22  shomrat
* Increased robustness in face of exceptions and minor bug fixes
*
* Revision 1.23  2003/03/31 14:40:13  shomrat
* $id: -> $id$
*
* Revision 1.22  2003/03/28 16:26:47  shomrat
* Removed IsResidue (moved to utilities)
*
* Revision 1.21  2003/03/21 21:15:27  shomrat
* Implemented ValidateSeqFeatProduct
*
* Revision 1.20  2003/03/20 18:56:44  shomrat
* Supress error in case of Seq-annot validation
*
* Revision 1.19  2003/03/18 21:48:37  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.18  2003/03/11 16:04:09  kuznets
* iterate -> ITERATE
*
* Revision 1.17  2003/02/14 21:54:21  shomrat
* Change use of GetBestOverlappingFeat due to addition of contains option
*
* Revision 1.16  2003/02/12 18:05:52  shomrat
* SerialNumberInComment was moved to validerror_imp; and a few bug fixes
*
* Revision 1.15  2003/02/10 15:54:02  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.14  2003/02/07 21:25:04  shomrat
* SameAsCDS checks partial gene or mRNA for CDS with proper intervals, drops severity to INFO; Bug fixes
*
* Revision 1.13  2003/02/03 20:21:41  shomrat
* Skip use of feature indexing if performance flag is set (testing)
*
* Revision 1.12  2003/02/03 18:03:22  shomrat
* Change in use of GetSequenceView and style improvments
*
* Revision 1.11  2003/01/31 18:28:03  shomrat
* Re-implementation of ValidateBadGeneOverlap
*
* Revision 1.10  2003/01/30 20:26:17  shomrat
* Explicitly call sequence::Compare
*
* Revision 1.9  2003/01/29 21:58:26  shomrat
* Commented check for multiple CDS due to feature indexing implementation issues
*
* Revision 1.8  2003/01/29 17:50:06  shomrat
* Bug fix in ValidateExceptText
*
* Revision 1.7  2003/01/24 21:21:00  shomrat
* Bug fixes in ValidateMrnaTrans & ValidateBadGeneOverlap
*
* Revision 1.6  2003/01/21 20:11:11  shomrat
* Added check for RNA product mismatch
*
* Revision 1.5  2003/01/10 16:29:51  shomrat
* Added Checks for eErr_SEQ_FEAT_UnnecessaryCitPubEquiv and eErr_SEQ_DESCR_BioSourceNeedsFocus
*
* Revision 1.4  2003/01/06 16:43:01  shomrat
* naming convention
*
* Revision 1.3  2003/01/02 22:13:04  shomrat
* Implemented checks relying on overlapping features (IsOverlappingGenePseudo, PeptideOnCodonBoundry, CommonCDSProduct, BadMRNAOverlap, BadGeneOverlap, GeneXRef); Check for bad product seq id in ValidateSeqFeat
*
* Revision 1.2  2002/12/24 16:54:02  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:16:34  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
