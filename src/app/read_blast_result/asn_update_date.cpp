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
* Author: Azat Badretdin
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <string>
#include <algorithm>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

enum ECoreDataType   
     {
     eUndefined = 0,
     eSubmit,
     eEntry,
     };


class CReadBlastApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

private:

// Main functions
// polymorphic wrappers around core data
    CBeginInfo      Begin(void);
    CConstBeginInfo ConstBegin(void);   
// input tools

    static ECoreDataType getCoreDataType(istream& in);
    bool IsSubmit();
    bool IsEntry ();
    bool IsTbl   ();
    void printGeneralInfo(ostream&);

// output tools

// member vars
    // Member variable to help illustrate our naming conventions
    CSeq_submit m_Submit;
    CSeq_entry  m_Entry;
    ECoreDataType   m_coreDataType;

   
};

void CReadBlastApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Reads seq-submit file and prints the date");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey("infmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("infmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());


// core data type
    m_coreDataType = eUndefined;

}


static ESerialDataFormat s_GetFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else {
        // Should be caught by argument processing, but as an illustration...
        THROW1_TRACE(runtime_error, "Bad serial format name " + name);
    }
}

int CReadBlastApp::Run(void)
{
// Get arguments
    CArgs args = GetArgs();


// output control
    string base = args["in"].AsString();

    // Read original annotation
    {{
        m_coreDataType = getCoreDataType(args["in"].AsInputFile());
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(s_GetFormat(args["infmt"].AsString()),
                                   args["in"].AsInputFile()));
        if(IsSubmit())
          *in >> m_Submit;
        else if(IsEntry() )
          *in >> m_Entry; 
        else
          {
          NcbiCerr << "FATAL: only tbl, Seq-submit or Seq-entry formats are accepted at this time" << NcbiEndl;
          throw;
          }
         
    }}


    printGeneralInfo(cout);
    
    // Exit successfully
    return 0;
}

void CReadBlastApp::printGeneralInfo(ostream& out)
{
  if ( IsEntry () && m_Entry.GetSet().CanGetDescr() )
    {
    ITERATE(CSeq_descr::Tdata, descr, m_Entry.GetSet().GetDescr().Get())
      {
      if((*descr)->IsCreate_date())
        {
        string label; 
        (*descr)->GetCreate_date().GetDate(&label);
        out << "Create date: " << label << NcbiEndl;
        }
      else if ((*descr)->IsUpdate_date())
        { 
        string label;
        (*descr)->GetUpdate_date().GetDate(&label);
        time_t  utime=0;
        if((*descr)->GetUpdate_date().IsStd())
          {
          int year=0;
          if((*descr)->GetUpdate_date().GetStd().CanGetYear())
             year=(*descr)->GetUpdate_date().GetStd().GetYear();
          int month=1;
          if((*descr)->GetUpdate_date().GetStd().CanGetMonth())
             month=(*descr)->GetUpdate_date().GetStd().GetMonth();
          int day=1;
          if((*descr)->GetUpdate_date().GetStd().CanGetDay())
             day=(*descr)->GetUpdate_date().GetStd().GetDay();
          struct tm utm = {0,0,0,day,month-1, year-1900, 0, 0, -1};
          utime = mktime(&utm);
          }
        out << "Update date: " << utime << " " << label << NcbiEndl;
        }
      }
    }

}




ECoreDataType CReadBlastApp::getCoreDataType(istream& in)
{
  char buffer[1024];
  in.getline(buffer, 1024);
  in.seekg(0);

  string result =  buffer;
  result.erase(result.find_first_of(": "));

  if(result=="Seq-entry") 
    {
    result =  buffer;
    if(result.find("set") != string::npos)
      return eEntry;
    return eUndefined;
    }
  if(result=="Seq-submit") return eSubmit;
  return eUndefined;
}

bool CReadBlastApp::IsSubmit()
{
  return m_coreDataType == eSubmit;
}

bool CReadBlastApp::IsEntry()
{
  return m_coreDataType == eEntry;
}


CBeginInfo CReadBlastApp::Begin(void)
{
  if(m_coreDataType == eSubmit) return ::Begin(m_Submit);
  if(m_coreDataType == eEntry)  return ::Begin(m_Entry);

  return ::Begin(m_Submit); // some platforms might have warnings without it
}

CConstBeginInfo CReadBlastApp::ConstBegin(void)
{
  if(m_coreDataType == eSubmit) return ::ConstBegin(m_Submit);
  if(m_coreDataType == eEntry)  return ::ConstBegin(m_Entry);

  return ::ConstBegin(m_Submit); // some platforms might have warnings without it
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CReadBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


