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

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/biblio/Auth_list.hpp>
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
 IFlatTextOStream& text_os) const
{
    static const string strands[]  = { "   ", "ss-", "ds-", "ms-" };

    const CFFContext& ctx = locus.GetContext();

    list<string> l;
    CNcbiOstrstream locus_line;

    string units = "bp";
    if ( !ctx.IsProt() ) {
        if ( ctx.IsWGSMaster()  &&  ctx.IsNZ() ) {
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
 IFlatTextOStream& text_os) const
{
    list<string> l;
    Wrap(l, "DEFLINE", defline.GetDefline());
    text_os.AddParagraph(l);
}

///////////////////////////////////////////////////////////////////////////
//
// Accession

void CGenbankFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream& text_os) const
{
    string acc_line = x_FormatAccession(acc, ';');
    
    if ( !acc_line.empty() ) {
        list<string> l;
        Wrap(l, "ACCESSION", x_FormatAccession(acc, ' '));
        text_os.AddParagraph(l);
    }
}


///////////////////////////////////////////////////////////////////////////
//
// Version

void CGenbankFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os) const
{
    list<string> l;
    CNcbiOstrstream version_line;

    version_line << version.GetAccession();
    if ( version.GetGi() > 0 ) {
        version_line << "  GI:" << version.GetGi();
    }

    Wrap(l, GetWidth(), "VERSION", CNcbiOstrstreamToString(version_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Keywords

void CGenbankFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os) const
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
 IFlatTextOStream& text_os) const
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
 IFlatTextOStream& text_os) const
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
    Wrap(l, GetWidth(), kEmptyStr, source.GetLineage() + '.', eSubp);
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

// The REFERENCE field consists of five parts: the keyword REFERENCE, and
// the subkeywords AUTHORS, TITLE (optional), JOURNAL, MEDLINE (optional),
// PUBMED (optional), and REMARK (optional).

void CGenbankFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& text_os) const
{
    CFFContext& ctx = const_cast<CFFContext&>(ref.GetContext()); // !!!

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
 CFFContext& ctx) const
{
    CNcbiOstrstream ref_line;

    // print serial number
    ref_line << ref.GetSerial() << (ref.GetSerial() < 10 ? "  " : " ");

    // print sites or range
    CPubdesc::TReftype reftype = ref.GetReftype();

    if ( reftype == CPubdesc::eReftype_sites  ||
         reftype == CPubdesc::eReftype_feats ) {
        ref_line << "(sites)";
    } else if ( reftype == CPubdesc::eReftype_no_target ) {
    } else {
        const CSeq_loc* loc = ref.GetLoc() != 0 ? ref.GetLoc() : ctx.GetLocation();
        x_FormatRefLocation(ref_line, *loc, " to ", "; ",
            ctx.IsProt(), ctx.GetScope());
    }
    Wrap(l, GetWidth(), "REFERENCE", CNcbiOstrstreamToString(ref_line));
}


void CGenbankFormatter::x_Authors
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    Wrap(l, "AUTHORS", CReferenceItem::GetAuthString(ref.GetAuthors()), eSubp);
}


void CGenbankFormatter::x_Consortium
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    Wrap(l, "CONSRTM", ref.GetConsortium(), eSubp);
}


void CGenbankFormatter::x_Title
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    string title;

    switch ( ref.GetCategory() ) {
    case CReferenceItem::eSubmission:
        title = "Direct Submission";
        break;

    case CReferenceItem::ePublished:
        title = ref.GetTitle();
        break;

    case CReferenceItem::eUnpublished:
        break;

    default:
        break;
    }

    if ( NStr::EndsWith(title, ".") ) {
        title.erase(title.length() - 1);
    }
    
    Wrap(l, "TITLE",   title,   eSubp);
}


void CGenbankFormatter::x_Journal
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    string journal;

    if ( ref.GetCategory() == CReferenceItem::eSubmission ) {
        journal = "Submitted ";
        if ( ref.GetDate() != 0 ) {
            journal += '(';
            DateToString(*ref.GetDate(), journal, true);
            journal += ") ";
        }
        journal += ref.GetJournal();
    } else {
        journal = ref.GetJournal();
        if ( !ref.GetVolume().empty() ) {
            journal += " " + ref.GetVolume();
        }
        if ( !ref.GetIssue().empty() ) {
            journal += " (" + ref.GetIssue() + ')';
        }
        if ( !ref.GetPages().empty() ) {
            journal += ", " + ref.GetPages();
        }
        if ( ref.GetDate() != 0 ) {
            journal += " (";
            ref.GetDate()->GetDate(&journal, "%Y");
            journal += ')';
        }
    }

    if ( journal.empty() ) {
        journal = "Unpublished";
    }

    Wrap(l, "JOURNAL", journal, eSubp);
}


