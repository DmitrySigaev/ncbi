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
 * Author:  Denis Vakatov, Aleksey Grichenko, Aaron Ucko
 *
 * File Description:
 *   New IDFETCH network client (get data from "ID1")
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <objects/entrez2/E2Reply.hpp>
#include <objects/entrez2/E2Request.hpp>
#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_db_id.hpp>
#include <objects/entrez2/Entrez2_docsum.hpp>
#include <objects/entrez2/Entrez2_docsum_data.hpp>
#include <objects/entrez2/Entrez2_docsum_list.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/Entrez2_reply.hpp>
#include <objects/entrez2/Entrez2_request.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/id1/ID1server_back.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/ID1server_request.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/gbloader.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/util/genbank.hpp>
#include <objects/util/sequence.hpp>

#include <memory>
#include <algorithm>


BEGIN_NCBI_SCOPE
USING_SCOPE(NCBI_NS_NCBI::objects); // MSVC requires qualification (!)


/////////////////////////////////
//  CId1FetchApp::
//

class CId1FetchApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

private:
    bool LookUpGI(int gi);
    int  LookUpSeqID(const CSeq_id& id);
    int  LookUpFastaSeqID(const string& s);
    int  LookUpFlatSeqID(const string& s);

    bool CheckEntrezReply  (const CE2Reply& rep);

    void WriteFastaIDs     (const list< CRef< CSeq_id > >& ids);

    void WriteFastaEntry   (const CBioseq& bioseq);
    void WriteFastaEntry   (const CSeq_entry& seq);

    void WriteHistoryTable (const CID1server_back& id1_reply);

    void WriteQualityScores(const CID1server_back& id1_reply);

    CNcbiOstream*                 m_OutputFile;
    auto_ptr<CConn_ServiceStream> m_ID1_Server;
    auto_ptr<CConn_ServiceStream> m_E2_Server;
    CRef<CObjectManager>          m_ObjMgr;
    CRef<CScope>                  m_Scope;
};


