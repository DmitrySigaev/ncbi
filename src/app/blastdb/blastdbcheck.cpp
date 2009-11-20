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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Simple application to test integrity of databases.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <util/random_gen.hpp>
#include "blastdb_aux.hpp"

#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"

#include <iostream>
#include <sstream>
#include <set>
#include <vector>

USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
///  CBlastDbCheckApplication: the main application class
class CBlastDbCheckApplication : public CNcbiApplication {
public:
    /** @inheritDoc */
    CBlastDbCheckApplication () {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
    
private:
    /** @inheritDoc */
    virtual void Init(void);
    /** @inheritDoc */
    virtual int  Run(void);
    /** @inheritDoc */
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

/// Verbosity levels.
enum {
    e_Silent,
    e_Brief,
    e_Summary,
    e_Details,
    e_Minutiae,
    e_Max = e_Minutiae
};

/// Types of tests (bit).
enum {
    e_IsamLookup = 1
};

static string s_VerbosityString(int v)
{
    switch(v) {
    case e_Silent:   return "Quiet";
    case e_Brief:    return "Brief";
    case e_Summary:  return "Summary";
    case e_Details:  return "Detailed";
    case e_Minutiae: return "Minutiae";
    default:
        return "";
    };
}

static string s_VerbosityText()
{
    string rv;
    
    for(int i = 0; i <= e_Max; i++) {
        string s = s_VerbosityString(i);
        
        if (rv.size()) {
            rv += ", ";
        }
        
        rv += NStr::IntToString(i) + "=" + s;
    }
    
    return rv;
}

void CBlastDbCheckApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "BLAST database integrity and validity "
                              "checking application");
    
    arg_desc->SetCurrentGroup("Input Options");

    arg_desc->AddOptionalKey
        ("db", "DbName",
         "Specify a database name.",
         CArgDescriptions::eString);
    
    arg_desc->AddDefaultKey("dbtype", "molecule_type",
                            "Molecule type of database",
                            CArgDescriptions::eString,
                            "guess");
    
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));
    
    arg_desc->AddOptionalKey
        ("dir", "DirName",
         "Specify a directory containing one or more databases.",
         CArgDescriptions::eString);
    arg_desc->SetDependency("dir", CArgDescriptions::eExcludes, "db");
    
    arg_desc->AddFlag
        ("recursive",
         "Specify true to recurse through all dbs in directory tree.");
    arg_desc->SetDependency("recursive", CArgDescriptions::eExcludes, "db");
    
    
    arg_desc->SetCurrentGroup("Output Options");
    
    // Describe the expected command-line arguments
    arg_desc->AddOptionalKey
        ("logfile", "LogFile",
         "If specified, output will be redirected to this file.",
         CArgDescriptions::eOutputFile);
    
    arg_desc->AddDefaultKey
        ("verbosity", "DefaultKey",
         s_VerbosityText(),
         CArgDescriptions::eInteger,
         NStr::IntToString(e_Summary));
    arg_desc->SetConstraint("verbosity", new
                            CArgAllowValuesBetween((int)e_Silent,
                                                   (int)e_Max, true));
    
    arg_desc->SetCurrentGroup("Test Methods");
    
    arg_desc->AddFlag
        ("full",
         "If true, test every sequence (warning: may be slow).");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "stride");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "random");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "ends");
    arg_desc->SetDependency("full", CArgDescriptions::eExcludes, "isam");
    
// Threading and multiprocess support should not be too complex, but
// I'll defer writing them until it is more obvious that there is
// actually a need for them.
//
//     arg_desc->AddFlag
//         ("fork",
//          "If true, fork() will be used to protect main app from crashes.");
//
//     arg_desc->AddDefaultKey
//         ("threads", "NumThreads",
//          "Number of threads (or processes with -fork) to use at once.",
//          CArgDescriptions::eInteger, "1");
    
    arg_desc->AddDefaultKey
        ("stride", "StrideLength",
         "Check integrity of every Nth sequence.",
         CArgDescriptions::eInteger, "10000");
    
    arg_desc->AddDefaultKey
        ("random", "NumSequences",
         "Check this many randomly selected sequences.",
         CArgDescriptions::eInteger, "200");
    
    arg_desc->AddDefaultKey
        ("ends", "NumSequences",
         "Check this many sequences at each end of the database.",
         CArgDescriptions::eInteger, "200");
    
    arg_desc->AddDefaultKey
        ("isam", "IsamLookups",
         "Specify false to disable ISAM testing.",
         CArgDescriptions::eBoolean, "T");
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


