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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/general/Person_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/genome_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGenbankFormatter::CGenbankFormatter(void) 
{
    SetIndent(string(12, ' '));
    SetFeatIndent(string(21, ' '));
}


///////////////////////////////////////////////////////////////////////////
//
// END SECTION

void CGenbankFormatter::EndSection
(const CEndSectionItem&,
 IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("//");
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Locus
//
// NB: The old locus line format is no longer supported for GenBank.
// (DDBJ will still show the old line format)

// Locus line format as specified in the GenBank release notes:
//
// Positions  Contents
// ---------  --------
// 01-05      'LOCUS'
// 06-12      spaces
// 13-28      Locus name
// 29-29      space
// 30-40      Length of sequence, right-justified
// 41-41      space
// 42-43      bp
// 44-44      space
// 45-47      spaces, ss- (single-stranded), ds- (double-stranded), or
//            ms- (mixed-stranded)
// 48-53      NA, DNA, RNA, tRNA (transfer RNA), rRNA (ribosomal RNA), 
//            mRNA (messenger RNA), uRNA (small nuclear RNA), snRNA,
//            snoRNA. Left justified.
// 54-55      space
// 56-63      'linear' followed by two spaces, or 'circular'
// 64-64      space
// 65-67      The division code (see Section 3.3 in GenBank release notes)
// 68-68      space
// 69-79      Date, in the form dd-MMM-yyyy (e.g., 15-MAR-1991)

void CGenbankFormatter::FormatLocus
(const CLocusItem& locus, 
 IFlatTextOStream& text_os)
{
    static const string strands[]  = { "   ", "ss-", "ds-", "ms-" };

    const CBioseqContext& ctx = *locus.GetContext();

    list<string> l;
    CNcbiOstrstream locus_line;

    string units = "bp";
    if ( !ctx.IsProt() ) {
        if ( ctx.IsWGSMaster()  &&  ctx.IsRSWGSNuc() ) {
            units = "rc";
        }
    } else {
        units = "aa";
    }
    string topology = (locus.GetTopology() == CSeq_inst::eTopology_circular) ?
                "circular" : "linear  ";

    locus_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
    locus_line << setw(16) << locus.GetName() << ' ';
    locus_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    locus_line
        << setw(11) << locus.GetLength()
        << ' '
        << units
        << ' '
        << strands[locus.GetStrand()];
    locus_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
    locus_line
        << setw(6) << s_GenbankMol[locus.GetBiomol()]
        << "  "
        << topology
        << ' '              
        << locus.GetDivision()
        << ' '
        << locus.GetDate();

    Wrap(l, GetWidth(), "LOCUS", CNcbiOstrstreamToString(locus_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Definition

void CGenbankFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream& text_os)
{
    list<string> l;
    Wrap(l, "DEFINITION", defline.GetDefline());
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGenbankFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream& text_os)
{
    string acc_line = x_FormatAccession(acc, ' ');
    if ( acc.IsSetRegion() ) {
        acc_line += " REGION: ";
        acc_line += NStr::Int8ToString(acc.GetRegion().GetFrom());
        acc_line += "..";
        acc_line += NStr::Int8ToString(acc.GetRegion().GetTo());
    }
    if ( !acc_line.empty() ) {
        list<string> l;
        Wrap(l, "ACCESSION", acc_line);
        text_os.AddParagraph(l);
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGenbankFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os)
{
    list<string> l;
    CNcbiOstrstream version_line;

    if ( version.GetAccession().empty() ) {
        l.push_back("VERSION");
    } else {
        version_line << version.GetAccession();
        if ( version.GetGi() > 0 ) {
            version_line << "  GI:" << version.GetGi();
        }
        Wrap(l, GetWidth(), "VERSION", CNcbiOstrstreamToString(version_line));
    }

    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGenbankFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os)
{
    list<string> l;
    x_GetKeywords(keys, "KEYWORDS", l);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Segment

void CGenbankFormatter::FormatSegment
(const CSegmentItem& seg,
 IFlatTextOStream& text_os)
{
    list<string> l;
    CNcbiOstrstream segment_line;

    segment_line << seg.GetNum() << " of " << seg.GetCount();

    Wrap(l, "SEGMENT", CNcbiOstrstreamToString(segment_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Source

// SOURCE + ORGANISM

void CGenbankFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream& text_os)
{
    list<string> l;
    x_FormatSourceLine(l, source);
    x_FormatOrganismLine(l, source);
    text_os.AddParagraph(l);    
}


void CGenbankFormatter::x_FormatSourceLine
(list<string>& l,
 const CSourceItem& source) const
{
    CNcbiOstrstream source_line;
    
    string prefix = source.IsUsingAnamorph() ? " (anamorph: " : " (";
    
    source_line << source.GetOrganelle() << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << prefix << source.GetCommon() << ")";
    }
    
    Wrap(l, GetWidth(), "SOURCE", CNcbiOstrstreamToString(source_line));
}


void CGenbankFormatter::x_FormatOrganismLine
(list<string>& l,
 const CSourceItem& source) const
{
    Wrap(l, GetWidth(), "ORGANISM", source.GetTaxname(), eSubp);
    Wrap(l, GetWidth(), kEmptyStr, source.GetLineage(), eSubp);
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

// The REFERENCE field consists of five parts: the keyword REFERENCE, and
// the subkeywords AUTHORS, TITLE (optional), JOURNAL, MEDLINE (optional),
// PUBMED (optional), and REMARK (optional).

void CGenbankFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& text_os)
{
    CBioseqContext& ctx = *ref.GetContext();

    list<string> l;

    x_Reference(l, ref, ctx);
    x_Authors(l, ref, ctx);
    x_Consortium(l, ref, ctx);
    x_Title(l, ref, ctx);
    x_Journal(l, ref, ctx);
    x_Medline(l, ref, ctx);
    x_Pubmed(l, ref, ctx);
    x_Remark(l, ref, ctx);

    text_os.AddParagraph(l);
}


// The REFERENCE line contains the number of the particular reference and
// (in parentheses) the range of bases in the sequence entry reported in
// this citation.
void CGenbankFormatter::x_Reference
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    CNcbiOstrstream ref_line;

    int serial = ref.GetSerial();
    CPubdesc::TReftype reftype = ref.GetReftype();

    // print serial
    if (serial > 99) {
        ref_line << serial << ' ';
    } else if (reftype ==  CPubdesc::eReftype_no_target) {
        ref_line << serial;
    } else {
        ref_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
        ref_line << setw(3) << serial;
    }

    // print sites or range
    if ( reftype == CPubdesc::eReftype_sites  ||
         reftype == CPubdesc::eReftype_feats ) {
        ref_line << "(sites)";
    } else if ( reftype == CPubdesc::eReftype_no_target ) {
        // do nothing
    } else {
        const CSeq_loc* loc = (ref.GetLoc() != 0) ? ref.GetLoc() : &ctx.GetLocation();
        x_FormatRefLocation(ref_line, *loc, " to ", "; ", ctx);
    }
    Wrap(l, GetWidth(), "REFERENCE", CNcbiOstrstreamToString(ref_line));
}


void CGenbankFormatter::x_Authors
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string auth = CReferenceItem::GetAuthString(ref.GetAuthors());
    if ( !NStr::EndsWith(auth, ".") ) {
        auth += ".";
    }
    Wrap(l, "AUTHORS", auth, eSubp);
}


void CGenbankFormatter::x_Consortium
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    Wrap(l, "CONSRTM", ref.GetConsortium(), eSubp);
}


void CGenbankFormatter::x_Title
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string title = ref.GetTitle();

    if ( NStr::EndsWith(title, ".") ) {
        title.erase(title.length() - 1);
    }
    
    Wrap(l, "TITLE",   title,   eSubp);
}


void CGenbankFormatter::x_Journal
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    string journal;
    x_FormatRefJournal(journal, ref);
    Wrap(l, "JOURNAL", journal, eSubp);
}


void CGenbankFormatter::x_Medline
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if ( ref.GetMUID() != 0 ) {
        Wrap(l, GetWidth(), "MEDLINE", NStr::IntToString(ref.GetMUID()), eSubp);
    }
}


