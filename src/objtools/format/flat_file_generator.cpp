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
* Author:  Mati Shomrat
*
* File Description:
*   User interface for generating flat file reports from ASN.1
*   
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objtools/format/format_item_ostream.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/flat_expt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC

// constructor

CFlatFileGenerator::CFlatFileGenerator
(CScope& scope,
 TFormat format,
 TMode   mode,
 TStyle  style,
 TFilter filter,
 TFlags  flags) :
    m_Ctx(new CFFContext(scope, format, mode, style, filter, flags))
{
    if ( !m_Ctx ) {
       NCBI_THROW(CFlatException, eInternal, "Unable to initialize context");
    }
}


// destructor

CFlatFileGenerator::~CFlatFileGenerator(void)
{
}


// Set annotation selector for feature gathering

SAnnotSelector& CFlatFileGenerator::SetAnnotSelector(void)
{
    return *(m_Ctx->SetAnnotSelector());
}


// Generate a flat-file report for a Seq-entry
void CFlatFileGenerator::Generate
(const CSeq_entry_Handle& entry,
 CFlatItemOStream& item_os)
{
    _ASSERT(entry  &&  entry.Which() != CSeq_entry::e_not_set);

    m_Ctx->SetTSE(entry);

    CRef<CFlatItemFormatter>  formatter(CFlatItemFormatter::New(m_Ctx->GetFormat()));
    if ( !formatter ) {
        NCBI_THROW(CFlatException, eInternal, "Unable to initialize formatter");
    }
    formatter->SetContext(*m_Ctx);
    item_os.SetFormatter(formatter);

    CRef<CFlatGatherer> gatherer(CFlatGatherer::New(m_Ctx->GetFormat()));
    if ( !gatherer ) {
        NCBI_THROW(CFlatException, eInternal, "Unable to initialize gatherer");
    }
    gatherer->Gather(*m_Ctx, item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_submit& submit,
 CFlatItemOStream& item_os)
{
    _ASSERT(submit.CanGetData());
    _ASSERT(submit.CanGetSub());
    _ASSERT(submit.GetData().IsEntrys());
    _ASSERT(!submit.GetData().GetEntrys().empty());

    // NB: though the spec specifies a submission may contain multiple entries
    // this is not the case. A submission should only have a single Top-level
    // Seq-entry
    CConstRef<CSeq_entry> e(submit.GetData().GetEntrys().front());
    if ( e ) {
        CSeq_entry_Handle entry = m_Ctx->GetScope().GetSeq_entryHandle(*e);
        m_Ctx->SetSubmit(submit.GetSub());
        Generate(entry, item_os);
    }
}


void CFlatFileGenerator::Generate
(const CSeq_loc& loc,
 CFlatItemOStream& item_os)
{
    if ( !loc.IsWhole()  ||  !loc.IsInt() ) {
        NCBI_THROW(CFlatException, eInvalidParam, 
            "locations must be an interval on a bioseq (or whole)");
    }

    CBioseq_Handle bsh = m_Ctx->GetScope().GetBioseqHandle(loc);
    if ( !bsh ) {
        NCBI_THROW(CFlatException, eInvalidParam, "location not in scope");
    }
    CSeq_entry_Handle entry = bsh.GetParentEntry();
    if ( !entry ) {
        NCBI_THROW(CFlatException, eInvalidParam, "Id not in scope");
    }
    m_Ctx->SetLocation(&loc);

    Generate(entry, item_os);
}


void CFlatFileGenerator::Generate(const CSeq_submit& submit, CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(submit, *item_os);
}


void CFlatFileGenerator::Generate(const CSeq_entry_Handle& entry, CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, *item_os);
}


void CFlatFileGenerator::Generate(const CSeq_loc& loc, CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(loc, *item_os);
}

//////////////////////////////////////////////////////////////////////////////
//
// DEPRECATED

void CFlatFileGenerator::Generate
(const CSeq_entry& entry,
 CFlatItemOStream& item_os)
{
    ERR_POST_ONCE(Warning<<
        "CFlatFileGenerator::Generate(const CSeq_entry&, ...) is deprecated"
        <<" use CFlatFileGenerator::Generate(const CSeq_entry_Handle, ...).");
    
    CSeq_entry_Handle h = m_Ctx->GetScope().GetSeq_entryHandle(entry);
    Generate(h, item_os);
}


void CFlatFileGenerator::Generate(const CSeq_entry& entry, CNcbiOstream& os)
{
    ERR_POST_ONCE(Warning<<
        "CFlatFileGenerator::Generate(const CSeq_entry&, ...) is deprecated"
        <<" use CFlatFileGenerator::Generate(const CSeq_entry_Handle, ...).");

    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, os);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/03/25 20:38:17  shomrat
* Use handles
*
* Revision 1.5  2004/02/11 22:52:06  shomrat
* allow user to specify selector for feature gathering
*
* Revision 1.4  2004/02/11 16:58:28  shomrat
* bug fix
*
* Revision 1.3  2004/01/14 16:13:11  shomrat
* Added GFF foramt; changed internal representation
*
* Revision 1.2  2003/12/18 17:43:33  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:21:05  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