typedef set<int> TSeen;


class CBlastDbCheckLog {
public:
    CBlastDbCheckLog(ostream & outp, int max_level)
        : m_Level(max_level), m_Output(outp)
    {
    }
    
    ostream & Log(int L)
    {
        if (L <= m_Level) {
            return m_Output;
        } else {
            m_DevNull.seekp(0);
            return m_DevNull;
        }
    }
    
    int GetLevel()
    {
        return m_Level;
    }
    
private:
    CBlastDbCheckLog(const CBlastDbCheckLog &);
    CBlastDbCheckLog & operator=(const CBlastDbCheckLog &);
    
    int m_Level;
    ostream & m_Output;
    ostringstream m_DevNull;
};


class CWrap;
class CTestAction;


class CTestAction : public CObject {
public:
    CTestAction(CBlastDbCheckLog & log, const string & nm, int flags)
        : m_Log(log), m_TestName(nm), m_Flags(flags)
    {
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & seen) = 0;
    
    ostream & Log(CSeqDB & db, int lvl)
    {
        m_Log.Log(lvl) << db.GetDBNameList() << " / " << m_TestName << ": ";
        return m_Log.Log(lvl);
    }
    
    ostream & LogMore(int lvl)
    {
        return m_Log.Log(lvl);
    }
    
    string Name()
    {
        return m_TestName;
    }
    
    bool TestOID(CSeqDB & db, TSeen & seen, int oid);
    
    int LogLevel()
    {
        return m_Log.GetLevel();
    }
    
private:
    CBlastDbCheckLog & m_Log;
    const string m_TestName;
    int m_Flags;
};


class CWrap : public CObject {
public:
    virtual bool DoTest(CTestAction & action, CSeqDB & db, TSeen & seen) = 0;
};


// class CForkWrap : public CWrap {
// public:
//     virtual bool DoTest(CTestAction & action, CSeqDB & db, TSeen & seen)
//     {
//         throw runtime_error("unimp: virtual void CForkWrap");
//     }
// };


class CTestActionList : public CTestAction {
public:
    // This test does not actually use the flags yet.
    CTestActionList(CBlastDbCheckLog & log, int flags)
        : CTestAction(log, "All Tests", flags)
    {
    }
    
    void Add(CTestAction * action)
    {
        m_List.push_back(CRef<CTestAction>(action));
    }
    
    void SetWrap(CWrap * wrap)
    {
        m_Wrap = wrap;
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & seen)
    {
        bool success = true;
        bool oneline = (LogLevel() == e_Summary);
        
        if (oneline) {
            Log(db, e_Brief) << "... " << flush;
        }
        
        NON_CONST_ITERATE(vector< CRef<CTestAction> >, iter, m_List) {
            CTestAction & test = **iter;
            
            bool ok = false;
            string msg;
            
            try {
                if (m_Wrap) {
                    ok = m_Wrap->DoTest(test, db, seen);
                } else {
                    ok = test.DoTest(db, seen);
                }
                if (ok) {
                    test.Log(db, e_Details) << " : pass" << endl;
                } else {
                    if (oneline) {
                        test.LogMore(e_Summary) << "\n";
                        oneline = false;
                    }
                    
                    test.Log(db, e_Brief) << " : fail" << endl;
                }
            }
            catch(exception & e) {
                msg = e.what();
            }
            
            if (msg.size()) {
                test.Log(db, e_Brief)
                    << "Caught exception: << " << msg << " >>" << endl;
            }
            
            if (! ok) {
                success = false;
                break;
            }
        }
        
        int lvl = success ? e_Summary : e_Brief;
        string msg = success ? "PASS" : "FAIL";
        
        if (oneline) {
            LogMore(lvl) << msg << endl;
        } else {
            Log(db, lvl) << msg << endl;
        }

        return success;
    }
    
private:
    vector< CRef<CTestAction> > m_List;
    CRef<CWrap> m_Wrap;
};


