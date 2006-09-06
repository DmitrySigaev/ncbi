/* 
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
 *  Please cite the authors in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Lewis Y. Geer
 *  
 * File Description:
 *    program for splitting omssa files
 *
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrxml.hpp>
#include <objects/omssa/omssa__.hpp>

#include "msmerge.hpp"

#include <fstream>
#include <string>
#include <list>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSAMerge
//
//  Main application
//

class COMSSAMerge : public CNcbiApplication {
public:
    COMSSAMerge();
private:
    virtual int Run();
    virtual void Init();

};

COMSSAMerge::COMSSAMerge()
{
    SetVersion(CVersionInfo(1, 0, 1));
}





void COMSSAMerge::Init()
{

    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);

    argDesc->AddDefaultKey("i", "infiles", 
                "file containing list of input files on separate lines",
                CArgDescriptions::eString,
                "");

    argDesc->AddFlag("sw", "output search results without spectra");

    argDesc->AddFlag("it", "input as text asn.1 formatted search results");
    argDesc->AddFlag("ib", "input as binary asn.1 formatted search results");
    argDesc->AddFlag("ix", "input as xml formatted search results");

    argDesc->AddPositional("o", "output file name", CArgDescriptions::eString);


    argDesc->AddFlag("ot", "output as text asn.1 formatted search results");
    argDesc->AddFlag("ob", "output as binary asn.1 formatted search results");
    argDesc->AddFlag("ox", "output as xml formatted search results");

    argDesc->AddExtra(0,10000, "input file names", CArgDescriptions::eString);


    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    COMSSAMerge theTestApp;
    return theTestApp.AppMain(argc, argv, 0, eDS_Default, 0);
}




int COMSSAMerge::Run()
{    

    try {

	CArgs args = GetArgs();


    CRef <COMSSASearch> MySearch(new COMSSASearch);

    ESerialDataFormat InFileType, OutFileType;

    if(args["ox"]) OutFileType = eSerial_Xml;
    else if(args["ob"]) OutFileType = eSerial_AsnBinary;
    else if(args["ot"]) OutFileType = eSerial_AsnText;
    else ERR_POST(Fatal << "output file type not given");

    if(args["ix"]) InFileType = eSerial_Xml;
    else if(args["ib"]) InFileType = eSerial_AsnBinary;
    else if(args["it"]) InFileType = eSerial_AsnText;
    else ERR_POST(Fatal << "input file type not given");


    // loop thru input files
    if ( args["i"].AsString() != "") {
        ifstream is(args["i"].AsString().c_str());
        bool Begin(true);
        if(!is)
            ERR_POST(Fatal << "unable to open input file list " << args["i"].AsString());
        while(!is.eof()) {
            string iFileName;
            NcbiGetline(is, iFileName, "\x0d\x0a");
            if(iFileName == "" || is.eof()) continue;
            try {
                CRef <COMSSASearch> InSearch(new COMSSASearch);
                InSearch->ReadCompleteSearch(iFileName, InFileType);
                if(Begin) {
                    Begin = false;
                    MySearch->CopyCMSSearch(InSearch);
                }
                else {
                    // add
                    MySearch->AppendSearch(InSearch);
                }
            }
            catch(CException& e) {
                ERR_POST(Fatal << "exception: " << e.what());
                return 1;
            }
        }
    }
    else if ( args.GetNExtra() ) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            CRef <COMSSASearch> InSearch(new COMSSASearch);
            InSearch->ReadCompleteSearch(args[extra].AsString(), InFileType);
            try {
                if(extra == 1) {
                    // copy
                    MySearch->CopyCMSSearch(InSearch);
                }
                else {
                    // add
                    MySearch->AppendSearch(InSearch);
                }
            }
            catch(CException& e) {
                ERR_POST(Fatal << "exception: " << e.what());
                return 1;
            }
        }
    }
 
    // write out the new search
    auto_ptr <CObjectOStream> txt_out(
         CObjectOStream::Open(args["o"].AsString(), OutFileType));

    if(txt_out.get()) {
        SetUpOutputFile(txt_out.get(), OutFileType);
        if (args["sw"]) {
            txt_out->Write(ObjectInfo(*(*MySearch->SetResponse().begin())));
	}
        else {
            txt_out->Write(ObjectInfo(*MySearch));
        }
        txt_out->Flush();
        txt_out->Close();
    }


    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Fatal << "Exception in COMSSAMerge::Run: " << e.what());
    }

    return 0;
}

