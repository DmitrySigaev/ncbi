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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger. Demonstration of CAlnMix usage.
*
* ===========================================================================
*/
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>

#include <objects/alnmgr/alnmix.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CAlnMrgApp : public CNcbiApplication
{
    virtual void     Init                (void);
    virtual int      Run                 (void);
    void             SetOptions          (void);
    void             LoadInputAlignments (void);
    void             PrintMergedAlignment(void);
    CRef<CDense_seg> ConvertStdsegToDenseg(const CStd_seg& ss);

private:
    CRef<CAlnMix>        m_Mix;
    CAlnMix::TMergeFlags m_MergeFlags;
    CAlnMix::TAddFlags   m_AddFlags;
};

void CAlnMrgApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Alignment merger demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("log", "LogFile",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("gen2est", "Gen2ESTMerge",
         "Perform Gen2EST Merge",
         CArgDescriptions::eBoolean, "t");

    arg_desc->AddDefaultKey
        ("gapjoin", "GapJoin",
         "Consolidate segments of equal lens with a gap on the query sequence",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("mingap", "MinGap",
         "Consolidate all segments with a gap on the query sequence",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("minusstrand", "MinusStrand",
         "Minus strand on the refseq when merging.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("calcscore", "CalcScore",
         "Calculate each aligned seq pair score and use it when merging.",
         CArgDescriptions::eBoolean, "f");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


// Create a Dense_seg from a Std_seg
CRef<CDense_seg> 
CAlnMrgApp::ConvertStdsegToDenseg(const CStd_seg& ss)
{
    CRef<CDense_seg> ds(new CDense_seg);

    return ds;
}


void CAlnMrgApp::LoadInputAlignments(void)
{
    CArgs args = GetArgs();

    CNcbiIstream& is = args["in"].AsInputFile();
    bool done = false;
    
    string asn_type;
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(eSerial_AsnText, is));

        asn_type = in->ReadFileHeader();
        is.seekg(0);
    }}

    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(eSerial_AsnText, is));
    
    CTypesIterator i;
    CType<CDense_seg>::AddTo(i);
    CType<CStd_seg>::AddTo(i);

    if (asn_type == "Seq-entry") {
        CRef<CSeq_entry> se(new CSeq_entry);
        *in >> *se;
        //m_Mix->GetScope().AddTopLevelSeqEntry(*se);
        for (i = Begin(*se); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_Mix->Add(*(CType<CDense_seg>::Get(i)),
                           m_AddFlags);
            } else if (CType<CStd_seg>::Match(i)) {
                m_Mix->Add(*ConvertStdsegToDenseg(*(CType<CStd_seg>::Get(i))),
                           m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-submit") {
        CRef<CSeq_submit> ss(new CSeq_submit);
        *in >> *ss;
        CType<CSeq_entry>::AddTo(i);
        int tse_cnt = 0;
        for (i = Begin(*ss); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_Mix->Add(*(CType<CDense_seg>::Get(i)),
                           m_AddFlags);
            } else if (CType<CStd_seg>::Match(i)) {
                m_Mix->Add(*ConvertStdsegToDenseg(*(CType<CStd_seg>::Get(i))),
                           m_AddFlags);
            } else if (CType<CSeq_entry>::Match(i)) {
                if ( !(tse_cnt++) ) {
                    m_Mix->GetScope().AddTopLevelSeqEntry
                        (*(CType<CSeq_entry>::Get(i)));
                }
            }
        }
    } else if (asn_type == "Seq-align") {
        CRef<CSeq_align> sa(new CSeq_align);
        *in >> *sa;
        for (i = Begin(*sa); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_Mix->Add(*(CType<CDense_seg>::Get(i)),
                           m_AddFlags);
            } else if (CType<CStd_seg>::Match(i)) {
                m_Mix->Add(*ConvertStdsegToDenseg(*(CType<CStd_seg>::Get(i))),
                           m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-align-set") {
        CRef<CSeq_align_set> sas(new CSeq_align_set);
        *in >> *sas;
        for (i = Begin(*sas); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_Mix->Add(*(CType<CDense_seg>::Get(i)),
                           m_AddFlags);
            } else if (CType<CStd_seg>::Match(i)) {
                m_Mix->Add(*ConvertStdsegToDenseg(*(CType<CStd_seg>::Get(i))),
                           m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-annot") {
        CRef<CSeq_annot> san(new CSeq_annot);
        *in >> *san;
        for (i = Begin(*san); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_Mix->Add(*(CType<CDense_seg>::Get(i)),
                           m_AddFlags);
            } else if (CType<CStd_seg>::Match(i)) {
                m_Mix->Add(*ConvertStdsegToDenseg(*(CType<CStd_seg>::Get(i))),
                           m_AddFlags);
                           
            }
        }
    } else if (asn_type == "Dense-seg") {
        CRef<CDense_seg> ds(new CDense_seg);
        *in >> *ds;
        m_Mix->Add(*ds, m_AddFlags);
    } else {
        cerr << "Cannot read: " << asn_type;
        return;
    }
}

void CAlnMrgApp::SetOptions(void)
{
    CArgs args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }

    m_MergeFlags = 0;

    if (args["gapjoin"]  &&  args["gapjoin"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fGapJoin;
    }

    if (args["mingap"]  &&  args["mingap"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fMinGap;
    }

    if (args["gen2est"]  &&  args["gen2est"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fGen2EST | CAlnMix::fTruncateOverlaps;
    }

    if (args["minusstrand"]  &&  args["minusstrand"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fNegativeStrand;
    }

    if (args["calcscore"]  &&  args["calcscore"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fCalcScore;
    }
}

void CAlnMrgApp::PrintMergedAlignment(void)
{
    auto_ptr<CObjectOStream> asn_out 
        (CObjectOStream::Open(eSerial_AsnText, cout));
        
    *asn_out << m_Mix->GetDenseg();
}

int CAlnMrgApp::Run(void)
{
    SetOptions();

    m_Mix = new CAlnMix();
    LoadInputAlignments();
    m_Mix->Merge(m_MergeFlags);
    PrintMergedAlignment();
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAlnMrgApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/04/03 21:17:48  todorov
* Adding demo projects
*
*
* ===========================================================================
*/
