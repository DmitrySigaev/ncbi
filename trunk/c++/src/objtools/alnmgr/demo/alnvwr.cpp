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
*   Various alignment viewers. Demonstration of CAlnMap/CAlnVec usage.
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

#include <objmgr/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/alnmix.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

void LogTime(const string& s)
{
    static time_t prev_t;
    time_t        t = time(0);

    if (prev_t==0) {
        prev_t=t;
    }
    
    cout << s << " " << (int)(t-prev_t) << endl;
}

class CAlnMgrTestApp : public CNcbiApplication
{
    virtual void     Init(void);
    virtual int      Run(void);
    void             LoadDenseg(void);
    void             View1();
    void             View2(int screen_width);
private:
    CRef<CAlnVec> m_AV;
    CRef<CScope>  m_Scope;
    CRef<CObjectManager> m_ObjMgr;
};

void CAlnMgrTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Alignment manager demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "Name of file to read the Dense-seg from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("log", "LogFile",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("a", "AnchorRow",
         "Anchor row (zero based)",
         CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey
        ("v1", "",
         "View1: Prints in table format.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("v2", "",
         "View2: Prints in popset viewer style.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("cf", "GetChunkFlags",
         "Flags for GetChunks (CAlnMap::TGetChunkFlags)",
         CArgDescriptions::eInteger, "0");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CAlnMgrTestApp::LoadDenseg(void)
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
    
    //create scope
    {{
        m_ObjMgr = new CObjectManager;
        
        m_ObjMgr->RegisterDataLoader
            (*new CGBDataLoader("ID", NULL, 2),
             CObjectManager::eDefault);

        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }}

    if (asn_type == "Dense-seg") {
        CRef<CDense_seg> ds(new CDense_seg);
        *in >> *ds;
        m_AV = new CAlnVec(*ds);
    } else if (asn_type == "Seq-submit") {
        CRef<CSeq_submit> ss(new CSeq_submit);
        *in >> *ss;
        CTypesIterator i;
        CType<CDense_seg>::AddTo(i);
        CType<CSeq_entry>::AddTo(i);
        int tse_cnt = 0;
        for (i = Begin(*ss); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_AV = new CAlnVec(*(CType<CDense_seg>::Get(i)), *m_Scope);
            } else if (CType<CSeq_entry>::Match(i)) {
                if ( !(tse_cnt++) ) {
                    m_Scope->AddTopLevelSeqEntry
                        (*(CType<CSeq_entry>::Get(i)));
                }
            }
        }
    } else {
        cerr << "Cannot read: " << asn_type;
        return;
    }
}

void CAlnMgrTestApp::View1()
{
    cout << endl << "\t";
    for (int seg=0; seg<m_AV->GetNumSegs(); seg++) {
        cout << m_AV->GetLen(seg) << "\t";
    }
    for (int row=0; row<m_AV->GetNumRows(); row++) {
        cout << row << "\t";
        for (int seg=0; seg<m_AV->GetNumSegs(); seg++) {
            cout << m_AV->GetStart(row, seg);
        }
    }
    cout << endl;
}

void CAlnMgrTestApp::View2(int screen_width)
{
    int aln_pos = 0;
    CAlnMap::TSignedRange rng;

    do {
        // create range

        rng.Set(aln_pos, aln_pos + screen_width - 1);

        string aln_seq_str;
        aln_seq_str.reserve(screen_width + 1);
        // for each sequence
        for (CAlnMap::TNumrow row = 0; row < m_AV->GetNumRows(); row++) {
            cout << m_AV->GetSeqId(row)
                 << "\t" 
                 << m_AV->GetSeqPosFromAlnPos(row, rng.GetFrom(),
                                              CAlnMap::eLeft)
                 << "\t"
                 << m_AV->GetAlnSeqString(aln_seq_str, row, rng)
                 << "\t"
                 << m_AV->GetSeqPosFromAlnPos(row, rng.GetTo(),
                                              CAlnMap::eLeft)
                 << endl;
        }
        cout << endl;
        aln_pos += screen_width;
    } while (aln_pos < m_AV->GetAlnStop());
}

