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
* Author: Michael Kholodov
*
* File Description:
*   String representation of the database character types.
*
* ===========================================================================
*/


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <vector>

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  MAIN

class CDbapiTest : public CNcbiApplication
{
private:
    virtual void Init();
    virtual int Run();
    virtual void Exit();

  
    CArgDescriptions *argList;
};


void CDbapiTest::Init()
{

    argList = new CArgDescriptions();

    argList->SetUsageContext(GetArguments().GetProgramBasename(),
                             "DBAPI test program");


#ifdef WIN32
   argList->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "MSSQL2");

   argList->AddDefaultKey("d", "string",
                           "Driver <ctlib|dblib|ftds|odbc>",
                           CArgDescriptions::eString, 
                           "odbc");
#else
    argList->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "STRAUSS");

    argList->AddDefaultKey("d", "string",
                           "Driver <ctlib|dblib|ftds>",
                           CArgDescriptions::eString, 
                           "ctlib");
#endif

    SetupArgDescriptions(argList);
}
  
int CDbapiTest::Run() 
{

    CArgs args = GetArgs();

    IDataSource *ds = 0;

    try {
        
        CDriverManager &dm = CDriverManager::GetInstance();

        string server = args["s"].AsString();


#ifdef WIN32
        DBAPI_RegisterDriver_ODBC(dm);
        //DBAPI_RegisterDriver_DBLIB(dm);
#endif

        // set TDS version for STRAUSS
        if( NStr::CompareNocase(server, "STRAUSS") == 0 ) {
            map<string,string> attr;
            attr["version"] = "100";
            ds = dm.CreateDs(args["d"].AsString(), &attr);
        }
        else
            ds = dm.CreateDs(args["d"].AsString());

        ds->SetLogStream(0);


        //ds->SetLogStream(&NcbiCerr);

        IConnection* conn = ds->CreateConnection();

        conn->SetMode(IConnection::eBulkInsert);
        //conn->ForceSingle(true);

        conn->Connect("anyone",
                      "allowed",
                      server,
                      "DBAPI_Sample");
    
        NcbiCout << "Using server: " << server
                 << ", driver: " << args["d"].AsString() << endl;

    
        IStatement *stmt = conn->CreateStatement();

        string sql = "select int_val, fl_val, date_val, str_val \
from SelectSample ";
        NcbiCout << "Testing simple select..." << endl
                 << sql << endl;

        conn->MsgToEx(true);

        try {
            stmt->Execute(sql);
    
            while( stmt->HasMoreResults() ) {
                if( stmt->HasRows() ) {   
                    IResultSet *rs = stmt->GetResultSet();
                    const IResultSetMetaData *rsMeta = rs->GetMetaData();

                    rs->BindBlobToVariant(true);

                    for(int i = 1; i <= rsMeta->GetTotalColumns(); ++i )
                        NcbiCout << rsMeta->GetName(i) << "  ";

                    NcbiCout << endl;

                    while(rs->Next()) { 
                        for(int i = 1;  i <= rsMeta->GetTotalColumns(); ++i ) {
                            if( rsMeta->GetType(i) == eDB_Text
                                || rsMeta->GetType(i) == eDB_Image ) {

                                CDB_Stream *b = dynamic_cast<CDB_Stream*>(rs->GetVariant(i).GetData());
                                _ASSERT(b);

                                char *buf = new char[b->Size() + 1];
                                b->Read(buf, b->Size());
                                buf[b->Size()] = '\0';
                                NcbiCout << buf << "|";
                                delete buf;
                                
                            }
                            else
                                NcbiCout << rs->GetVariant(i).GetString() << "|";
                        }
                        NcbiCout << endl;
                                
                            
#if 0
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << rs->GetVariant(4).GetString()
                                 << endl;
#endif
                    
                    } 
                    
                }
            }
        }
        catch(CDB_Exception& e) {
            NcbiCout << "Exception: " << e.what() << endl;
            NcbiCout << ds->GetErrorInfo();
        } 

        NcbiCout << "Rows : " << stmt->GetRowCount() << endl;

        conn->MsgToEx(false);

        //delete stmt;

        // Testing bulk insert w/o BLOBs
        NcbiCout << "Creating BulkSample table..." << endl;
        sql = "if exists( select * from sysobjects \
where name = 'BulkSample' \
AND type = 'U') \
begin \
	drop table BulkSample \
end";
        stmt->ExecuteUpdate(sql);


        sql = "create table BulkSample (\
	id int not null, \
	ord int not null, \
    mode tinyint not null, \
    date datetime not null)";

        stmt->ExecuteUpdate(sql);

        //I_DriverContext *ctx = ds->GetDriverContext();
        //delete ctx;

        try {
            //Initialize table using bulk insert
            NcbiCout << "Initializing BulkSample table..." << endl;
            IBulkInsert *bi = conn->CreateBulkInsert("BulkSample", 4);
            CVariant b1(eDB_Int);
            CVariant b2(eDB_Int);
            CVariant b3(eDB_TinyInt);
            CVariant b4(eDB_DateTime);
            bi->Bind(1, &b1);
            bi->Bind(2, &b2);
            bi->Bind(3, &b3);
            bi->Bind(4, &b4);
            int i;
            for( i = 0; i < 10; ++i ) {
                b1 = i;
                b2 = i * 2;
                b3 = Uint1(i + 1);
                b4 = CTime(CTime::eCurrent);
                bi->AddRow();
                //bi->StoreBatch();
            }
            
            bi->Complete();

        }
        catch(...) {
            throw;
        }


        // create a stored procedure
        sql = "if exists( select * from sysobjects \
where name = 'SampleProc' \
AND type = 'P') \
begin \
	drop proc SampleProc \
end";
        stmt->ExecuteUpdate(sql);


        sql = "create procedure SampleProc \
	@id int, \
	@f float, \
    @o int output \
as \
begin \
  select int_val, fl_val, date_val from SelectSample \
  where int_val < @id and fl_val <= @f \
  select @o = 555 \
  select 2121, 'Parameter @id:', @id, 'Parameter @f:', @f, 'Parameter @o:', @o  \
  return @id \
end";

        stmt->ExecuteUpdate(sql);

        float f = 2.999f;

        // call stored procedure
        NcbiCout << "Calling stored procedure..." << endl;
        
        ICallableStatement *cstmt = conn->PrepareCall("SampleProc", 3);
        cstmt->SetParam(CVariant(5), "@id");
        cstmt->SetParam(CVariant::Float(&f), "@f");
        cstmt->SetOutputParam(CVariant(eDB_Int), "@o");
        cstmt->Execute(); 
    
        while(cstmt->HasMoreResults()) {
            IResultSet *rs = cstmt->GetResultSet();

            //NcbiCout << "Row count: " << cstmt->GetRowCount() << endl;

            if( rs == 0 )
                continue;

            switch( rs->GetResultType() ) {
            case eDB_ParamResult:
                while( rs->Next() ) {
                    NcbiCout << "Output param: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_RowResult:
                while( rs->Next() ) {
                    if( rs->GetVariant(1).GetInt4() == 2121 ) {
                            NcbiCout << rs->GetVariant(2).GetString() << " "
                                     << rs->GetVariant(3).GetString() << " "
                                     << rs->GetVariant(4).GetString() << " "
                                     << rs->GetVariant(5).GetString() << " "
                                     << rs->GetVariant(6).GetString() << " "
                                     << rs->GetVariant(7).GetString() << " "
                                     << endl;
                    }
                    else {
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << endl;
                    }
                }
                break;
            }
        }
        NcbiCout << "Status : " << cstmt->GetReturnStatus() << endl;

       
        delete cstmt;

        // call stored procedure using language call
        NcbiCout << "Calling stored procedure using language call..." << endl;
        
        sql = "exec SampleProc @id, @f, @o output";
        stmt->SetParam(CVariant(5), "@id");
        stmt->SetParam(CVariant::Float(&f), "@f");
        stmt->SetParam(CVariant(5), "@o");
        stmt->Execute(sql); 
    
        while(stmt->HasMoreResults()) {
            IResultSet *rs = stmt->GetResultSet();

            //NcbiCout << "Row count: " << stmt->GetRowCount() << endl;

            if( rs == 0 )
                continue;

            switch( rs->GetResultType() ) {
            case eDB_ParamResult:
                while( rs->Next() ) {
                    NcbiCout << "Output param: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_StatusResult:
                while( rs->Next() ) {
                    NcbiCout << "Return status: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_RowResult:
                while( rs->Next() ) {
                    if( rs->GetVariant(1).GetInt4() == 2121 ) {
                            NcbiCout << rs->GetVariant(2).GetString() << "|"
                                     << rs->GetVariant(3).GetString() << "|"
                                     << rs->GetVariant(4).GetString() << "|"
                                     << rs->GetVariant(5).GetString() << "|"
                                     << rs->GetVariant(6).GetString() << "|"
                                     << rs->GetVariant(7).GetString() << "|"
                                     << endl;
                    }
                    else {
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << endl;
                    }
                }
                break;
            }
        }

        stmt->ClearParamList();
        //exit(1);

        // Reconnect
        NcbiCout << "Reconnecting..." << endl;

        delete conn;
        conn = ds->CreateConnection();

        conn->SetMode(IConnection::eBulkInsert);

        conn->Connect("anyone",
                      "allowed",
                      server,
                      "DBAPI_Sample");


        stmt = conn->CreateStatement();

        NcbiCout << "Reading BLOB..." << endl;

        // Read blob to vector
        vector<char> blob;

 
        NcbiCout << "Retrieve BLOBs using streams" << endl;

        stmt->ExecuteUpdate("set textsize 2000000");
    
        stmt->Execute("select str_val, text_val, text_val \
from SelectSample where int_val = 1");
    
        while( stmt->HasMoreResults() ) { 
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                int size = 0;
                while(rs->Next()) { 
                    NcbiCout << "Reading: " << rs->GetVariant(1).GetString() 
                             << endl;
                    istream& in1 = rs->GetBlobIStream();
                    int c = 0; 
                    NcbiCout << "Reading first blob..." << endl;
                    for( ;(c = in1.get()) != CT_EOF; ++size ) {
                        blob.push_back(c);
                    }
                    istream& in2 = rs->GetBlobIStream();
                    NcbiCout << "Reading second blob..." << endl;
                    
                    for( ;(c = in2.get()) != CT_EOF; ++size ) { 
                        blob.push_back(c);
                    }
                } 
                NcbiCout << "Bytes read: " << size << endl;
            }
        }

    
        // create a table
        NcbiCout << "Creating BlobSample table..." << endl;
        sql = "if exists( select * from sysobjects \
where name = 'BlobSample' \
AND type = 'U') \
begin \
	drop table BlobSample \
end";
        stmt->ExecuteUpdate(sql);


        sql = "create table BlobSample (\
	id int null, \
	blob text null, unique (id))";
        stmt->ExecuteUpdate(sql);

        // Write BLOB several times
        const int COUNT = 5;
        int cnt = 0;

        // Create alternate blob storage
        char *buf = new char[blob.size()];
        char *p = buf;
        vector<char>::iterator i_b = blob.begin();
        for( ; i_b != blob.end(); ++i_b ) {
            *p++ = *i_b;
        }

        //Initialize table using bulk insert
        NcbiCout << "Initializing BlobSample table..." << endl;
        IBulkInsert *bi = conn->CreateBulkInsert("BlobSample", 2);
        CVariant col1 = CVariant(eDB_Int);
        CVariant col2 = CVariant(eDB_Text);
        bi->Bind(1, &col1);
        bi->Bind(2, &col2);
        for(int i = 0; i < COUNT; ++i ) {
            string im = "BLOB data " + NStr::IntToString(i);
            col1 = i;
            col2.Truncate();
            col2.Append(im.c_str(), im.size());
            bi->AddRow();
        }
        bi->Complete();

        

        NcbiCout << "Writing BLOB using cursor..." << endl;

        ICursor *blobCur = conn->CreateCursor("test", 
           "select id, blob from BlobSample for update of blob");
    
        IResultSet *blobRs = blobCur->Open();
        while(blobRs->Next()) {
                NcbiCout << "Writing BLOB " << ++cnt << endl;
                ostream& out = blobCur->GetBlobOStream(2, blob.size(), eDisableLog);
                out.write(buf, blob.size());
                out.flush();
        }
     
        blobCur->Close();

#ifndef WIN32 // Not supported by ODBC driver
        NcbiCout << "Writing BLOB using resultset..." << endl;

        stmt->Execute("select id, blob from BlobSample at isolation read uncommitted");
    
        cnt = 0;
        while( stmt->HasMoreResults() ) {
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                while(rs->Next()) {
                    NcbiCout << "Writing BLOB " << ++cnt << endl;
                    ostream& out = rs->GetBlobOStream(blob.size(), eDisableLog);
                    out.write(buf, blob.size());
                    out.flush();
                }
            }
        }

#endif

#if 0
        while( stmt->HasMoreResults() ) {
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                while(rs->Next()) {
                    ostream& out = rs->GetBlobOStream(blob.size());
                    vector<char>::iterator i = blob.begin();
                    for( ; i != blob.end(); ++i )
                        out << *i;
                    out.flush();
                }
            }
        }
#endif
        delete buf;

        // check if Blob is there
        NcbiCout << "Checking BLOB size..." << endl;
        stmt->Execute("select 'Written blob size' as size, datalength(blob) \
from BlobSample where id = 1");
    
        while( stmt->HasMoreResults() ) {
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                while(rs->Next()) {
                    NcbiCout << rs->GetVariant(1).GetString() << ": " 
                             << rs->GetVariant(2).GetInt4() << endl;
                }
            }
        }

        // Cursor test (remove blob)
        NcbiCout << "Cursor test, removing blobs" << endl;

        ICursor *cur = conn->CreateCursor("test", 
                                          "select id, blob from BlobSample for update of blob");
    
        IResultSet *rs = cur->Open();
        while(rs->Next()) {
            cur->Update("BlobSample", "update BlobSample set blob = ' '");
        }
     
        cur->Close();

        // drop BlobSample table
        NcbiCout << "Deleting BlobSample table..." << endl;
        sql = "drop table BlobSample";
        stmt->ExecuteUpdate(sql);
        NcbiCout << "Done." << endl;


    }
    catch(out_of_range) {
        NcbiCout << "Exception: Out of range" << endl;
    }
    catch(exception& e) {
        NcbiCout << "Exception: " << e.what() << endl;
        NcbiCout << ds->GetErrorInfo();
    }

    return 0;
}