class CMetaDataTest : public CTestAction {
public:
    // This test does not actually use the flags yet.
    CMetaDataTest(CBlastDbCheckLog & log, int flags)
        : CTestAction(log, "MetaData", flags)
    {
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & /*seen*/)
    {
        // Here I get more values than I actually have useful tests
        // for, because I want to trigger exceptions due to binary
        // file corruption in the all the myriad files.
        
        int noid = db.GetNumOIDs();
        int nseq = db.GetNumSeqs();
        /*string t =*/ db.GetTitle();
        string d = db.GetDate();
        Uint8 tl = db.GetTotalLength();
        Uint8 vl = db.GetVolumeLength();
        
        vector<string> vols;
        db.FindVolumePaths(vols);
        
        int nv = vols.size();
        
        SSeqDBTaxInfo taxinfo;
        db.GetTaxInfo(9606, taxinfo);
        
        bool ok = true;
        
        if (! d.size()) {
            Log(db, e_Brief) << "db has empty date string" << endl;
            ok = false;
        }
        if (! nv) {
            Log(db, e_Brief) << "db has no volumes" << endl;
            ok = false;
        }
        if ((nseq > noid) || ((! tl) != (! noid)) || ((! vl) && tl)) {
            Log(db, e_Brief) << "sequence count/length mismatch" << endl;
            ok = false;
        }
        string hs = taxinfo.scientific_name;
        if (hs != "Homo sapiens") {
            Log(db, e_Brief) << "tax info looks wrong (" << hs << ")" << endl;
            ok = false;
        }
        
        string s1("a"), s2("b");
        
        for(int i = 0; i < 100000; i++) {
            string s3 = s1 + s2;
            
            if (s3.size() > 100) {
                s3 = "c";
            }
            
            if (i & 1) {
                s1 = s3;
            } else {
                s2 = s3;
            }
        }
        
        return ok;
    }
};


bool CTestAction::TestOID(CSeqDB & db, TSeen & seen, int oid)
{
    ostringstream details;
    ostringstream minutiae;
    
    // If we've seen this OID before (for this db instance), assume 'true'.
    
    if (seen.find(oid) != seen.end()) {
        return true;
    }
    
    seen.insert(oid);
    
    string where;
    bool rv = true;
    
    try {
        // Getting the SeqIDs implies getting the headers.
        where = "headers";
        list< CRef<CSeq_id> > seqids = db.GetSeqIDs(oid);
        
        // These non-bioseq things are a subset of the work done for
        // GetBioseq.  By doing these first, we might have a better
        // idea where the failure actually occured than by calling
        // GetBioseq() directly.
        
        where = "sequence";
        const char * p = NULL;
        int length = db.GetSequence(oid, & p);
        if ((length == 0) || (p == NULL)) {
            throw runtime_error("sequence data is empty");
        }
        
        db.RetSequence(& p);
        
        // Finally, the bioseq, which also tests the taxinfo dbs.
        
        where = "bioseq";
        CRef<CBioseq> bs = db.GetBioseq(oid);
        if (bs.Empty()) {
            throw runtime_error("no bioseq");
        }
        
        // Reverse look up all the Seq-ids.
        
        if (m_Flags & e_IsamLookup) {
            where = "isam lookups";
            ITERATE(list< CRef<CSeq_id> >, iter, seqids) {
                int oid2(-1);
            
                ostringstream msg;
            
                if ((! db.SeqidToOid(**iter, oid2)) || (oid != oid2)) {
                    if (! db.SeqidToOid(**iter, oid2)) {
                        msg << "seqid=" << (**iter).AsFastaString();
                        throw runtime_error(msg.str());
                    } else if (oid != oid2) {
                        msg << "oid1=" << oid
                            << " oid2=" << oid2
                            << " seqid=" << (**iter).AsFastaString();
                        throw runtime_error(msg.str());
                    }
                }
            }
        }
    }
    catch(CSeqDBException & e) {
        details << "Failed during " << where
                << ", oid " << oid
                << ": " << e.what() << endl;
        
        rv = false;
    }
    catch(exception & e) {
        details << where
                << " failed; oid=" << oid
                << ": " << e.what() << endl;
        
        rv = false;
    }
    
    minutiae << "Status for OID " << oid << ": "
             << (rv ? "PASS" : "FAIL") << endl;
    
    string msg = details.str();
    string msg2 = minutiae.str();
    
    if (msg.size()) {
        Log(db, e_Details) << msg << flush;
    }
    
    if (msg2.size()) {
        Log(db, e_Minutiae) << msg2 << flush;
    }
    
    return rv;
}


