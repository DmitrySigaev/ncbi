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
* Author: Eugene Vasilchenko
*
* File Description:
*   asn2asn test program
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/01/10 14:17:47  vasilche
* Fixed usage of different types in ?: statement.
*
* Revision 1.2  2000/01/06 21:28:04  vasilche
* Fixed for variable scope.
*
* Revision 1.1  2000/01/05 19:46:58  vasilche
* Added asn2asn test application.
*
* ===========================================================================
*/

#include "asn2asn.hpp"
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiutil.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/choiceptr.hpp>
#include <serial/serial.hpp>

class CNcbiDiagStream
{
public:
    CNcbiDiagStream(CNcbiOstream* afterClose = 0)
        : m_AfterClose(afterClose)
        {
            SetDiagStream(afterClose);
        }
    ~CNcbiDiagStream(void)
        {
            Reset();
        }

    void Reset(void)
        {
            SetDiagStream(m_AfterClose);
            m_CloseStream.reset(0);
        }
    void AssignTo(CNcbiOstream* out, bool deleteOnClose = false)
        {
            SetDiagStream(out);
            if ( deleteOnClose )
                m_CloseStream.reset(out);
        }
    void AssignToNull(void)
        {
            AssignTo(0, true);
        }
    bool AssignToFile(const string& fileName)
        {
            CNcbiOstream* out = new CNcbiOfstream(fileName.c_str());
            if ( !*out ) {
                delete out;
                return false;
            }
            AssignTo(out, true);
            return true;
        }

private:
    auto_ptr<CNcbiOstream> m_CloseStream;
    CNcbiOstream* m_AfterClose;
};

int main(int argc, char** argv)
{
    return CAsn2Asn().AppMain(argc, argv);
}

static
void PrintUsage(void)
{
    NcbiCout <<
        "Arguments:\n" <<
        "  -i  Filename for asn.1 input\n" <<
        "  -e  Input is a Seq-entry\n" <<
        "  -b  Input asnfile in binary mode\n" <<
        "  -o  Filename for asn.1 output\n" <<
        "  -s  Output asnfile in binary mode\n" <<
        "  -l  Log errors to file named\n";
    exit(1);
}

static
void InvalidArgument(const char* arg)
{
    ERR_POST(Error << "Invalid argument: " << arg);
    PrintUsage();
}

static
const char* StringArgument(const char* arg)
{
    if ( !arg || !*arg )
        InvalidArgument("Argument required");
    return arg;
}

static
void SeqEntryProcess(Seq_entry* sep);  /* dummy function */


/*****************************************************************************
*
*   Main program loop to read, process, write SeqEntrys
*
*****************************************************************************/
int CAsn2Asn::Run(void)
{
    string inFile;
    bool inSeqEntry = false;
    bool inBinary = false;
    string outFile;
    bool outBinary = false;
    string logFile;
    CNcbiDiagStream logStream(&NcbiCerr);

    if ( m_Argc == 1 )
        PrintUsage();

    {
        for ( int i = 1; i < m_Argc; ++i ) {
            const char* arg = m_Argv[i];
            if ( arg[0] == '-' ) {
                switch ( arg[1] ) {
                case 'i':
                    inFile = StringArgument(m_Argv[++i]);
                    break;
                case 'e':
                    inSeqEntry = true;
                    break;
                case 'b':
                    inBinary = true;
                    break;
                case 'o':
                    outFile = StringArgument(m_Argv[++i]);
                    break;
                case 's':
                    outBinary = true;
                    break;
                case 'l':
                    logFile = StringArgument(m_Argv[++i]);
                    break;
                default:
                    InvalidArgument(arg);
                    break;
                }
            }
            else {
                InvalidArgument(arg);
            }
        }
    }

    if ( !logFile.empty() ) {
        if ( logFile == "stderr" || logFile == "-" ) {
            logStream.AssignTo(&NcbiCerr);
        }
        else if ( logFile == "null" ) {
            logStream.AssignToNull();
        }
        else if ( !logStream.AssignToFile(logFile) ) {
            ERR_POST(Fatal << "Cannot open log file: " << logFile);
        }
    }

    auto_ptr<CNcbiIstream> inFStream;
    CNcbiIstream* inStream;
    if ( inFile.empty() || inFile == "stdin" ) {
        inStream = &NcbiCin;
    }
    else {
        inFStream.reset(inStream = new CNcbiIfstream(inFile.c_str()));
        if ( !*inStream )
            ERR_POST(Fatal << "Cannot open file: " << inFile);
    }
    auto_ptr<CObjectIStream> inObject;
    if ( inBinary )
        inObject.reset(new CObjectIStreamAsnBinary(*inStream));
    else
        inObject.reset(new CObjectIStreamAsn(*inStream));

    auto_ptr<CNcbiOstream> outFStream;
    CNcbiOstream* outStream;
    if ( outFile.empty() ) {
        outStream = 0;
    }
    else if ( outFile == "stdout" || outFile == "-" ) {
        outStream = &NcbiCout;
    }
    else {
        outFStream.reset(outStream = new CNcbiOfstream(outFile.c_str()));
        if ( !*outStream )
            ERR_POST(Fatal << "Cannot open file: " << outFile);
    }
    auto_ptr<CObjectOStream> outObject;
    if ( outStream ) {
        if ( outBinary )
            outObject.reset(new CObjectOStreamAsnBinary(*outStream));
        else
            outObject.reset(new CObjectOStreamAsn(*outStream));
    }

    if ( inSeqEntry ) { /* read one Seq-entry */
        Seq_entry* entry = 0;
        TTypeInfo typeInfo =
            CChoicePointerTypeInfo::GetTypeInfo(Seq_entry::GetTypeInfo());
        inObject->Read(&entry, typeInfo);
        SeqEntryProcess(entry);     /* do any processing */
        if ( outObject.get() )
            outObject->Write(&entry, typeInfo);
	}
	else {              /* read Seq-entry's from a Bioseq-set */
        Bioseq_set entries;
        *inObject >> entries;
        iterate ( Bioseq_set::TSeqEntries, i, entries.GetSeq_set() ) {
            SeqEntryProcess(*i);     /* do any processing */
        }
        if ( outObject.get() )
            *outObject << entries;
	}

    inObject.reset(0);
    outObject.reset(0);

	return 0;
}


/*****************************************************************************
*
*   void SeqEntryProcess (sep)
*      just a dummy routine that does nothing
*
*****************************************************************************/
static
void SeqEntryProcess (Seq_entry* sep)
{
	if (sep == NULL)
		return;
	return;
}