void CGenbankFormatter::x_Pubmed
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    if ( ref.GetPMID() != 0 ) {
        Wrap(l, " PUBMED", NStr::IntToString(ref.GetPMID()), eSubp);
    }
}


void CGenbankFormatter::x_Remark
(list<string>& l,
 const CReferenceItem& ref,
 CBioseqContext& ctx) const
{
    Wrap(l, "REMARK", ref.GetRemark(), eSubp);
}


///////////////////////////////////////////////////////////////////////////
//
// COMMENT


void CGenbankFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& text_os)
{
    list<string> l;
    
    if ( !comment.IsFirst() ) {
        Wrap(l, kEmptyStr, comment.GetComment(), eSubp);
    } else {
        Wrap(l, "COMMENT", comment.GetComment());
    }

    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

// Fetures Header

void CGenbankFormatter::FormatFeatHeader
(const CFeatHeaderItem& fh,
 IFlatTextOStream& text_os)
{
    list<string> l;

    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);

    text_os.AddParagraph(l);
}


void CGenbankFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{ 
    CConstRef<CFlatFeature> feat = f.Format();
    list<string>        l;
    Wrap(l, feat->GetKey(), feat->GetLoc().GetString(), eFeat);
    ITERATE (vector<CRef<CFormatQual> >, it, feat->GetQuals()) {
        string qual = '/' + (*it)->GetName(), value = (*it)->GetValue();
        switch ((*it)->GetStyle()) {
        case CFormatQual::eEmpty:
            value = qual;
            qual.erase();
            break;
        case CFormatQual::eQuoted:
            qual += "=\"";  value += '"';
            break;
        case CFormatQual::eUnquoted:
            qual += '=';
            break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, GetWidth(), l,
                   /*DoHTML() ? NStr::fWrap_HTMLPre : */0, GetFeatIndent(),
                   GetFeatIndent() + qual);
    }
    NON_CONST_ITERATE (list<string>, it, l) {
        NStr::TruncateSpacesInPlace(*it, NStr::eTrunc_End);
    }
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

void CGenbankFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os)
{
    list<string> l;

    CNcbiOstrstream bc_line;

    bc_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
    bc_line
        << setw(7) << bc.GetA() << " a"
        << setw(7) << bc.GetC() << " c"
        << setw(7) << bc.GetG() << " g"
        << setw(7) << bc.GetT() << " t";
    if ( bc.GetOther() > 0 ) {
        bc_line << setw(7) << bc.GetOther() << " others";
    }
    Wrap(l, "BASE COUNT", CNcbiOstrstreamToString(bc_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGenbankFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    list<string> l;
    CNcbiOstrstream seq_line;

    const CSeqVector& vec = seq.GetSequence();

    TSeqPos base_count = seq.GetFrom();
    CSeqVector::const_iterator iter = vec.begin();
    while ( iter ) {
        seq_line.setf(IOS_BASE::right, IOS_BASE::adjustfield);
        seq_line << setw(9) << base_count;
        for ( TSeqPos count = 0; count < 60  &&  iter; ++count, ++iter, ++base_count ) {
            if ( count % 10 == 0 ) {
                seq_line << ' ';
            }
            seq_line << (char)tolower(*iter);
        }
        seq_line << '\n';
    }

    NStr::Split(CNcbiOstrstreamToString(seq_line), "\n", l);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// DBSOURCE

void CGenbankFormatter::FormatDBSource
(const CDBSourceItem& dbs,
 IFlatTextOStream& text_os)
{
    list<string> l;

    if ( !dbs.GetDBSource().empty() ) {
        string tag = "DBSOURCE";
        ITERATE (list<string>, it, dbs.GetDBSource()) {
            Wrap(l, tag, *it);
            tag.erase();
        }
        if ( !l.empty() ) {
            text_os.AddParagraph(l);
        }
    }        
}


///////////////////////////////////////////////////////////////////////////
//
// WGS

void CGenbankFormatter::FormatWGS
(const CWGSItem& wgs,
 IFlatTextOStream& text_os)
{
    string tag;

    switch ( wgs.GetType() ) {
    case CWGSItem::eWGS_Projects:
        tag = "WGS";
        break;

    case CWGSItem::eWGS_ScaffoldList:
        tag = "WGS_SCAFLD";
        break;

    case CWGSItem::eWGS_ContigList:
        tag = "WGS_CONTIG";
        break;

    default:
        return;
    }

    list<string> l;
    if ( wgs.GetFirstID() == wgs.GetLastID() ) {
        Wrap(l, tag, wgs.GetFirstID());
    } else {
        Wrap(l, tag, wgs.GetFirstID() + "-" + wgs.GetLastID());
    }
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

void CGenbankFormatter::FormatPrimary
(const CPrimaryItem& primary,
 IFlatTextOStream& text_os)
{
    list<string> l;

    Wrap(l, "PRIMARY", primary.GetString());

    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// GENOME

void CGenbankFormatter::FormatGenome
(const CGenomeItem& genome,
 IFlatTextOStream& text_os)
{
    // !!!
}


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGenbankFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream& text_os)
{
    list<string> l;
    string assembly = CFlatSeqLoc(contig.GetLoc(), *contig.GetContext(), 
        CFlatSeqLoc::eType_assembly).GetString();
    Wrap(l, "CONTIG", assembly);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// ORIGIN

void CGenbankFormatter::FormatOrigin
(const COriginItem& origin,
 IFlatTextOStream& text_os)
{
    list<string> l;
    if ( origin.GetOrigin().empty() ) {
        l.push_back("ORIGIN      ");
    } else {
        Wrap(l, "ORIGIN", origin.GetOrigin());
    }
    text_os.AddParagraph(l);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.19  2004/10/05 18:06:28  shomrat
* in place TruncateSpaces ->TruncateSpacesInPlace
*
* Revision 1.18  2004/10/05 15:57:20  shomrat
* Use non-const NStr::TruncateSpaces
*
* Revision 1.17  2004/08/30 13:41:08  shomrat
* Truncate spaces from feature quals
*
* Revision 1.16  2004/08/19 16:36:45  shomrat
* Fixed REFERENCE format
*
* Revision 1.15  2004/08/09 19:17:55  shomrat
* Remove redundent spaces from REFERENCE line
*
* Revision 1.14  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.13  2004/05/06 17:53:12  shomrat
* CFlatQual -> CFormatQual
*
* Revision 1.12  2004/04/22 15:59:59  shomrat
* Changes in context
*
* Revision 1.11  2004/04/13 16:47:53  shomrat
* Journal formatting moved to base class
*
* Revision 1.10  2004/03/26 17:25:36  shomrat
* fixes to reference formatting
*
* Revision 1.9  2004/03/18 15:40:28  shomrat
* Fixes to COMMENT formatting
*
* Revision 1.8  2004/03/10 21:29:01  shomrat
* Fix VERSION formatting when empty
*
* Revision 1.7  2004/03/05 18:46:05  shomrat
* fixed formatting of empty qualifier
*
* Revision 1.6  2004/02/19 18:13:12  shomrat
* Added formatting of Contig and Origin
*
* Revision 1.5  2004/01/14 16:16:39  shomrat
* removed const; using ctrl_items
*
* Revision 1.4  2003/12/19 00:14:44  ucko
* Eliminate remaining uses of LEFT and RIGHT manipulators.
*
* Revision 1.3  2003/12/18 21:23:41  ucko
* Avoid using LEFT and RIGHT manipulators lacking in GCC 2.9x.
*
* Revision 1.2  2003/12/18 17:43:34  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:22:12  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/