void CId1FetchApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI
    arg_desc->AddOptionalKey
        ("gi", "SeqEntryID",
         "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("gi", new CArgAllow_Integers(0, 99999999));

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump the resulting data in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("fmt", &(*new CArgAllow_Strings,
                  "asn", "asnb", "xml", "raw", "genbank", "genpept", "fasta",
                  "quality", "docsum"));

    // Output datafile
    arg_desc->AddDefaultKey
        ("out", "ResultFile",
         "File to dump the resulting data to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);

    // Log file
    arg_desc->AddOptionalKey
        ("log", "LogFile",
         "File to post errors and messages to",
         CArgDescriptions::eOutputFile,
         0);

    // Database to use
    arg_desc->AddOptionalKey
        ("db", "Database", // was "-d" in `idfetch'
         "Database to use",
         CArgDescriptions::eString);
    
    // Entity number
    arg_desc->AddOptionalKey
        ("ent", "EntityNumber", // was "-e" in `idfetch'
         "(Sub)entity number (retrieval number) to dump",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("ent", new CArgAllow_Integers(0, 99999999));

    // Type of lookup
    arg_desc->AddDefaultKey
        ("lt", "LookupType", // combination of "-i" (!) and "-n" in `idfetch'
         "Type of lookup",
         CArgDescriptions::eString, "entry");
    arg_desc->SetConstraint
        ("lt", &(*new CArgAllow_Strings,
                 "entry", "state", "ids", "history", "revisions", "none"));
    
    // File with list of stuff to dump
    arg_desc->AddOptionalKey
        ("in", "RequestFile", // was "-G" (!) in `idfetch'
         "File with list of GIs, (versioned) accessions, FASTA SeqIDs to dump",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
         
    // Maximum complexity
    arg_desc->AddDefaultKey
        ("maxplex", "MaxComplexity", // was "-c" in `idfetch'
         "Maximum complexity to return",
         CArgDescriptions::eString, "entry");
    arg_desc->SetConstraint
        ("maxplex", &(*new CArgAllow_Strings,
                      "entry", "bioseq", "bioseq-set", "nuc-prot",
                      "pub-set"));
    
    // Flattened SeqID
    arg_desc->AddOptionalKey
        ("flat", "FlatID", // was "-f" in `idfetch'
         "Flattened SeqID; format can be\n"
         "\t'type([name][,[accession][,[release][,version]]])'"
         " [e.g., '5(HUMHBB)'],\n"
         "\ttype=accession[.version], or type:number",
         CArgDescriptions::eString);
    
    // FASTA-style SeqID
    arg_desc->AddOptionalKey
        ("fasta", "FastaID", // was "-s" in `idfetch'
         "FASTA-style SeqID, in the form \"type|data\"; choices are\n"
         "\tlcl|int lcl|str bbs|int bbm|int gim|int gb|acc|loc emb|acc|loc\n"
         "\tpir|acc|name sp|acc|name pat|country|patent|seq ref|acc|name|rel\n"
         "\tgnl|db|id gi|int dbj|acc|loc prf|acc|name pdb|entry|chain\n"
         "\ttpg|acc|name tpe|acc|name tpd|acc|name",
         CArgDescriptions::eString);

    // Generate GI list by Entrez query
    arg_desc->AddOptionalKey
        ("query", "EntrezQueryString", // was "-q" in `idfetch'
         "Generate GI list by Entrez query given on command line",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("qf", "EntrezQueryFile", // was "-Q" in `idfetch'
         "Generate GI list by Entrez query in given file",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    // Program description
    string prog_description =
        "Fetch SeqEntry from ID server by its GI ID, possibly obtained from\n"
        "its SeqID or an Entrez query";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);


    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


// Workaround for "replace_if"
inline bool s_IsControl(char c)
{
    return iscntrl(c) ? true : false;
}


int CId1FetchApp::Run(void)
{
    // Process command line args
    const CArgs& args = GetArgs();

    // Setup and tune logging facilities
    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }
#ifdef _DEBUG
    // SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
#endif

    // Make sure the combination of arguments is valid
    {{
        int id_count = 0;
        const string& fmt = args["fmt"].AsString();

        if (args["gi"])     id_count++;
        if (args["in"])     id_count++;
        if (args["flat"])   id_count++;
        if (args["fasta"])  id_count++;
        if (args["query"])  id_count++;
        if (args["qf"])     id_count++;

        if (id_count != 1) {
            throw CArgException("You must supply exactly one argument"
                                " indicating what to look up.");
        }
        if ((args["query"]  ||  args["qf"]  ||  fmt == "docsum")
            &&  !args["db"]) {
            ERR_POST("No Entrez database supplied.  Try -db Nucleotide or "
                     "-db Protein.");
            return -1;
        }
        if ((fmt == "genbank"  ||  fmt == "genpept"  ||  fmt == "quality")
            &&  args["lt"].AsString() != "entry") {
            ERR_POST("The output format '" << fmt
                     << "' is only available for Seq-Entries.");
            return -1;
        }
    }}

    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));

    // Open output file
    m_OutputFile = &args["out"].AsOutputFile();

    // Open connections to servers we may need
    STimeout tmout;  tmout.sec = 9;  tmout.usec = 0;  
    m_ID1_Server.reset
        (new CConn_ServiceStream("ID1",     fSERV_Any, 0, 0, &tmout));
    m_E2_Server.reset
        (new CConn_ServiceStream("Entrez2", fSERV_Any, 0, 0, &tmout));

    // Set up object manager
    m_ObjMgr = new CObjectManager;
    m_Scope = new CScope(*m_ObjMgr);

    if (args["gi"]) {
        if ( !LookUpGI(args["gi"].AsInteger()) )
            return -1;
    }

    if (args["fasta"]) {
        int gi = LookUpFastaSeqID(args["fasta"].AsString());
        if (gi <= 0  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["flat"]) {
        int gi = LookUpFlatSeqID(args["flat"].AsString());
        if (gi <= 0  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["in"]) {
        CNcbiIstream& is = args["in"].AsInputFile();
        while (is  &&  !is.eof()) {
            string id;
            int    gi;

            is >> id;
            if (id.find('|') != NPOS) {
                gi = LookUpFastaSeqID(id);
            } else if (id.find_first_of(":=(") != NPOS) {
                gi = LookUpFlatSeqID(id);
            } else {
                gi = NStr::StringToInt(id);
            }

            if (gi <= 0  ||  !LookUpGI(gi)) {
                return -1;
            }
        }
    }

    if (args["query"]  ||  args["qf"]) {
        // Form query
        CRef<CEntrez2_boolean_element> e2_element
            (new CEntrez2_boolean_element);

        if (args["query"]) {
            e2_element->SetStr(args["query"].AsString());
        } else {
            CNcbiIstream& is = args["qf"].AsInputFile();
            CNcbiOstrstream oss;
            oss << is.rdbuf();
            string& str = e2_element->SetStr();
            str.assign(oss.str(), oss.pcount());
            oss.freeze(false);
            replace_if(str.begin(), str.end(), s_IsControl, ' ');
        }

        {{
            // Compose request to "Entrez2" service
            CEntrez2_request e2_request;
            e2_request.SetTool("id1_fetch");
            CEntrez2_eval_boolean& eb =
                e2_request.SetRequest().SetEval_boolean();
            eb.SetReturn_UIDs(true);
            CEntrez2_boolean_exp& query = eb.SetQuery();
            query.SetExp().push_back(e2_element);
            query.SetDb() = args["db"].AsString();

            // Send it to the server
            CObjectOStreamAsnBinary e2_server_output(*m_E2_Server);
            e2_server_output << e2_request;
            e2_server_output.Flush();
        }}

        // "Entrez2" service response
        CEntrez2_reply e2_reply;

        // Get response from "Entrez2" service
        {{
            CObjectIStream& e2_server_input = *CObjectIStream::Open
                (eSerial_AsnBinary, *m_E2_Server, false);
            e2_server_input >> e2_reply;
        }}

        // Deal with bad or unsuccessful queries
        if ( !CheckEntrezReply(e2_reply.GetReply()) ) {
            return -1;
        }
        if ( !e2_reply.GetReply().GetEval_boolean().GetCount() ) {
            ERR_POST("Entrez query returned no results.");
            return -1;
        }

        // Query succeeded; proceed to next stage of lookup
        for (CEntrez2_id_list::TConstUidIterator it
                 = e2_reply.GetReply().GetEval_boolean().GetUids()
                 .GetConstUidIterator();
             !it.AtEnd();  ++it) {
            if ( !LookUpGI(*it) ) {
                return -1;
            }
        }
    }

    return 0;
}


bool CId1FetchApp::LookUpGI(int gi)
{    
    const CArgs&         args             = GetArgs();
    CConn_ServiceStream* server           = m_ID1_Server.get();
    bool                 use_objmgr       = false;
    bool                 using_id1_server = true;
    const string&        fmt              = args["fmt"].AsString();
    const string&        lt               = args["lt"].AsString();

    m_ObjMgr->RegisterDataLoader( *new CGBDataLoader("GENBANK"));
    m_Scope->AddDataLoader("GENBANK");

    // Compose request to appropriate server
    CID1server_request id1_request;
    CEntrez2_request   e2_request;

    if (lt == "none") {
        *m_OutputFile << gi << NcbiEndl;
        return true;  // Done
    } else if (fmt == "docsum") {
        // Handling this here costs some efficiency when the GI came
        // from an Entrez query in the first place, but wins on generality.
        server = m_E2_Server.get();
        using_id1_server = false;
        e2_request.SetTool("id1_fetch");
        CEntrez2_id_list& uids = e2_request.SetRequest().SetGet_docsum();
        uids.SetDb() = args["db"].AsString();
        uids.SetNum(1);
        uids.SetUids().resize(uids.sm_UidSize);
        CEntrez2_id_list::TUidIterator it = uids.GetUidIterator();
        *it = gi;
    } else if (lt == "entry") {
        using_id1_server = false;
        use_objmgr = true;
    }
      else if (lt == "state") {
        id1_request.SetGetgistate(gi);
    } else if (lt == "ids") {
        using_id1_server = false;
        use_objmgr = true;
    } else if (lt == "history") {
        id1_request.SetGetgihist(gi);
    } else if (lt == "revisions") {
        id1_request.SetGetgirev(gi);
    }

    CID1server_back id1_reply;
    CEntrez2_reply e2_reply;
    if (!use_objmgr) {
        {{
            CObjectOStreamAsnBinary server_output(*server);

            // Send request to the server
            if (using_id1_server) {
                server_output << id1_request;
            } else {
                server_output << e2_request;
            }
            server_output.Flush();
        }}

        // Get response (Seq-Entry) from the server, dump it to the
        // output data file in the requested format

        // Dump the raw data coming from server "as is", if so specified
        if (fmt == "raw") {
            *m_OutputFile << server->rdbuf();
            return true;  // Done
        }    

        {{
            // Read server response in ASN.1 binary format
            //### Use CObjectIStream::Open() since only this function
            //### supports opening streams with non-blocking read.
            CObjectIStream& server_input
                = *CObjectIStream::Open(eSerial_AsnBinary, *server, false);

            if (using_id1_server) {
                server_input >> id1_reply;
            } else {
                server_input >> e2_reply;
            }
        }}
    }

    CSeq_id id;
    id.SetGi(gi);
    CBioseq_Handle handle;
    if (use_objmgr) {
        // What about db, ent, and maxplex?
        handle = m_Scope->GetBioseqHandle(id);
    }

    // Dump server response in the specified format
    ESerialDataFormat format = eSerial_None;
    if        (fmt == "asn") {
        format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        format = eSerial_Xml;
    } else if (fmt == "docsum") {
        // Deal with bad or unsuccessful queries
        if ( !CheckEntrezReply(e2_reply.GetReply()) ) {
            return false;
        }
        if ( !e2_reply.GetReply().GetGet_docsum().GetCount() ) {
            ERR_POST("Entrez query returned no results.");
            return false;
        }

        const CEntrez2_docsum& docsum
            = *e2_reply.GetReply().GetGet_docsum().GetList().front();
        string caption, title;
        for (CTypeConstIterator<CEntrez2_docsum_data> it = ConstBegin(docsum);
             it;  ++it) {
            // Should this be case-insensitive?
            if (it->GetField_name() == "Caption") {
                caption = it->GetField_value();
            } else if (it->GetField_name() == "Title") {
                title = it->GetField_value();
            }
        }
        *m_OutputFile << '>';
        if ( !caption.empty() ) {
            *m_OutputFile << caption;
        }
        *m_OutputFile << ' ';
        if ( !title.empty() ) {
            *m_OutputFile << title;
        }
    } else if (fmt == "fasta"  &&  lt == "ids") {
        if (!use_objmgr) {
            WriteFastaIDs( id1_reply.GetIds() );
        }
        else {
            WriteFastaIDs( handle.GetBioseq().GetId());
        }
    } else if (fmt == "fasta"  &&  lt == "entry") {
        if (!use_objmgr) {
            WriteFastaEntry(id1_reply.GetGotseqentry());
        } else {
            WriteFastaEntry(handle.GetTopLevelSeqEntry());
        }
    } else if (fmt == "fasta"  &&  lt == "state") {
        int state = id1_reply.GetGistate();
        *m_OutputFile << "gi = " << gi << ", states: ";
        switch (state & 0xff) {
        case  0: *m_OutputFile << "NONEXISTANT"; break; // was "NOT EXIST"
        case 10: *m_OutputFile << "DELETED";     break;
        case 20: *m_OutputFile << "REPLACED";    break;
        case 40: *m_OutputFile << "LIVE";        break;
        default: *m_OutputFile << "UNKNOWN";     break;
        }
        if (state & 0x100) {
            *m_OutputFile << "|SUPPRESSED";
        }
        if (state & 0x200) {
            *m_OutputFile << "|WITHDRAWN";
        }
        if (state & 0x400) {
            *m_OutputFile << "|CONFIDENTIAL";
        }
    } else if (fmt == "fasta") {
        // `lt' should be "history" or "revisions"
        WriteHistoryTable(id1_reply);
    } else if (fmt == "quality") {
        WriteQualityScores(id1_reply);
    } else if (fmt == "genbank") {
        const CSeq_entry& entry = handle.GetTopLevelSeqEntry();
        CGenbankWriter(*m_OutputFile, *m_Scope,
                       CGenbankWriter::eFormat_Genbank).Write(entry);
    } else if (fmt == "genpept") {
        const CSeq_entry& entry = handle.GetTopLevelSeqEntry();
        CGenbankWriter(*m_OutputFile, *m_Scope,
                       CGenbankWriter::eFormat_Genpept).Write(entry);
    }

    if (format != eSerial_None) {
        auto_ptr<CObjectOStream> id1_client_output
           (CObjectOStream::Open(format, *m_OutputFile));
        if (!use_objmgr) {
            *id1_client_output << id1_reply;
        } else {
            const CSeq_entry& entry = handle.GetTopLevelSeqEntry();
            *id1_client_output << entry;
        }
    }

    if (fmt != "asnb"  &&  fmt != "raw") {
        *m_OutputFile << NcbiEndl;
    }

    return true;  // Done
}


// Cleanup
void CId1FetchApp::Exit(void)
{
    SOCK_ShutdownAPI();
    SetDiagStream(0);
}


static vector<string> s_SplitString(const string& s, char delimiter)
{
    vector<string> pieces;
    SIZE_TYPE pos, start = 0;

    do {
        pos = s.find(delimiter, start);
        pieces.push_back(s.substr(start, pos - start));
        _TRACE("SplitString: got piece " << pieces.back());
        start = pos + 1;
    } while (pos != NPOS);

    return pieces;
}


int CId1FetchApp::LookUpFastaSeqID(const string& s)
{
    return LookUpSeqID(CSeq_id(s));
}


int CId1FetchApp::LookUpFlatSeqID(const string& s)
{
    CSeq_id::E_Choice type = static_cast<CSeq_id::E_Choice>(atoi(s.c_str()));
    SIZE_TYPE pos = s.find_first_of(":=(");
    if (pos == NPOS) {
        THROW0_TRACE(runtime_error("Malformatted flat ID " + s));
    }
    string data = s.substr(pos + 1);

    switch (s[pos]) {
    case ':':
    case '=':
        return LookUpSeqID(CSeq_id(type, data, kEmptyStr));
    case '(':
    {
        data.erase(data.end() - 1);
        // remove last character, which should be ')'
        vector<string> pieces = s_SplitString(data, ',');
        pieces.resize(4, kEmptyStr);
        // name acc rel ver -> acc name ver rel
        return LookUpSeqID(CSeq_id(type, pieces[1], pieces[0], pieces[3],
                                   pieces[2]));
    }
    default: // can't happen, but shut the compiler up
        return -1;
    }
}


int CId1FetchApp::LookUpSeqID(const CSeq_id& id)
{
    CID1server_request      request;
    CObjectOStreamAsnBinary server_output(*m_ID1_Server);

    SerialAssign<CSeq_id>(request.SetGetgi(), id);
    server_output << request;

    CID1server_back         reply;
    CObjectIStreamAsnBinary server_input(*m_ID1_Server);

    server_input >> reply;
    if ( reply.IsError() ) {
        CNcbiOstrstream oss;
        id.WriteAsFasta(oss);
        THROW1_TRACE(runtime_error, "Unable to find seq_id for "
            + (string)CNcbiOstrstreamToString(oss));
    }

    return reply.GetGotgi();
}


bool CId1FetchApp::CheckEntrezReply(const CE2Reply& rep)
{
    if ( rep.IsError() ) {
        ERR_POST("[Entrez] " << rep.GetError()
                 << "\nTry -db Nucleotide or -db Protein?");
        return false;
    }

    return true;
}


void CId1FetchApp::WriteFastaIDs(const list< CRef< CSeq_id > >& ids)
{
    iterate (list< CRef< CSeq_id > >, it, ids) {
        if (it != ids.begin()) {
            *m_OutputFile << '|';
        }
        (*it)->WriteAsFasta(*m_OutputFile);
    }
}


void CId1FetchApp::WriteFastaEntry(const CBioseq& bioseq)
{
    CBioseq_Handle handle =
        m_Scope->GetBioseqHandle(*(bioseq.GetId().front()));

    // Print the identifier(s) and description.
    *m_OutputFile << '>';
    WriteFastaIDs(bioseq.GetId());
    if (bioseq.IsSetDescr()) {
        iterate (CSeq_descr::Tdata, it2, bioseq.GetDescr().Get()) {
            if ((*it2)->IsName()) {
                *m_OutputFile << ' ' << (*it2)->GetName();
                break;
            }
        }
    }
    *m_OutputFile << ' ' << GetTitle(handle);

    // Now print the actual sequence in an appropriate ASCII format.
    {{
        CSeqVector vec = handle.GetSeqVector();
        vec.SetIupacCoding();
        for (TSeqPos pos = 0;  pos < vec.size();  ++pos) {
            if (pos % 70 == 0)
                *m_OutputFile << NcbiEndl;
            *m_OutputFile << vec[pos];
        }
        *m_OutputFile << NcbiEndl;
    }}
}

void CId1FetchApp::WriteFastaEntry(const CSeq_entry& seq)
{
    if (seq.IsSeq()) {
        WriteFastaEntry(seq.GetSeq());
    } else {
        iterate ( list< CRef< CSeq_entry > >, it, seq.GetSet().GetSeq_set()) {
            WriteFastaEntry(**it);
        }
    }
}


// for formatting text
class CTextColumn
{
public:
    CTextColumn() : m_Width(0) { }
    CTextColumn& Add(string s) {
        m_Strings.push_back(s);
        if (s.size() > m_Width)
            m_Width = s.size();
        return *this;
    }
    string Get(unsigned int index) const {
        const string& s = m_Strings[index];
        return s + string(m_Width - s.size(), ' ');
    }
    SIZE_TYPE Width()  const { return m_Width; }
    size_t    Height() const { return m_Strings.size(); }
 
private:
    SIZE_TYPE      m_Width;
    vector<string> m_Strings;
};


void CId1FetchApp::WriteHistoryTable(const CID1server_back& id1_reply)
{
    CTextColumn gis, dates, dbs, numbers;
    gis.Add("GI").Add("--");
    dates.Add("Loaded").Add("------");
    dbs.Add("DB").Add("--");
    numbers.Add("Retrieval No.").Add("-------------");
    for (CTypeConstIterator<CSeq_hist_rec> it = ConstBegin(id1_reply);
         it;  ++it) {
        int    gi = 0;
        string dbname, number;

        if ( it->GetDate().IsStr() ) {
            dates.Add(it->GetDate().GetStr());
        } else {
            CNcbiOstrstream oss;
            const CDate_std& date = it->GetDate().GetStd();
            oss << setfill('0') << setw(2) << date.GetMonth() << '/'
                << setw(2) << date.GetDay() << '/' << date.GetYear();
            dates.Add(CNcbiOstrstreamToString(oss));
        }

        iterate (CSeq_hist_rec::TIds, it2, it->GetIds()) {
            if ( (*it2)->IsGi() ) {
                gi = (*it2)->GetGi();
            } else if ( (*it2)->IsGeneral() ) {
                dbname = (*it2)->GetGeneral().GetDb();
                const CObject_id& tag = (*it2)->GetGeneral().GetTag();
                if ( tag.IsStr() ) {
                    number = tag.GetStr();
                } else {
                    number = NStr::IntToString(tag.GetId());
                }
            }
        }

        gis.Add(NStr::IntToString(gi));
        dbs.Add(dbname);
        numbers.Add(number);
    }

    for (unsigned int n = 0;  n < gis.Height();  n++) {
        *m_OutputFile << gis.Get(n) << "  " << dates.Get(n) << "  "
                      << dbs.Get(n) << "  " << numbers.Get(n) << NcbiEndl;
    }
}



void CId1FetchApp::WriteQualityScores(const CID1server_back& id1_reply)
{   
    /* Test case:
     * /net/ncbi/ncbi/ftp/genbank/quality_scores/gbvrt.qscore.gz
     *  GI 13508865: gotseqentry.set.annot.data.graph
     *  >AL590146.2 Phrap Quality (Length:121086, Min: 31, Max: 99)
     *   99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99
     *   ...
     */
    string id;

    {
        CTypeConstIterator<CTextseq_id> it = ConstBegin(id1_reply);
        for ( ;it;  ++it) {
            id = it->GetAccession() + '.' + NStr::IntToString(it->GetVersion());
            BREAK(it);
        }
    }

    CTypeConstIterator<CSeq_graph> it = ConstBegin(id1_reply);
    for ( ; it;  ++it) {
        string title = it->GetTitle();
        if (title.find("uality") == NPOS) {
            continue;
        }

        const CByte_graph& data = it->GetGraph().GetByte();
        *m_OutputFile << '>' << id << ' ' << title
                      << " (Length: " << it->GetNumval()
                      << ", Min: " << data.GetMin()
                      << ", Max: " << data.GetMax() << ')' << NcbiEndl;
        for (SIZE_TYPE n = 0;  n < data.GetValues().size();  ++n) {
            *m_OutputFile << setw(3) << static_cast<int>(data.GetValues()[n]);
            if (n % 20 == 19) {
                *m_OutputFile << NcbiEndl;
            }
        }
    }
}

END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CId1FetchApp().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2002/06/06 23:46:24  vakatov
* Use GetTitle() from <objects/util/sequence.hpp>
*
* Revision 1.31  2002/05/06 16:13:46  ucko
* Merge in Andrei Gourianov's changes to use the new OM (thanks!)
* Remove some dead code.
* Don't automatically turn on tracing, even when building with _DEBUG;
* it is always possible to set DIAG_TRACE in the environment instead.
* Move CVS log to end.
*
*
* *** These four entries are from src/app/id1_fetch1/id1_fetch1.cpp ***
* Revision 1.5  2002/05/06 03:31:52  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:21  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/04/10 21:02:21  gouriano
* moved construction of iterators out of "for" loop initialization:
* Sun Workshop compiler does not call destructors of such objects
* in case we use break to exit the loop
*
* Revision 1.2  2002/04/10 16:07:30  gouriano
* corrected data output in different formats
*
* Revision 1.1  2002/04/04 16:31:36  gouriano
* id1_fetch1 - modified version of id1_fetch, which uses objmgr1
*
* Revision 1.29  2002/03/11 21:52:05  lavr
* Print complete debug and trace information when compiled with _DEBUG
*
* Revision 1.28  2002/01/16 22:14:00  ucko
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.27  2001/12/07 21:15:16  ucko
* Remove duplicate includes.
*
* Revision 1.26  2001/12/07 21:11:01  grichenk
* Fixed includes to work with the updated datatool
*
* Revision 1.25  2001/12/07 21:03:47  ucko
* Add #includes required by new datatool version.
*
* Revision 1.24  2001/11/16 16:06:45  ucko
* Handle new Entrez docsum interface properly.
*
* Revision 1.23  2001/10/26 14:49:16  ucko
* Restructured to avoid CRefs as arguments.
*
* Revision 1.22  2001/10/23 20:05:12  ucko
* Request ASCII from CSeq_vector.
*
* Revision 1.21  2001/10/17 21:17:53  ucko
* Seq_vector now properly starts from zero rather than one; adjust code
* that uses it accordingly.
*
* Revision 1.20  2001/10/12 19:32:58  ucko
* move BREAK to a central location; move CBioseq::GetTitle to object manager
*
* Revision 1.19  2001/10/12 15:34:01  ucko
* Edit in-source version of CVS log to avoid end-of-comment marker.  (Oops.)
*
* Revision 1.18  2001/10/12 15:29:08  ucko
* Drop {src,include}/objects/util/asciiseqdata.* in favor of CSeq_vector.
* Rewrite GenBank output code to take fuller advantage of the object manager.
*
* Revision 1.17  2001/10/10 16:02:53  ucko
* Clean up includes.
*
* Revision 1.16  2001/10/04 19:11:56  ucko
* Centralize (rudimentary) code to get a sequence's title.
*
* Revision 1.15  2001/09/25 20:31:13  ucko
* Work around bug in Workshop's handling of declarations in for-loop
* initializers.
*
* Revision 1.14  2001/09/25 20:12:02  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
*
* Revision 1.13  2001/09/25 13:26:36  lavr
* CConn_ServiceStream() - arguments adjusted
*
* Revision 1.12  2001/09/24 03:22:09  vakatov
* Un-#include <cstdlib> and <cctype> which break IRIX/MIPSpro compilation and
* apparently are not needed after all
*
* Revision 1.11  2001/09/21 22:38:59  ucko
* Cope with new Entrez interface; fix MSVC build.
*
* Revision 1.10  2001/09/05 16:25:56  ucko
* Adapted to latest revision of object manager interface.
*
* Revision 1.9  2001/09/05 14:44:37  ucko
* Use NStr::IntToString instead of Stringify.
*
* Revision 1.8  2001/09/04 16:20:53  ucko
* Dramatically fleshed out id1_fetch
*
* Revision 1.7  2001/07/19 19:40:20  lavr
* Typo fixed
*
* Revision 1.6  2001/06/01 18:43:44  vakatov
* Comment out excessive debug/trace printout
*
* Revision 1.5  2001/05/16 17:55:37  grichenk
* Redesigned support for non-blocking stream read operations
*
* Revision 1.4  2001/05/11 20:41:16  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.3  2001/05/11 14:06:45  grichenk
* The first working revision
*
* Revision 1.2  2001/04/13 14:09:34  grichenk
* Next debug version, still not working
*
* Revision 1.1  2001/04/10 22:39:04  vakatov
* Initial revision.
* Compiles and links, but apparently is not working yet.
*
* ===========================================================================
*/