int CAlnMgrTestApp::Run(void)
{
    CArgs args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }

    CAlnMap::TNumrow row;
    CAlnMap::TNumseg seg;

    LoadDenseg();

    cout << "-----";

    if (args["a"]) {
        m_AV->SetAnchor(args["a"].AsInteger());
    }

    if (args["v1"]  &&  args["v1"].AsBoolean()) {
        View1();
    }
    if (args["v2"]  &&  args["v2"].AsBoolean()) {
        m_AV->SetGapChar('-');
        m_AV->SetEndChar('.');
        View2(40);
    }

    return 0;

    string buff;

    //////////////////////
    // print segments
    //////////////////////
    for (row=0; row<m_AV->GetNumRows(); row++) {
        cout << "Row: " << row << endl;
        for (int seg=0; seg<m_AV->GetNumSegs(); seg++) {
            
            // aln coords
            cout << m_AV->GetAlnStart(seg) << "-"
                 << m_AV->GetAlnStop(seg) << " ";


            // type
            CAlnMap::TSegTypeFlags type = m_AV->GetSegType(row, seg);
            if (type & CAlnMap::fSeq) {
                // seq coords
                cout << m_AV->GetStart(row, seg) << "-" 
                     << m_AV->GetStop(row, seg) << " (Seq)";
            } else {
                cout << "(Gap)";
            }

            if (type & CAlnMap::fNotAlignedToSeqOnAnchor) cout << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(type)) cout << "(Insert)";
            if (type & CAlnMap::fUnalignedOnRight) cout << "(UnalignedOnRight)";
            if (type & CAlnMap::fUnalignedOnLeft) cout << "(UnalignedOnLeft)";
            if (type & CAlnMap::fNoSeqOnRight) cout << "(NoSeqOnRight)";
            if (type & CAlnMap::fNoSeqOnLeft) cout << "(NoSeqOnLeft)";
            if (type & CAlnMap::fEndOnRight) cout << "(EndOnRight)";
            if (type & CAlnMap::fEndOnLeft) cout << "(EndOnLeft)";

            cout << NcbiEndl;
        }
    }
    cout << "---------" << endl;
    /////////////////////

    /////////////////////
    // print chunks
    /////////////////////
    CAlnMap::TSignedRange range(-1, m_AV->GetAlnStop()+1);

    for (row=0; row<m_AV->GetNumRows(); row++) {
        cout << "Row: " << row << endl;
        //CAlnMap::TSignedRange range(m_AV->GetSeqStart(row) -1,
        //m_AV->GetSeqStop(row) + 1);
        CRef<CAlnMap::CAlnChunkVec> chunk_vec = m_AV->GetAlnChunks(row, range, args["cf"].AsInteger());
    
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];

            cout << "[row" << row << "|" << i << "]";
            cout << chunk->GetAlnRange().GetFrom() << "-"
                 << chunk->GetAlnRange().GetTo() << " ";

            if (!chunk->IsGap()) {
                cout << chunk->GetRange().GetFrom() << "-"
                    << chunk->GetRange().GetTo();
            } else {
                cout << "(Gap)";
            }

            if (chunk->GetType() & CAlnMap::fSeq) cout << "(Seq)";
            if (chunk->GetType() & CAlnMap::fNotAlignedToSeqOnAnchor) cout << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(chunk->GetType())) cout << "(Insert)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnRight) cout << "(UnalignedOnRight)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnLeft) cout << "(UnalignedOnLeft)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnRight) cout << "(NoSeqOnRight)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnLeft) cout << "(NoSeqOnLeft)";
            if (chunk->GetType() & CAlnMap::fEndOnRight) cout << "(EndOnRight)";
            if (chunk->GetType() & CAlnMap::fEndOnLeft) cout << "(EndOnLeft)";
            cout << NcbiEndl;
        }
    }
    cout << "---------" << endl;

    // GetSequence
    m_AV->SetGapChar('-');
    m_AV->SetEndChar('.');
    for (seg=0; seg<m_AV->GetNumSegs(); seg++) {
        for (row=0; row<m_AV->GetNumRows(); row++) {
            cout << "row " << row << ", seg " << seg << " ";
            // if (m_AV->GetSegType(row, seg) & CAlnMap::fSeq) {
                cout << "["
                    << m_AV->GetStart(row, seg)
                    << "-"
                    << m_AV->GetStop(row, seg) 
                    << "]"
                    << NcbiEndl;
                for(int i=0; i<m_AV->GetLen(seg); i++) {
                    cout << m_AV->GetResidue(row, m_AV->GetAlnStart(seg)+i);
                }
                cout << NcbiEndl;
                cout << m_AV->GetSeqString(buff, row,
                                           m_AV->GetStop(row, seg),
                                           m_AV->GetStop(row, seg)) << NcbiEndl;
                cout << m_AV->GetSegSeqString(buff, row, seg) 
                    << NcbiEndl;
                //            } else {
                //                cout << "-" << NcbiEndl;
                //            }
            cout << NcbiEndl;
        }
    }


    //////
    // GetSeqPosFromAlnPos
    cout << "["
        << m_AV->GetSeqPosFromAlnPos(2, 1390, CAlnMap::eForward, false)
        << "-" 
        << m_AV->GetSeqPosFromAlnPos(2, 1390, (CAlnMap::ESearchDirection)7, false)
        << "]"
        << NcbiEndl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAlnMgrTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/06/04 18:20:40  todorov
* read seq-submit
*
* Revision 1.2  2003/06/02 16:06:41  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.1  2003/04/03 21:17:48  todorov
* Adding demo projects
*
*
* ===========================================================================
*/
