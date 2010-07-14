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
* Authors:  Aleksey Grichenko
*
* File Description:
*   Test object manager with LDS2 data loader
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/lds2/lds2_db.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CLDS2TestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    // subdir and src_name are relative to m_SrcDir
    void x_ConvertDir(const string& subdir);
    void x_ConvertFile(const string& rel_name);

    void x_TestDatabase(const string& id);
    void x_InitStressTest(void);
    void x_RunStressTest(void);

    string              m_DbFile;
    string              m_SrcDir;
    string              m_DataDir;
    string              m_FmtName;
    ESerialDataFormat   m_Fmt;
    bool                m_RunStress;
};


void CLDS2TestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "LDS2 test program");

    arg_desc->AddDefaultKey("src_dir", "SrcDir",
        "Directory with the source text ASN.1 data files.",
        CArgDescriptions::eString, "lds2_data/asn");

    arg_desc->AddDefaultKey("db", "DbFile",
        "LDS2 database file name.",
        CArgDescriptions::eString, "lds2_data/lds2.db");

    arg_desc->AddOptionalKey("format", "DataFormat",
        "Data format to use. If other than asn, the source data will be " \
        "converted and saved to the sub-directory named by the format.",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("format", 
        &(*new CArgAllow_Strings, "asnb", "xml", "fasta"),
        CArgDescriptions::eConstraint);

    arg_desc->AddOptionalKey("id", "SeqId",
        "Seq-id to use in the database test.",
        CArgDescriptions::eString);

    arg_desc->AddFlag("stress", "Run stress test.");

    SetupArgDescriptions(arg_desc.release());
}


void CLDS2TestApplication::x_ConvertDir(const string& subdir)
{
    CDir src(CDirEntry::ConcatPath(m_SrcDir, subdir));
    CDir::TEntries subs = src.GetEntries("*", CDir::fIgnoreRecursive);
    ITERATE(CDir::TEntries, it, subs) {
        const CDirEntry& sub = **it;
        string rel = CDirEntry::CreateRelativePath(m_SrcDir, sub.GetPath());
        if ( sub.IsDir() ) {
            x_ConvertDir(rel);
        }
        else if ( sub.IsFile() ) {
            x_ConvertFile(rel);
        }
    }
}