class CStrideTest : public CTestAction {
public:
    CStrideTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "Stride", flags), m_N(n)
    {
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & seen)
    {
        Log(db, e_Minutiae) << "<testing every " << m_N << "th OID>" << endl;
        
        for(int oid = 0; db.CheckOrFindOID(oid); oid += m_N) {
            if (! TestOID(db, seen, oid)) {
                return false;
            }
        }
        return true;
    }
    
private:
    int m_N;
};


class CSampleTest : public CTestAction {
public:
    CSampleTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "Sample", flags), m_N(n)
    {
        CTime now(CTime::eCurrent);
        time_t t = now.GetTimeT();
        m_Rng.SetSeed(t);
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & seen)
    {
        int max = db.GetNumOIDs();
        
        if (! max) {
            // Technically this is still a valid DB.
            Log(db, e_Details) << "Empty volume" << endl;
            return true;
        }
        
        set<int> oids;
        
        for(int i = 0; i < m_N;) {
            int oid = m_Rng.GetRand(0, max-1);
            // for alias DBs, not all OIDs will be fine, so check it first
            if (db.CheckOrFindOID(oid)) {
                oids.insert(oid);
                i++;
            }
        }
        
        Log(db, e_Minutiae) << "<testing " << m_N
                            << " randomly selected OIDs (" << oids.size()
                            << " unique)>" << endl;
        
        ITERATE(set<int>, iter, oids) {
            if (! TestOID(db, seen, *iter)) {
                return false;
            }
        }
        
        return true;
    }
    
private:
    CRandom m_Rng;
    int m_N;
};


class CEndsTest : public CTestAction {
public:
    CEndsTest(CBlastDbCheckLog & log, int n, int flags)
        : CTestAction(log, "EndPoints", flags), m_N(n)
    {
    }
    
    virtual bool DoTest(CSeqDB & db, TSeen & seen)
    {
        Log(db, e_Minutiae) << "<testing " << m_N << " OIDs at each end>" << endl;

        for(int oid = 0; db.CheckOrFindOID(oid); oid ++) {
            if (oid > m_N) {
                int new_oid = db.GetNumOIDs()-m_N;
                
                if (new_oid > oid) {
                    oid = new_oid;
                }
            }
            if (! TestOID(db, seen, oid))
                return false;
        }
        return true;
    }
    
private:
    /// Test m_N elements from the start, m_N elements from the end
    int m_N;
};


class CTestData : public CObject {
public:
    virtual bool Test(CTestAction & action) = 0;
};


class CDbTest : public CTestData {
public:
    CDbTest(CBlastDbCheckLog & outp, string db, string dbtype)
        : m_Out    (outp),
          m_Db     (db),
          m_DbType (dbtype)
    {
    }
    
    virtual bool Test(CTestAction & action);
    
private:
    CBlastDbCheckLog & m_Out;
    
    string m_Db;
    string m_DbType;

    void x_GetVolumeList(const vector <string> &dbs, 
                                CSeqDB::ESeqType stype, 
                                set <string> &list) const;
};

void CDbTest::x_GetVolumeList(const vector <string>  &dbs, 
                              CSeqDB::ESeqType       stype, 
                              set <string>          &list) const {

    ITERATE(vector<string>, iter, dbs) {
        vector <string> paths;
        try {
            CSeqDB::FindVolumePaths(*iter, stype, paths);
        } catch (...) {
            m_Out.Log(e_Summary) 
                 << endl << "BLAST AliasDB error: Could not find all volume or alias "
                 << "files referenced in " << *iter << ", [skipped]" << endl;
            continue;
        }
        list.insert(paths.begin(), paths.end());
    }
}

