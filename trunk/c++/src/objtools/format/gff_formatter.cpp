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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*           
*
*/
#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/general/Date.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/format/gff_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/context.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGFFFormatter::CGFFFormatter(void) :
    m_GFFFlags(CGFFFormatter::fGTFCompat)
{
}


void CGFFFormatter::Start(IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("##gff-version 2");
    l.push_back("##source-version NCBI C++ formatter 0.1");
    l.push_back("##date " + CurrentTime().AsString("Y-M-D"));
    text_os.AddParagraph(l);
}


void CGFFFormatter::StartSection(IFlatTextOStream& text_os)
{
    list<string> l;

    switch (GetContext().GetMol()) {
    case CSeq_inst::eMol_dna:  m_SeqType = "DNA";      break;
    case CSeq_inst::eMol_rna:  m_SeqType = "RNA";      break;
    case CSeq_inst::eMol_aa:   m_SeqType = "Protein";  break;
    default:                   m_SeqType.erase();      break;
    }
    if ( !m_SeqType.empty() ) {
        l.push_back("##Type " + m_SeqType + ' '
                    + GetContext().GetAccession());
    }
    text_os.AddParagraph(l);
}


void CGFFFormatter::EndSection(IFlatTextOStream& text_os)
{
    if ( !m_EndSequence.empty() ) {
        list<string> l;
        l.push_back(m_EndSequence);
        text_os.AddParagraph(l);
    }
}


void CGFFFormatter::FormatLocus
(const CLocusItem& locus, 
 IFlatTextOStream& text_os)
{
    m_Strandedness = locus.GetStrand();
}


void CGFFFormatter::FormatDate
(const CDateItem& date,
 IFlatTextOStream& text_os)
{
    m_Date.erase();

    const CDate* d = date.GetUpdateDate();
    if ( d != 0 ) {
        d->GetDate(&m_Date, "%4Y-%{%2M%|??%}-%{%2D%|??%}");
    }
}



///////////////////////////////////////////////////////////////////////////
//
// FEATURES


void CGFFFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{
    const CSeq_feat& seqfeat = f.GetFeat();
    string           key(f.GetKey()), oldkey;
    bool             gtf     = false;
    CFFContext& ctx = const_cast<CFFContext&>(f.GetContext());
    CScope* scope = &ctx.GetScope();

    // CSeq_loc         tentative_stop;

    if ((m_GFFFlags & fGTFCompat)  &&  !ctx.IsProt()
        &&  (key == "CDS"  ||  key == "exon")) {
        gtf = true;
    } else if ((m_GFFFlags & fGTFCompat)
               &&  ctx.GetMol() == CSeq_inst::eMol_dna
               &&  seqfeat.GetData().IsRna()) {
        oldkey = key;
        key    = "exon";
        gtf    = true;
    } else if ((m_GFFFlags & fGTFOnly) == fGTFOnly) {
        return;
    }

    CFlatFeature& feat = *f.Format();
    list<string>  l;
    list<string>  attr_list;

    if ( !oldkey.empty() ) {
        attr_list.push_back("gbkey \"" + oldkey + "\";");
    }

    ITERATE (CFlatFeature::TQuals, it, feat.GetQuals()) {
        string name = (*it)->GetName();
        if (name == "codon_start"  ||  name == "translation"
            ||  name == "transcription") {
            continue; // suppressed to reduce verbosity
        } else if (name == "number"  &&  key == "exon") {
            name = "exon_number";
        } else if ((m_GFFFlags & fGTFCompat)  &&  !ctx.IsProt()
                   &&  name == "gene") {
            string gene_id = x_GetGeneID(feat, (*it)->GetValue(), ctx);
            attr_list.push_front
                ("transcript_id \"" + gene_id + '.' + m_Date + "\";");
            attr_list.push_front("gene_id \"" + gene_id + "\";");
            continue;
        }
        string value;
        NStr::Replace((*it)->GetValue(), " \b", kEmptyStr, value);
        string value2(NStr::PrintableString(value));
        // some parsers may be dumb, so quote further
        value.erase();
        ITERATE (string, c, value2) {
            switch (*c) {
            case ' ':  value += "\\x20"; break;
            case '\"': value += "x22";   break; // already backslashed
            case '#':  value += "\\x23"; break;
            default:   value += *c;
            }
        }
        attr_list.push_back(name + " \"" + value + "\";");
    }
    string attrs(NStr::Join(attr_list, " "));

    string source = x_GetSourceName(ctx);

    int frame = -1;
    if (seqfeat.GetData().IsCdregion()  &&  !ctx.IsProt() ) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        frame = max(cds.GetFrame() - 1, 0);
    }
    x_AddFeature(l, f.GetLoc(), source, key, "." /*score*/, frame, attrs, gtf, ctx);

    if (gtf  &&  seqfeat.GetData().IsCdregion()) {
        const CCdregion& cds = seqfeat.GetData().GetCdregion();
        if ( !f.GetLoc().IsPartialLeft() ) {
            CRef<CSeq_loc> tentative_start;
            {{
                CRef<SRelLoc::TRange> range(new SRelLoc::TRange);
                SRelLoc::TRanges      ranges;
                range->SetFrom(frame);
                range->SetTo(frame + 2);
                ranges.push_back(range);
                tentative_start = SRelLoc(f.GetLoc(), ranges).Resolve(scope);
            }}

            string s;
            ctx.GetHandle().GetSequenceView
                (*tentative_start, CBioseq_Handle::eViewConstructed)
                .GetSeqData(0, 3, s);
            const CTrans_table* tt;
            if (cds.IsSetCode()) {
                tt = &CGen_code_table::GetTransTable(cds.GetCode());
            } else {
                tt = &CGen_code_table::GetTransTable(1);
            }
            if (s.size() == 3
                &&  tt->IsAnyStart(tt->SetCodonState(s[0], s[1], s[2]))) {
                x_AddFeature(l, *tentative_start, source, "start_codon",
                             "." /* score */, 0, attrs, gtf, ctx);
            }
        }

        if ( !f.GetLoc().IsPartialRight()  &&  seqfeat.IsSetProduct() ) {
            TSeqPos loc_len = sequence::GetLength(f.GetLoc(), scope);
            TSeqPos prod_len = sequence::GetLength(seqfeat.GetProduct(),
                                                   scope);
            CRef<CSeq_loc> tentative_stop;
            if (loc_len >= frame + 3 * prod_len + 3) {
                SRelLoc::TRange range;
                range.SetFrom(frame + 3 * prod_len);
                range.SetTo  (frame + 3 * prod_len + 2);
                // needs to be partial for TranslateCdregion to DTRT
                range.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
                SRelLoc::TRanges ranges;
                ranges.push_back(CRef<SRelLoc::TRange>(&range));
                tentative_stop = SRelLoc(f.GetLoc(), ranges).Resolve(scope);
            }
            if (tentative_stop.NotEmpty()  &&  !tentative_stop->IsNull()) {
                string s;
                CCdregion_translate::TranslateCdregion
                    (s, ctx.GetHandle(), *tentative_stop, cds);
                if (s == "*") {
                    x_AddFeature(l, *tentative_stop, source, "stop_codon",
                                 "." /* score */, 0, attrs, gtf, ctx);
                }
            }
        }
    }

    text_os.AddParagraph(l, &seqfeat);
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

// used as a trigger for the sequence header

void CGFFFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os)
{
    if ( !(m_GFFFlags & fShowSeq) )
        return;

    CFFContext& ctx = const_cast<CFFContext&>(bc.GetContext());

    list<string> l;
    l.push_back("##" + m_SeqType + ' ' + ctx.GetAccession());
    text_os.AddParagraph(l);
    m_EndSequence = "##end-" + m_SeqType;
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGFFFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    if ( !(m_GFFFlags & fShowSeq) )
        return;
    CFFContext& ctx = const_cast<CFFContext&>(seq.GetContext());

    list<string> l;
    CSeqVector v = seq.GetSequence();
    v.SetCoding(CBioseq_Handle::eCoding_Iupac);

    CSeqVector_CI vi(v);
    string s;
    while (vi) {
        s.erase();
        vi.GetSeqData(s, 70);
        l.push_back("##" + s);
    }
    text_os.AddParagraph(l, &ctx.GetActiveBioseq());
}


// Private

string CGFFFormatter::x_GetGeneID
(const CFlatFeature& feat,
 const string& gene,
 CFFContext& ctx) const
{
    const CSeq_feat& seqfeat = feat.GetFeat();

    string               main_acc;
    if (ctx.IsPart()) {
        const CSeq_id& id = *(ctx.Master()->GetHandle().GetSeqId());
        main_acc = ctx.GetPreferredSynonym(id).GetSeqIdString(true);
    } else {
        main_acc = ctx.GetAccession();
    }

    string               gene_id   = main_acc + ':' + gene;
    CConstRef<CSeq_feat> gene_feat = sequence::GetBestOverlappingFeat
        (seqfeat.GetLocation(), CSeqFeatData::e_Gene,
         sequence::eOverlap_Interval, ctx.GetScope());
    
    TFeatVec&                v  = m_Genes[gene_id];
    TFeatVec::const_iterator it = find(v.begin(), v.end(), gene_feat);
    int                      n;
    if (it == v.end()) {
        n = v.size();
        v.push_back(gene_feat);
    } else {
        n = it - v.begin();
    }
    if (n > 0) {
        gene_id += '.' + NStr::IntToString(n + 1);
    }

    return gene_id;
}