void CDbapiTest::Exit()
{

}


int main(int argc, const char* argv[])
{
    return CDbapiTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2003/03/06 16:17:21  kholodov
* Fixed: incorrect TDS version for STRAUSS
*
* Revision 1.7  2002/10/23 13:32:59  kholodov
* Fixed: extra exit() call removed
*
* Revision 1.6  2002/10/03 18:59:55  kholodov
* *** empty log message ***
*
* Revision 1.5  2002/10/02 15:06:58  kholodov
* Modified: default connection settings for UNIX and NT platforms
*
* Revision 1.4  2002/10/01 18:57:45  kholodov
* Added: bulk insert test
*
* Revision 1.3  2002/09/19 14:38:49  kholodov
* Modified: to work with ODBC driver
*
* Revision 1.2  2002/09/17 21:17:15  kholodov
* Filed moved to new location
*
* Revision 1.5  2002/09/16 21:08:33  kholodov
* Added: bulk insert operations
*
* Revision 1.4  2002/09/03 18:41:53  kholodov
* Added multiple BLOB updates, use of IResultSetMetaData
*
* Revision 1.3  2002/07/12 19:23:03  kholodov
* Added: update BLOB using cursor for all platforms
*
* Revision 1.2  2002/07/08 16:09:24  kholodov
* Added: BLOB update thru cursor
*
* Revision 1.1  2002/07/02 13:48:30  kholodov
* First commit
*
*
*/