bool CDbTest::Test(CTestAction & action)
{
    TSeen seen;
    bool success = true;
    
    vector<string> dbs;
    NStr::Tokenize(m_Db, " ", dbs, NStr::eMergeDelims);
    
    CSeqDB::ESeqType seqtype = ParseTypeString(m_DbType);

    set <string> vol_list;
    
    x_GetVolumeList(dbs, seqtype, vol_list);
    
    m_Out.Log(e_Summary)
        << "Testing " << vol_list.size() << " volume(s)." << endl;
    
    int total = vol_list.size(), passed = 0;
    
    ITERATE(set<string>, iter, vol_list) {
        CRef<CSeqDB> db(new CSeqDB(*iter, seqtype));
        bool okay = action.DoTest(*db, seen);
        
        if (okay) {
            passed ++;
        } else {
            success = false;
        }
    }
    
    if (total == passed) {
        m_Out.Log(e_Brief)
            << " Result=SUCCESS. No errors reported for "
            << total << " volume(s)." << endl;
    } else {
        m_Out.Log(e_Brief)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported for "
            << total << " volume(s)." << endl;
    }
    
    return success;
}

class CDirTest : public CTestData {
public:
    CDirTest(CBlastDbCheckLog & outp,
             string dir,
             string dbtype,
             int threads,
             bool recurse)
        : m_Dir     (dir),
          m_Out     (outp),
          m_DbType  (dbtype),
          m_Threads (threads),
          m_Recurse (recurse)
    {
    }
    
    virtual bool Test(CTestAction & action);
    
private:
    string         m_Dir;
    vector<SSeqDBInitInfo> m_DBs;
    
    CBlastDbCheckLog & m_Out;
    
    string m_DbType;
    int    m_Threads;
    bool   m_Recurse;

    void x_GetVolumeList(CSeqDB::ESeqType stype, set <string> &list) const;
};

void CDirTest::x_GetVolumeList(CSeqDB::ESeqType stype, set <string> &list) const {

    ITERATE(vector<SSeqDBInitInfo>, iter, m_DBs) {
        if (iter->m_MoleculeType != stype) continue;
        vector <string> paths;
        try {
            CSeqDB::FindVolumePaths(iter->m_BlastDbName, iter->m_MoleculeType, paths);
        } catch (...) {
            m_Out.Log(e_Summary) 
                 << endl << "BLAST AliasDB error: Could not find all volume or alias "
                 << "files referenced in " << iter->m_BlastDbName << ", [skipped]" << endl;
            continue;
        }
        list.insert(paths.begin(), paths.end());
    }
   
};

bool CDirTest::Test(CTestAction & action)
{
    bool success = true;
    
    m_Out.Log(e_Summary)
        << "Finding database volumes..." << flush;
    
    m_DBs = FindBlastDBs(m_Dir, m_DbType, m_Recurse, true);

    set <string> prot_list;
    set <string> nucl_list;
    
    x_GetVolumeList(CSeqDB::eProtein, prot_list);
    x_GetVolumeList(CSeqDB::eNucleotide, nucl_list);
    
    m_Out.Log(e_Summary) << "(done)" << endl;
    
    int total = prot_list.size() + nucl_list.size(), passed = 0;

    m_Out.Log(e_Summary)
        << "Testing " << total << " volume(s)." << endl;
    
    
    ITERATE(set<string>, iter, prot_list) {

        CRef<CSeqDB> db (new CSeqDB(*iter, CSeqDB::eProtein));
        TSeen seen;
        
        bool okay = action.DoTest(*db, seen);
        
        if (okay) {
            passed ++;
        } else {
            success = false;
        }
    }
    
    ITERATE(set<string>, iter, nucl_list) {

        CRef<CSeqDB> db (new CSeqDB(*iter, CSeqDB::eNucleotide));
        TSeen seen;
        
        bool okay = action.DoTest(*db, seen);
        
        if (okay) {
            passed ++;
        } else {
            success = false;
        }
    }

    if (total == passed) {
        m_Out.Log(e_Brief)
            << " Result=SUCCESS. No errors reported for "
            << total << " volumes." << endl;
    } else {
        m_Out.Log(e_Brief)
            << " Result=FAILURE. "
            << (total-passed) << " errors reported for "
            << total << " volumes." << endl;
    }
    
    return success;
}