string CGFFFormatter::x_GetSourceName(CFFContext& ctx) const
{
    // XXX - get from annot name (not presently available from IFF)?
    switch ( ctx.GetPrimaryId()->Which() ) {
    case CSeq_id::e_Local:                           return "Local";
    case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:   case CSeq_id::e_Gi:      return "GenInfo";
    case CSeq_id::e_Genbank:                         return "Genbank";
    case CSeq_id::e_Swissprot:                       return "SwissProt";
    case CSeq_id::e_Patent:                          return "Patent";
    case CSeq_id::e_Other:                           return "RefSeq";
    case CSeq_id::e_General:
        return ctx.GetPrimaryId()->GetGeneral().GetDb();
    default:
    {
        string source
            (CSeq_id::SelectionName(ctx.GetPrimaryId()->Which()));
        return NStr::ToUpper(source);
    }
    }
}


void CGFFFormatter::x_AddFeature
(list<string>& l,
 const CSeq_loc& loc,
 const string& source,
 const string& key,
 const string& score,
 int frame,
 const string& attrs,
 bool gtf,
 CFFContext& ctx) const
{
    int exon_number = 1;
    for (CSeq_loc_CI it(loc);  it;  ++it) {
        TSeqPos from   = it.GetRange().GetFrom(), to = it.GetRange().GetTo();
        char    strand = '+';

        if (IsReverse(it.GetStrand())) {
            strand = '-';
        } else if (it.GetRange().IsWhole()
                   ||  (m_Strandedness <= CSeq_inst::eStrand_ss
                        &&  ctx.GetMol() != CSeq_inst::eMol_dna)) {
            strand = '.'; // N/A
        }

        if (it.GetRange().IsWhole()) {
            to = sequence::GetLength(it.GetSeq_id(), &ctx.GetScope()) - 1;
        }

        string extra_attrs;
        if (gtf  &&  attrs.find("exon_number ") == NPOS) {
            CSeq_loc       loc2;
            CSeq_interval& si = loc2.SetInt();
            si.SetFrom(from);
            si.SetTo(to);
            si.SetStrand(it.GetStrand());
            si.SetId(const_cast<CSeq_id&>(it.GetSeq_id()));
            CConstRef<CSeq_feat> exon = sequence::GetBestOverlappingFeat
                (loc2, CSeqFeatData::eSubtype_exon,
                 sequence::eOverlap_Contains, ctx.GetScope());
            if (exon.NotEmpty()  &&  exon->IsSetQual()) {
                ITERATE (CSeq_feat::TQual, q, exon->GetQual()) {
                    if ( !NStr::CompareNocase((*q)->GetQual(), "number") ) {
                        int n = NStr::StringToNumeric((*q)->GetVal());
                        if (n >= exon_number) {
                            exon_number = n;
                            break;
                        }
                    }
                }
            }
            extra_attrs = " exon_number \"" + NStr::IntToString(exon_number)
                + "\";";
            ++exon_number;
        }

        if ( sequence::IsSameBioseq(it.GetSeq_id(), *ctx.GetPrimaryId(),
                                    &ctx.GetScope()) ) {
            // conditionalize printing, but update state regardless
            l.push_back(ctx.GetAccession() + '\t'
                        + source + '\t'
                        + key + '\t'
                        + NStr::UIntToString(from + 1) + '\t'
                        + NStr::UIntToString(to + 1) + '\t'
                        + score + '\t'
                        + strand + '\t'
                        + (frame >= 0 ? char(frame + '0') : '.') + "\t"
                        + attrs + extra_attrs);
        }

        if (frame >= 0) {
            frame = (frame + to - from + 1) % 3;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/03/25 20:42:41  shomrat
* Master returns a CBioseq_Handle instead of a CBioseq
*
* Revision 1.2  2004/02/11 16:59:11  shomrat
* removed unused variable
*
* Revision 1.1  2004/01/14 16:07:29  shomrat
* Initial Revision
*
*
* ===========================================================================
*/

