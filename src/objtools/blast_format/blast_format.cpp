/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_format.cpp

Author: Ilya Dondoshansky

Contents: Formatting of BLAST results (SeqAlign)

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <objmgr/object_manager.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/alnmgr/util/blast_format.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CBlastFormatOptions::CBlastFormatOptions(EProgram program, 
    CNcbiOstream &ostr) THROWS((CBlastException))
        : m_ostr(&ostr)
{
   m_believe_query = FALSE;
   m_descriptions = BLAST_NUM_DESCRIPTIONS;
   m_alignments = BLAST_NUM_ALIGNMENTS;
   
   m_print_options = 0;
   m_align_options = 0;
   m_align_options += ALIGN_COMPRESS;
   m_align_options += ALIGN_END_NUM;
   m_align_options += ALIGN_SHOW_GI;
   m_print_options += ALIGN_SHOW_GI;
   
   m_align_options += ALIGN_MATRIX_VAL;
   m_align_options += ALIGN_SHOW_QS;
   if (program == eBlastx)
      m_align_options += ALIGN_BLASTX_SPECIAL;
   
   m_align_view = BLAST_ALIGN_VIEW;
}

CBlastFormatOptions::~CBlastFormatOptions()
{
}

static void 
SetDisplayParameters(CDisplaySeqalign &display, 
    const CBlastFormatOptions* format_options, EProgram program)
{
    bool db_is_na = (program == eBlastn ||
                     program == eTblastn ||
                     program == eTblastx);
    bool query_is_na = (program == eBlastn ||
                        program == eBlastx ||
                        program == eTblastx);
    display.SetDbType(db_is_na);
    display.SetQueryType(query_is_na);
    
    int AlignOption=0;
    int align_view = format_options->GetAlignView();

    if (align_view == 1 ){
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eMasterAnchored;
        AlignOption += CDisplaySeqalign::eShowIdentity;
    } else if (align_view == 2){
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eMasterAnchored;
    } else if (align_view == 3 ) {
        AlignOption += CDisplaySeqalign::eMultiAlign;
        AlignOption += CDisplaySeqalign::eShowIdentity;
    } else if (align_view ==  4) {
        AlignOption += CDisplaySeqalign::eMultiAlign;
    }
    
    AlignOption += CDisplaySeqalign::eShowMiddleLine;
    if(program == eBlastn){
      display.SetMiddleLineStyle (CDisplaySeqalign::eBar); 
      display.SetAlignType(CDisplaySeqalign::eNuc);
    } else {
        display.SetMiddleLineStyle (CDisplaySeqalign::eChar);
        display.SetAlignType(CDisplaySeqalign::eProt);
    }
    AlignOption += CDisplaySeqalign::eShowBlastInfo;
    AlignOption += CDisplaySeqalign::eShowBlastStyleId;
    if(format_options->GetHtml()){
        AlignOption += CDisplaySeqalign::eHtml;
    }

    display.SetAlignOption(AlignOption);
}

int
BLAST_FormatResults(TSeqAlignVector &seqalignv, 
    EProgram program, const TSeqLocVector &query,
    TSeqLocInfoVector &maskv, const CBlastFormatOptions* format_options, 
    bool is_ooframe)
{
    unsigned int index;
    
    list <CDisplaySeqalign::FeatureInfo*> featureInfo;
    
    for (index = 0; index < seqalignv.size(); ++index) {
        if (!seqalignv[index]->IsSet())
            continue;
        
        query[index].scope->AddDataLoader("BLASTDB");
        CDisplaySeqalign display(*seqalignv[index], maskv[index], 
                                 featureInfo, 0, *query[index].scope);
        SetDisplayParameters(display, format_options, program);
        display.DisplaySeqalign(*format_options->GetOstream());
    }

    return 0;
}
END_SCOPE(blast)
END_NCBI_SCOPE