/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CBlastDbCheckApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    int status = 0;
    
    try {
        // Do run
        
        // Stream to result output
        // (NOTE: "x_lg" is just a workaround for bug in SUN WorkShop 5.1 compiler)
        ostream* lg = args["logfile"] ? &args["logfile"].AsOutputFile() : &cout;
        
        // Get verbosity as well
        const int verbosity = args["verbosity"].AsInteger();
        
        CBlastDbCheckLog output(*lg, verbosity);
        
        output.Log(e_Summary) << "Writing messages to ";
        
        if (args["logfile"]) {
            output.Log(e_Summary)
                << "file  (" << args["logfile"].AsString() << ")";
        } else {
            output.Log(e_Summary) << "<stdout>";
        }
        
        output.Log(e_Summary)
            << " at verbosity ("
            << s_VerbosityString(verbosity) << ")" << endl;
        
        // Get data source (db or directory) object.
        
        CRef<CTestData> data;
        
        string db(args["db"] ? args["db"].AsString() : "");
        string dir(args["dir"] ? args["dir"].AsString() : "");
        string dbtype(args["dbtype"].AsString());
        bool recurse = !! args["recursive"];
        //int threads = args["threads"].AsInteger();
        
        if ((db == "") == (dir == "")) {
            output.Log(e_Brief)
                << "error: Must specify exactly one of -dir or -db." << endl;
            
            return 1;
        } else if (db != "") {
            data.Reset(new CDbTest(output, db, dbtype));
        } else {
            data.Reset(new CDirTest(output, dir, dbtype, 1, recurse));
        }
        
        // Build action object (set of tests)
        
        bool full = !! args["full"];
        //bool fork1 = !! args["fork"];
        
        // FIXME: should use flags for all of these
        int stride = 1;
        int random_sample = 0;
        int end_amt = 0;
        bool isam = false;
        
        if (full) {
            output.Log(e_Summary)
                << "Using `full' mode: every OID will be tested." << endl;
            
            stride = 1;
            random_sample = 0;
            end_amt = 0;
        } else {
            stride = args["stride"].AsInteger();
            random_sample = args["random"].AsInteger();
            end_amt = args["ends"].AsInteger();
            isam = args["isam"].AsString() == "T";
        }
        
        // Behavior modification flags
        
        int flags = 0;
        
        output.Log(e_Summary)
            << "ISAM testing is " << (isam ? "EN" : "DIS") << "ABLED." << endl;
        
        if (isam) {
            flags |= e_IsamLookup;
        }
        
        // Test actions
        
        CRef<CTestActionList> tests(new CTestActionList(output, flags));
        
        tests->Add(new CMetaDataTest(output, flags));
        
        if (stride) {
            if (! full) {
                output.Log(e_Summary)
                    << "Testing every " << stride << "-th OID." << endl;
            }
            
            tests->Add(new CStrideTest(output, stride, flags));
        }
        
        if (random_sample) {
            output.Log(e_Summary)
                << "Testing " << random_sample << " randomly sampled OIDs." << endl;
            
            tests->Add(new CSampleTest(output, random_sample, flags));
        }
        
        if (end_amt) {
            output.Log(e_Summary)
                << "Testing first " << end_amt
                << " and last " << end_amt << " OIDs." << endl;
            
            tests->Add(new CEndsTest(output, end_amt, flags));
        }
        
        //if (fork1) {
        //    output.Log(e_Summary)
        //        << "Using fork() before each action for safety." << endl;
        //    
        //    tests->SetWrap(new CForkWrap);
        //}
        
        bool okay = data->Test(*tests);
        
        status = okay ? 0 : 1;
    } CATCH_ALL(status)
    return status;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastDbCheckApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBlastDbCheckApplication().AppMain(argc, argv, 0, eDS_Default, 0);
#endif /* SKIP_DOXYGEN_PROCESSING */
}
