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
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/scope.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objtools/format/format_item_ostream.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/context.hpp>


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
    m_Scope(scope), m_Format(format), m_Mode(mode), m_Style(style),
    m_Filter(filter), m_Flags(flags)
{
}


// destructor

CFlatFileGenerator::~CFlatFileGenerator(void)
{
}



// Generate a flat-file report for a Seq-entry

void CFlatFileGenerator::Generate
(const CSeq_entry& entry,
 CFlatItemOStream& item_os)
{
    CRef<CFFContext> ctx(new CFFContext(m_Scope, m_Format, 
        m_Mode, m_Style, m_Filter, m_Flags));
    ctx->SetTSE(entry);

    CRef<CFlatItemFormatter>  formatter(CFlatItemFormatter::New(m_Format));
    item_os.SetFormatter(formatter);

    CRef<CFlatGatherer> gatherer(CFlatGatherer::New(m_Format));
    if ( gatherer ) {
        gatherer->Gather(*ctx, item_os);
    }
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
    const CSeq_entry& entry = *(submit.GetData().GetEntrys().front());

    CRef<CFFContext> ctx(new CFFContext(m_Scope, m_Format, 
        m_Mode, m_Style, m_Filter, m_Flags));
    ctx->SetSubmit(submit.GetSub());
    Generate(entry, item_os);
}



void CFlatFileGenerator::Generate(const CSeq_entry& entry, CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, *item_os);
}


void CFlatFileGenerator::Generate(const CSeq_submit& submit, CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(submit, *item_os);
}

//////////////////////////////////////////////////////////////////////////////
//
// PRIVATE



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/12/18 17:43:33  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:21:05  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