void CGenbankFormatter::x_Medline
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    if ( ref.GetMUID() != 0 ) {
        Wrap(l, GetWidth(), "MEDLINE", NStr::IntToString(ref.GetMUID()), eSubp);
    }
}


void CGenbankFormatter::x_Pubmed
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    if ( ref.GetPMID() != 0 ) {
        Wrap(l, " PUBMED", NStr::IntToString(ref.GetPMID()), eSubp);
    }
}


void CGenbankFormatter::x_Remark
(list<string>& l,
 const CReferenceItem& ref,
 CFFContext& ctx) const
{
    Wrap(l, "REMARK", ref.GetRemark(), eSubp);
}


///////////////////////////////////////////////////////////////////////////
//
// COMMENT


void CGenbankFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& text_os) const
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
 IFlatTextOStream& text_os) const
{
    list<string> l;

    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);

    text_os.AddParagraph(l);
}


void CGenbankFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os) const
{ 
    const CFlatFeature& feat = *f.Format();
    list<string>        l;
    Wrap(l, feat.GetKey(), feat.GetLoc().GetString(), eFeat);
    ITERATE (vector<CRef<CFlatQual> >, it, feat.GetQuals()) {
        string qual = '/' + (*it)->GetName(), value = (*it)->GetValue();
        switch ((*it)->GetStyle()) {
        case CFlatQual::eEmpty:                    value.erase();  break;
        case CFlatQual::eQuoted:   qual += "=\"";  value += '"';   break;
        case CFlatQual::eUnquoted: qual += '=';                    break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, GetWidth(), l,
                   /*DoHTML() ? NStr::fWrap_HTMLPre : */0, GetFeatIndent(),
                   GetFeatIndent() + qual);
    }
    text_os.AddParagraph(l);
    //m_Stream->AddParagraph(l, &f, &feat.GetFeat());
    /*
    list<string>        l;
    Wrap(l, feat.GetKey(), feat.GetLoc().GetString(), eFeat);

    string qual, value;
    ITERATE ( TQuals, it, feat.GetQuals() ) {
        const CFlatQual& q = *it;

        qual  = '/' + q.GetName();
        value = q.GetValue();

        switch ( q.GetStyle() ) {
        case CFlatQual::eEmpty:
            value.erase();
            break;
        case CFlatQual::eQuoted:
            qual += "=\"";
            value += '"';
            break;
        case CFlatQual::eUnquoted: 
            qual += '=';
            break;
        }

        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, GetWidth(), l, 0, m_FeatIndent, m_FeatIndent + qual);

        qual.erase();
        value.erase();
    }

    text_os.AddParagraph(l);
    */
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

void CGenbankFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os) const
{
    list<string> l;

    CNcbiOstrstream bc_line;

    bc_line 
        << right << setw(7) << bc.GetA() << " a"
        << right << setw(7) << bc.GetC() << " c"
        << right << setw(7) << bc.GetG() << " g"
        << right << setw(7) << bc.GetT() << " t";
    if ( bc.GetOther() > 0 ) {
        bc_line << right << setw(7) << bc.GetOther() << " others";
    }
    Wrap(l, "BASE COUNT", CNcbiOstrstreamToString(bc_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CGenbankFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os) const
{
    list<string> l;
    CNcbiOstrstream seq_line;

    const CSeqVector& vec = seq.GetSequence();

    TSeqPos base_count = seq.GetFrom();
    CSeqVector::const_iterator iter = vec.begin();
    while ( iter ) {
        seq_line << setw(9) << right << base_count;
        for ( TSeqPos count = 0; count < 60  &&  iter; ++count, ++iter, ++base_count ) {
            if ( count % 10 == 0 ) {
                seq_line << ' ';
            }
            seq_line << (char)tolower(*iter);
        }
        seq_line << '\n';
    }

    if ( seq.IsFirst() ) {
        l.push_back("ORIGIN      ");
    }
    NStr::Split(CNcbiOstrstreamToString(seq_line), "\n", l);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// END SECTION

void CGenbankFormatter::FormatEndSection
(const CEndSectionItem& end,
 IFlatTextOStream& text_os) const
{
    list<string> l;
    l.push_back("//");
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// DBSOURCE

void CGenbankFormatter::FormatDBSource
(const CDBSourceItem& dbs,
 IFlatTextOStream& text_os) const
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
 IFlatTextOStream& text_os) const
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
(const CPrimaryItem& prim,
 IFlatTextOStream& text_os) const
{
    // !!!
}


///////////////////////////////////////////////////////////////////////////
//
// GENOME

void CGenbankFormatter::FormatGenome
(const CGenomeItem& genome,
 IFlatTextOStream& text_os) const
{
    // !!!
}

///////////////////////////////////////////////////////////////////////////
//
// CONTIG

void CGenbankFormatter::FormatContig
(const CContigItem& contig,
 IFlatTextOStream& text_os) const
{
    // !!!
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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