void CLDS2TestApplication::x_ConvertFile(const string& rel_name)
{
    CFile src(CDirEntry::ConcatPath(m_SrcDir, rel_name));

    CFile tmp(CDirEntry::ConcatPath(m_DataDir, rel_name));
    CFile dst(CDirEntry::MakePath(tmp.GetDir(), tmp.GetBase(), m_FmtName));
    CDir(tmp.GetDir()).CreatePath();

    cout << "Converting " << src.GetName() << " to " << dst.GetName() << endl;

    CNcbiIfstream fin(src.GetPath().c_str(), ios::binary | ios::in);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, fin));

    CNcbiOfstream fout(dst.GetPath().c_str(), ios::binary | ios::out);
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(m_Fmt, fout));
    CObjectStreamCopier cp(*in, *out);

    while ( in->HaveMoreData() ) {
        try {
            string type_name = in->ReadFileHeader();
            if (type_name == "Seq-entry") {
                cp.Copy(CType<CSeq_entry>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Bioseq") {
                cp.Copy(CType<CBioseq>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Bioseq-set") {
                cp.Copy(CType<CBioseq_set>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-annot") {
                cp.Copy(CType<CSeq_annot>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-align") {
                cp.Copy(CType<CSeq_align>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-align-set") {
                cp.Copy(CType<CSeq_align_set>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else {
                ERR_POST("Unknown object in the source data: " << type_name);
                break;
            }
        }
        catch (CEofException) {
            break;
        }
    }
}


void CLDS2TestApplication::x_TestDatabase(const string& id)
{
    CSeq_id seq_id(id);
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(seq_id);
    cout << "Testing LDS2 database, seq-id: " << idh.AsString() << endl;

    string sep = "";
    CLDS2_Database::TBlobSet blobs;

    CRef<CLDS2_Database> db(new CLDS2_Database(m_DbFile));
    SLDS2_Blob blob = db->GetBlobInfo(idh);
    cout << "Main blob id: " << blob.id;
    if (blob.id > 0) {
        SLDS2_File file = db->GetFileInfo(blob.file_id);
        cout << " in " << file.name << " @ " << blob.file_pos << endl;
        cout << "LDS2 synonyms for the bioseq: ";
        CLDS2_Database::TLdsIdSet ids;
        db->GetSynonyms(idh, ids);
        ITERATE(CLDS2_Database::TLdsIdSet, it, ids) {
            cout << sep << *it;
            sep = ", ";
        }
        cout << endl;
    }
    else {
        cout << " (unknown seq-id or multiple bioseqs found)" << endl;
        cout << "All blobs with bioseq: ";
        db->GetBioseqBlobs(idh, blobs);
        sep = "";
        ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
            cout << sep << it->id;
            sep = ", ";
        }
        cout << endl;
    }

    blobs.clear();
    cout << "Blobs with internal annots: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_Internal, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;

    blobs.clear();
    cout << "Blobs with external annots: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_External, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;

    blobs.clear();
    cout << "Blobs with any annots for: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_All, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;
}


void CLDS2TestApplication::x_InitStressTest(void)
{
    cout << "Initializing stress test data..." << endl;
    // Create data files
    CSeq_entry e;
    {{
        string src = "./lds2_data/base_entry.asn";
        CNcbiIfstream fin(src.c_str());
        fin >> MSerial_AsnText >> e;
    }}

    m_DataDir = "./lds2_data/stress";
    CDir(m_DataDir).CreatePath();

    for (int f = 1; f < 100; f++) {
        string fname = CDirEntry::ConcatPath(m_DataDir,
            "data" + NStr::Int8ToString(f) + "." + m_FmtName);
        CNcbiOfstream fout(fname.c_str(), ios::binary | ios::out);
        auto_ptr<CObjectOStream> out;
        auto_ptr<CFastaOstream> fasta_out;
        if (m_FmtName == "fasta") {
            fasta_out.reset(new CFastaOstream(fout));
        }
        else {
            out.reset(CObjectOStream::Open(m_Fmt, fout));
        }

        for (int g = 0; g < 100; g++) {
            int gi = g + f*100;
            CSeq_id& id = *e.SetSeq().SetId().front();
            id.SetGi(gi);
            CSeq_feat& feat = *e.SetSeq().SetAnnot().front()->SetData().SetFtable().front();
            feat.SetLocation().SetWhole().SetGi(gi);
            feat.SetProduct().SetWhole().SetGi(gi+1);
            if ( fasta_out.get() ) {
                fasta_out->Write(e);
            }
            else {
                out->Write(&e, e.GetThisTypeInfo());
            }
        }
    }
}


class CLDS2_TestThread : public CThread
{
public:
    CLDS2_TestThread(int id);

    virtual void* Main(void);

private:
    int     m_Id;
    CScope  m_Scope;
};


CLDS2_TestThread::CLDS2_TestThread(int id)
    : m_Id(id),
      m_Scope(*CObjectManager::GetInstance())
{
    m_Scope.AddDefaults();
}


void* CLDS2_TestThread::Main(void)
{
    for (int i = 10; i < 990; i++) {
        int gi = i*10 + m_Id;
        CSeq_id seq_id;
        seq_id.SetGi(gi);
        CSeq_id_Handle id = CSeq_id_Handle::GetHandle(seq_id);
        CBioseq_Handle h = m_Scope.GetBioseqHandle(id);
        _ASSERT(h);

        SAnnotSelector sel;
        sel.SetSearchUnresolved()
            .SetResolveAll();
        CSeq_loc loc;
        loc.SetWhole().Assign(seq_id);
        CFeat_CI fit(m_Scope, loc, sel);
        int fcount = 0;
        for (; fit; ++fit) {
            fcount++;
        }

        sel.SetByProduct(true);
        CFeat_CI fitp(m_Scope, loc, sel);
        fcount = 0;
        for (; fitp; ++fitp) {
            fcount++;
        }
    }
    return 0;
}


void CLDS2TestApplication::x_RunStressTest(void)
{
    cout << "Running stress test" << endl;
    CStopWatch sw(CStopWatch::eStart);
    vector< CRef<CThread> > threads;
    for (int i = 0; i < 10; i++) {
        CRef<CThread> thr(new CLDS2_TestThread(i));
        threads.push_back(thr);
        thr->Run();
    }
    NON_CONST_ITERATE(vector< CRef<CThread> >, it, threads) {
        (*it)->Join();
    }
    double elapsed = sw.Elapsed();
    cout << "Finished stress test in " << elapsed << " sec (" <<
        elapsed/9800 << " per bioseq)" << endl;
}


int CLDS2TestApplication::Run(void)
{
    const CArgs& args(GetArgs());

    m_DbFile = args["db"].AsString();
    m_SrcDir = CDirEntry::CreateAbsolutePath(args["src_dir"].AsString());
    m_DataDir = m_SrcDir;
    m_Fmt = eSerial_AsnText;
    m_RunStress = args["stress"];

    if ( args["format"] ) {
        m_FmtName = args["format"].AsString();
        if (m_FmtName == "asnb") {
            m_Fmt = eSerial_AsnBinary;
        }
        else if (m_FmtName == "xml") {
            m_Fmt = eSerial_Xml;
        }
        else if (m_FmtName == "fasta") {
            // Only stress test supports fasta format
            if ( !m_RunStress ) {
                return 0;
            }
        }

        if ( !m_RunStress ) {
            m_DataDir = CDirEntry::NormalizePath(
                CDirEntry::ConcatPath(
                CDirEntry::ConcatPath(m_SrcDir, ".."), m_FmtName));
            x_ConvertDir("");
        }
    }

    if ( m_RunStress ) {
        x_InitStressTest();
    }

    cout << "Indexing data..." << endl;
    CRef<CLDS2_Manager> mgr(new CLDS2_Manager(m_DbFile));
    mgr->ResetData(); // Re-create the database
    mgr->AddDataDir(m_DataDir);
    mgr->SetGBReleaseMode(CLDS2_Manager::eGB_Guess);
    CStopWatch sw(CStopWatch::eStart);
    mgr->UpdateData();
    mgr.Reset();
    cout << "Data indexing done in " << sw.Elapsed() << " sec" << endl;

    if ( args["id"] ) {
        x_TestDatabase(args["id"].AsString());
    }

    // Run object manager tests
    CRef<CObjectManager> objmgr (CObjectManager::GetInstance());
    CLDS2_DataLoader::RegisterInObjectManager(*objmgr, m_DbFile,
        -1, CObjectManager::eDefault);

    if ( m_RunStress ) {
        x_RunStressTest();
    }
    else {
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    CLDS2TestApplication app;
    return app.AppMain(argc, argv, 0, eDS_Default);
}
