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
    void             View3(int screen_width);
    void             View4(int screen_width);
    void             View5();
    void             View6();
    void             View7();
    void             GetSeqPosFromAlnPosDemo();
private:
    CRef<CAlnVec> m_AV;
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>  m_Scope;
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

    arg_desc->AddKey
        ("v", "",
         "View format:\n"
         "1. CSV table\n"
         "2. Popset style using chunks\n"
         "3. Popset style\n"
         "4. Popset style speed optimized\n"
         "5. Print segments\n"
         "6. Print chunks\n"
         "7. Alternative ways to get sequence\n",
         CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey
        ("w", "ScreenWidth",
         "Screen width for some of the viewers",
         CArgDescriptions::eInteger, "60");

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
        m_AV = new CAlnVec(*ds, *m_Scope);
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
    cout << ",";
    for (int seg=0; seg<m_AV->GetNumSegs(); seg++) {
        cout << "," << m_AV->GetLen(seg) << ",";
    }
    cout << endl;
    for (int row=0; row<m_AV->GetNumRows(); row++) {
        cout << row << ",";
        for (int seg=0; seg<m_AV->GetNumSegs(); seg++) {
            cout << m_AV->GetStart(row, seg) << "," 
                 << m_AV->GetStop(row, seg) << ",";
        }
        cout << endl;
    }
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


void CAlnMgrTestApp::View3(int screen_width)
{
    TSeqPos aln_len = m_AV->GetAlnStop() + 1;
    const CAlnMap::TNumrow nrows = m_AV->GetNumRows();
    const CAlnMap::TNumseg nsegs = m_AV->GetNumSegs();
    const CDense_seg::TStarts& starts = m_AV->GetDenseg().GetStarts();
    const CDense_seg::TLens& lens = m_AV->GetDenseg().GetLens();

    vector<string> buffer(nrows);
    for (CAlnMap::TNumrow row = 0; row < nrows; row++) {

        // allocate space for the row
        buffer[row].reserve(aln_len + 1);
        string buff;

        int seg, pos, left_seg = -1, right_seg = -1;
        TSignedSeqPos start;
        TSeqPos len;

        // determine the ending right seg
        for (seg = nsegs - 1, pos = seg * nrows + row;
             seg >= 0; --seg, pos -= nrows) {
            if (starts[pos] >= 0) {
                right_seg = seg;
                break;
            }
        }

        for (seg = 0, pos = row;  seg < nsegs; ++seg, pos += nrows) {
            len = lens[seg];
            if ((start = starts[pos]) >= 0) {

                left_seg = seg; // ending left seg is at most here

                m_AV->GetSeqString(buff, row, start, start + len - 1);
                buffer[row] += buff;
            } else {
                // add appropriate number of gap/end chars
                char* ch_buff = new char[len+1];
                char fill_ch;
                if (left_seg < 0  ||  seg > right_seg  &&  right_seg > 0) {
                    fill_ch = m_AV->GetEndChar();
                } else {
                    fill_ch = m_AV->GetGapChar(row);
                }
                memset(ch_buff, fill_ch, len);
                ch_buff[len] = 0;
                buffer[row] += ch_buff;
                delete[] ch_buff;
            }
        }
    }

    TSeqPos pos = 0;
    do {
        for (CAlnMap::TNumrow row = 0; row < nrows; row++) {
            cout << m_AV->GetSeqId(row)
                 << "\t"
                 << m_AV->GetSeqPosFromAlnPos(row, pos, CAlnMap::eLeft)
                 << "\t"
                 << buffer[row].substr(pos, screen_width)
                 << "\t"
                 << m_AV->GetSeqPosFromAlnPos(row, pos + screen_width - 1,
                                              CAlnMap::eLeft)
                 << endl;
        }
        cout << endl;
        pos += screen_width;
        if (pos + screen_width > aln_len) {
            screen_width = aln_len - pos;
        }
    } while (pos < aln_len);
}


void CAlnMgrTestApp::View4(int scrn_width)
{
    CAlnMap::TNumrow row, nrows = m_AV->GetNumRows();

    vector<string> buffer(nrows);
    vector<CAlnMap::TSeqPosList> insert_aln_starts(nrows);
    vector<CAlnMap::TSeqPosList> insert_starts(nrows);
    vector<CAlnMap::TSeqPosList> insert_lens(nrows);
    vector<CAlnMap::TSeqPosList> scrn_lefts(nrows);
    vector<CAlnMap::TSeqPosList> scrn_rights(nrows);
    
    // Fill in the vectors for each row
    for (row = 0; row < nrows; row++) {
        m_AV->GetWholeAlnSeqString
            (row,
             buffer[row],
             &insert_aln_starts[row],
             &insert_starts[row],
             &insert_lens[row],
             scrn_width,
             &scrn_lefts[row],
             &scrn_rights[row]);
    }
        
    // Visualization
    TSeqPos pos = 0, aln_len = m_AV->GetAlnStop() + 1;
    do {
        for (row = 0; row < nrows; row++) {
            cout << row 
                 << "\t"
                 << m_AV->GetSeqId(row)
                 << "\t" 
                 << scrn_lefts[row].front()
                 << "\t"
                 << buffer[row].substr(pos, scrn_width)
                 << "\t"
                 << scrn_rights[row].front()
                 << endl;
            scrn_lefts[row].pop_front();
            scrn_rights[row].pop_front();
        }
        cout << endl;
        pos += scrn_width;
        if (pos + scrn_width > aln_len) {
            scrn_width = aln_len - pos;
        }
    } while (pos < aln_len);
}



// print segments

void CAlnMgrTestApp::View5()
{
    CAlnMap::TNumrow row;

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
}



// print chunks

void CAlnMgrTestApp::View6()
{
    CArgs args = GetArgs();
    CAlnMap::TNumrow row;

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
}



// alternative ways to get the sequence

void CAlnMgrTestApp::View7()
{
    string buff;
    CAlnMap::TNumseg seg;
    CAlnMap::TNumrow row;

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
}


//////
// GetSeqPosFromAlnPos
void CAlnMgrTestApp::GetSeqPosFromAlnPosDemo()
{
    cout << "["
        << m_AV->GetSeqPosFromAlnPos(2, 1390, CAlnMap::eForward, false)
        << "-" 
        << m_AV->GetSeqPosFromAlnPos(2, 1390, (CAlnMap::ESearchDirection)7, false)
        << "]"
        << NcbiEndl;
}


int CAlnMgrTestApp::Run(void)
{
    CArgs args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }

    LoadDenseg();

    cout << "-----" << endl;

    if (args["a"]) {
        m_AV->SetAnchor(args["a"].AsInteger());
    }

    int screen_width = args["w"].AsInteger();
    m_AV->SetGapChar('-');
    m_AV->SetEndChar('.');
    if (args["v"]) {
        switch (args["v"].AsInteger()) {
        case 1: View1(); break;
        case 2: View2(screen_width); break;
        case 3: View3(screen_width); break;
        case 4: View4(screen_width); break;
        case 5: View5(); break;
        case 6: View6(); break;
        case 7: View7(); break;
        }
    }
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
* Revision 1.10  2003/09/26 15:30:07  todorov
* +Print segments
*
* Revision 1.9  2003/07/23 21:01:08  ucko
* Revert use of (uncommitted) BLAST DB data loader.
*
* Revision 1.8  2003/07/23 20:52:07  todorov
* +width, +aln_starts for the inserts in GetWhole..
*
* Revision 1.7  2003/07/17 22:48:17  todorov
* View4 implemented in CAlnVec::GetWholeAlnSeqString
*
* Revision 1.6  2003/07/17 21:06:44  todorov
* -v is now required param
*
* Revision 1.5  2003/07/14 20:25:18  todorov
* Added another, even faster viewer
*
* Revision 1.4  2003/07/08 19:27:46  todorov
* Added an speed-optimized viewer
*
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
