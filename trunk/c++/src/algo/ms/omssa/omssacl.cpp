/* $Id$
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
 *    command line OMSSA search
 *
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>
#include <objects/omssa/omssa__.hpp>
#include <serial/objostrxml.hpp>

#include <fstream>
#include <string>
#include <list>
#include <stdio.h>

#include "omssa.hpp"
#include "SpectrumSet.hpp"
#include "Mod.hpp"
#include <readdb.h>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  COMSSA
//
//  Main application
//
typedef list <string> TStringList;

class COMSSA : public CNcbiApplication {
public:
    COMSSA();
private:
    virtual int Run();
    virtual void Init();
    void PrintMods(void);
    void PrintEnzymes(void);
    void PrintIons(void);
    template <class T> void InsertList(const string& List, T& ToInsert, string error); 
};

COMSSA::COMSSA()
{
    SetVersion(CVersionInfo(0, 9, 5));
}


template <class T> void COMSSA::InsertList(const string& Input, T& ToInsert, string error) 
{
    TStringList List;
    NStr::Split(Input, ",", List);

    TStringList::iterator iList(List.begin());
    int Num;

    for(;iList != List.end(); iList++) {
	try {
	    Num = NStr::StringToInt(*iList);
	} catch (CStringException &e) {
	    ERR_POST(Info << error);
	    continue;
	}
	ToInsert.push_back(Num);
    }
}


///
/// print out a list of modification that can be used in OMSSA
///
void COMSSA::PrintMods(void)
{
    int i;
    for(i = 0; i < eMSMod_max; i++)
	cout << i << ": " << kModNames[i] << endl;
}


///
/// print out a list of enzymes that can be used in OMSSA
///
void COMSSA::PrintEnzymes(void)
{
    int i;
    for(i = 0; i < eMSEnzymes_max; i++)
	cout << i << ": " << kEnzymeNames[i] << endl;
}

///
/// print out a list of ions that can be used in OMSSA
///
void COMSSA::PrintIons(void)
{
    int i;
    for(i = 0; i < eMSIonType_max; i++)
	cout << i << ": " << kIonLabels[i] << endl;
}


void COMSSA::Init()
{

    auto_ptr<CArgDescriptions> argDesc(new CArgDescriptions);

    argDesc->AddDefaultKey("d", "blastdb", "Blast sequence library to search. Do not include .p* filename suffixes.",
			   CArgDescriptions::eString, "nr");
    argDesc->AddDefaultKey("f", "infile", "single dta file to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fx", "xmlinfile", "multiple xml-encapsulated dta files to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fb", "dtainfile", "multiple dta files separated by blank lines to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("o", "textasnoutfile", "filename for text asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ob", "binaryasnoutfile", "filename for binary asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ox", "xmloutfile", "filename for xml formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddFlag("w", "include spectra and search params in search results");

#if 0
    argDesc->AddDefaultKey("wi", "textasninfile", "filename to _write_ text asn.1 formatted search input",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("wib", "binaryasninfile", "filename to _write_ binary asn.1 formatted search input",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("wix", "xmlinfile", "filename to _write_ xml formatted search input",
			   CArgDescriptions::eString, "");
#endif
#if MSGRAPH
    argDesc->AddDefaultKey("g", "graph", "graph file to write out",
			   CArgDescriptions::eString, "");
#endif
    argDesc->AddDefaultKey("to", "pretol", "product ion mass tolerance in Da",
			   CArgDescriptions::eDouble, "0.8");
    argDesc->AddDefaultKey("te", "protol", "precursor ion  mass tolerance in Da",
			   CArgDescriptions::eDouble, "2.0");
    argDesc->AddDefaultKey("tom", "premass", "product ion search type (0 = mono, 1 = avg)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tem", "promass", "precursor ion search type (0 = mono, 1 = avg)",
                CArgDescriptions::eInteger, "0");

    argDesc->AddDefaultKey("i", "ions", 
               "id numbers of ions to search (comma delimited, no spaces)",
               CArgDescriptions::eString, 
               "1,4");
    argDesc->AddFlag("il", "print a list of ions and their corresponding id number");


    argDesc->AddDefaultKey("cl", "cutlo", "low intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0");
    argDesc->AddDefaultKey("ch", "cuthi", "high intensity cutoff as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.2");
    argDesc->AddDefaultKey("ci", "cutinc", "intensity cutoff increment as a fraction of max peak",
			   CArgDescriptions::eDouble, "0.0005");
    //    argDesc->AddDefaultKey("u", "cull", 
    //			   "number of peaks to leave in each 100Da bin",
    //			   CArgDescriptions::eInteger, "3");
    argDesc->AddDefaultKey("v", "cleave", 
			   "number of missed cleavages allowed",
			   CArgDescriptions::eInteger, "1");
    //    argDesc->AddDefaultKey("n", "numseq", 
    //			   "number of sequences to search (0 = all)",
    //			   CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("x", "taxid", 
			   "comma delimited list of taxids to search (0 = all)",
			   CArgDescriptions::eString, "0");
    argDesc->AddDefaultKey("w1", "window1", 
			   "single charge window in Da",
			   CArgDescriptions::eInteger, "20");
    argDesc->AddDefaultKey("w2", "window2", 
			   "double charge window in Da",
			   CArgDescriptions::eInteger, "14");
    argDesc->AddDefaultKey("h1", "hit1", 
			   "number of peaks allowed in single charge window",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("h2", "hit2", 
			   "number of peaks allowed in double charge window",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("hl", "hitlist", 
			   "maximum number of hits retained for one spectrum",
			   CArgDescriptions::eInteger, "30");
    argDesc->AddDefaultKey("ht", "tophitnum", 
			   "number of m/z values corresponding to the most intense peaks that must include one match to the theoretical peptide",
			   CArgDescriptions::eInteger, "3");
    argDesc->AddDefaultKey("hm", "minhit", 
			   "the minimum number of m/z matches a sequence library peptide must have for the hit to the peptide to be recorded",
			   CArgDescriptions::eInteger, "2");
    argDesc->AddDefaultKey("hs", "minspectra", 
			   "the minimum number of m/z values a spectrum must have to be searched",
			   CArgDescriptions::eInteger, "4");
    argDesc->AddDefaultKey("he", "evalcut", 
			   "the maximum evalue allowed in the hit list",
			   CArgDescriptions::eDouble, "1");
    argDesc->AddDefaultKey("mf", "fixedmod", 
			   "comma delimited (no spaces) list of id numbers for fixed modifications",
			   CArgDescriptions::eString,
			   "");
    argDesc->AddDefaultKey("mv", "variablemod", 
			   "comma delimited (no spaces) list of id numbers for variable modifications",
			   CArgDescriptions::eString,
			   "");
    argDesc->AddFlag("ml", "print a list of modifications and their corresponding id number");
    argDesc->AddDefaultKey("mm", "maxmod", 
			   "the maximum number of mass ladders to generate per database peptide",
			   CArgDescriptions::eInteger, NStr::IntToString(MAXMOD2));
    argDesc->AddDefaultKey("e", "enzyme", 
			   "id number of enzyme to use",
			   CArgDescriptions::eInteger, 
			   NStr::IntToString(eMSEnzymes_trypsin));
    argDesc->AddFlag("el", "print a list of enzymes and their corresponding id number");
	argDesc->AddDefaultKey("zh", "maxcharge", 
				"maximum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"3");
	argDesc->AddDefaultKey("zl", "mincharge", 
				"minimum precursor charge to search when not 1+",
				CArgDescriptions::eInteger, 
				"2");
	argDesc->AddDefaultKey("zt", "chargethresh", 
				 "minimum precursor charge to start considering multiply charged products",
				 CArgDescriptions::eInteger, 
				 "3");


    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    COMSSA theTestApp;
    return theTestApp.AppMain(argc, argv);
}


int COMSSA::Run()
{    

    try {

	CArgs args = GetArgs();

	// print out the modification list
	if(args["ml"]) {
	    PrintMods();
	    return 0;
	}

	// print out the enzymes list
	if(args["el"]) {
	    PrintEnzymes();
	    return 0;
	}

    // print out the ions list
     if(args["il"]) {
         PrintIons();
         return 0;
     }

	_TRACE("omssa: initializing score");
	CSearch Search;
	int retval = Search.InitBlast(args["d"].AsString().c_str(), false);
	if(retval) {
	    ERR_POST(Fatal << "ommsacl: unable to initialize blastdb, error " 
		     << retval);
	    return 1;
	}
	_TRACE("ommsa: score initialized");
	CRef <CSpectrumSet> Spectrumset(new CSpectrumSet);

    int FileRetVal(1);
	if(args["fx"].AsString().size() != 0) {
	    ifstream PeakFile(args["fx"].AsString().c_str());
	    if(!PeakFile) {
		ERR_POST(Fatal <<" omssacl: not able to open spectrum file " <<
			 args["fx"].AsString());
		return 1;
	    }
	    FileRetVal = Spectrumset->LoadMultDTA(PeakFile);
	}
	else if(args["f"].AsString().size() != 0) {
	    ifstream PeakFile(args["f"].AsString().c_str());
	    if(!PeakFile) {
		ERR_POST(Fatal << "omssacl: not able to open spectrum file " <<
			 args["f"].AsString());
		return 1;
	    }
	    FileRetVal = Spectrumset->LoadDTA(PeakFile);
	}
	else if(args["fb"].AsString().size() != 0) {
	    ifstream PeakFile(args["fb"].AsString().c_str());
	    if(!PeakFile) {
		ERR_POST(Fatal << "omssacl: not able to open spectrum file " <<
			 args["fb"].AsString());
		return 1;
	    }
	    FileRetVal = Spectrumset->LoadMultBlankLineDTA(PeakFile);
	}
	else {
	    ERR_POST(Fatal << "omssatest: input file not given.");
	    return 1;
	}

    if(FileRetVal == -1) {
        ERR_POST(Fatal << "omssacl: too many spectra in input file");
        return 1;
    }
    else if(FileRetVal == 1) {
        ERR_POST(Fatal << "omssacl: unable to read spectrum file -- incorrect file type?");
        return 1;
    }


	CMSResponse Response;
	CMSRequest Request;
	Request.SetSettings().SetPrecursorsearchtype(args["tem"].AsInteger());
	Request.SetSettings().SetProductsearchtype(args["tom"].AsInteger());
	Request.SetSettings().SetPeptol(args["te"].AsDouble());
	Request.SetSettings().SetMsmstol(args["to"].AsDouble());
	InsertList(args["i"].AsString(), Request.SetSettings().SetIonstosearch(), "unknown ion");
	Request.SetSettings().SetCutlo(args["cl"].AsDouble());
	Request.SetSettings().SetCuthi(args["ch"].AsDouble());
	Request.SetSettings().SetCutinc(args["ci"].AsDouble());
	Request.SetSettings().SetSinglewin(args["w1"].AsInteger());
	Request.SetSettings().SetDoublewin(args["w2"].AsInteger());
	Request.SetSettings().SetSinglenum(args["h1"].AsInteger());
	Request.SetSettings().SetDoublenum(args["h2"].AsInteger());
	Request.SetSettings().SetEnzyme(args["e"].AsInteger());
	Request.SetSettings().SetMissedcleave(args["v"].AsInteger());
	Request.SetSpectra(*Spectrumset);
	InsertList(args["mv"].AsString(), Request.SetSettings().SetVariable(), "unknown variable mod");
	InsertList(args["mf"].AsString(), Request.SetSettings().SetFixed(), "unknown fixed mod");
	Request.SetSettings().SetDb(args["d"].AsString());
	//	Request.SetCull(args["u"].AsInteger());
	Request.SetSettings().SetHitlistlen(args["hl"].AsInteger());
	Request.SetSettings().SetTophitnum(args["ht"].AsInteger());
	Request.SetSettings().SetMinhit(args["hm"].AsInteger());
	Request.SetSettings().SetMinspectra(args["hs"].AsInteger());
	Request.SetSettings().SetCutoff(args["he"].AsDouble());
	Request.SetSettings().SetMaxmods(args["mm"].AsInteger());

	if(args["x"].AsString() != "0") {
	    InsertList(args["x"].AsString(), Request.SetSettings().SetTaxids(), "unknown tax id");
	}

	Request.SetSettings().SetChargehandling().SetConsidermult(args["zt"].AsInteger());
	Request.SetSettings().SetChargehandling().SetMincharge(args["zl"].AsInteger());
	Request.SetSettings().SetChargehandling().SetMaxcharge(args["zh"].AsInteger());

	// validate the input
        list <string> ValidError;
	if(Request.GetSettings().Validate(ValidError) != 0) {
	    list <string>::iterator iErr;
	    for(iErr = ValidError.begin(); iErr != ValidError.end(); iErr++)
		ERR_POST(Warning << *iErr);
	    ERR_POST(Fatal << "Unable to validate settings");
	}


	_TRACE("omssa: search begin");
	Search.Search(Request, Response);
	_TRACE("omssa: search end");


#if _DEBUG
	// read out hits
	CMSResponse::THitsets::const_iterator iHits;
	iHits = Response.GetHitsets().begin();
	for(; iHits != Response.GetHitsets().end(); iHits++) {
	    CRef< CMSHitSet > HitSet =  *iHits;
	    ERR_POST(Info << "Hitset: " << HitSet->GetNumber());
	    if( HitSet-> CanGetError() && HitSet->GetError() ==
		eMSHitError_notenuffpeaks) {
		ERR_POST(Info << "Hitset Empty");
		continue;
	    }
	    CRef< CMSHits > Hit;
	    CMSHitSet::THits::const_iterator iHit; 
	    CMSHits::TPephits::const_iterator iPephit;
	    for(iHit = HitSet->GetHits().begin();
		iHit != HitSet->GetHits().end(); iHit++) {
		ERR_POST(Info << (*iHit)->GetPepstring() << ": " << "P = " << 
			 (*iHit)->GetPvalue() << " E = " <<
			 (*iHit)->GetEvalue());    
		for(iPephit = (*iHit)->GetPephits().begin();
		    iPephit != (*iHit)->GetPephits().end();
		    iPephit++) {
		    ERR_POST(Info << (*iPephit)->GetGi() << ": " << (*iPephit)->GetStart() << "-" << (*iPephit)->GetStop() << ":" << (*iPephit)->GetDefline());
		}
    
	    }
	}
#endif

	// Check to see if there is a hitset

	if(!Response.CanGetHitsets()) {
	  ERR_POST(Fatal << "No results found");
	}

	// output
    auto_ptr <CObjectOStream> txt_out;  
    CNcbiOfstream os;

	if(args["o"].AsString() != "") {
		txt_out.reset(CObjectOStream::Open(args["o"].AsString().c_str(),
					     eSerial_AsnText));
	}
	else if(args["ob"].AsString() != "") {
		txt_out.reset(CObjectOStream::Open(args["ob"].AsString().c_str(),
					     eSerial_AsnBinary));
	}
	else if(args["ox"].AsString() != "") {
        txt_out.reset(CObjectOStream::Open(args["ox"].AsString().c_str(), eSerial_Xml));
        // turn on xml schema
        CObjectOStreamXml *xml_out = dynamic_cast <CObjectOStreamXml *> (txt_out.get());
        xml_out->SetReferenceSchema();
	}

    if(txt_out.get() != 0) {
        if(args["w"]) {
            // make complex object
            CMSSearch MySearch;
            CRef<CMSRequest> requestref(&Request);
            MySearch.SetRequest().push_back(requestref);
            CRef<CMSResponse> responseref(&Response);
            MySearch.SetResponse().push_back(responseref);
            // write out
            txt_out->Write(ObjectInfo(MySearch));
            requestref.Release();
            responseref.Release();
            MySearch.SetRequest().begin()->Release();
            MySearch.SetResponse().begin()->Release();
        }
        else {
            txt_out->Write(ObjectInfo(Response));
        }
    }


    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Fatal << "Exception in COMSSA::Run: " << e.what());
    }

    return 0;
}



/*
  $Log$
  Revision 1.22  2004/11/30 23:39:57  lewisg
  fix interval query

  Revision 1.21  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.20  2004/11/01 22:04:12  lewisg
  c-term mods

  Revision 1.19  2004/10/20 22:24:48  lewisg
  neutral mass bugfix, concatenate result and response

  Revision 1.18  2004/09/29 19:43:09  lewisg
  allow setting of ions

  Revision 1.17  2004/09/15 18:35:00  lewisg
  cz ions

  Revision 1.16  2004/07/06 22:38:05  lewisg
  tax list input and user settable modmax

  Revision 1.15  2004/06/23 22:34:36  lewisg
  add multiple enzymes

  Revision 1.14  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.13  2004/05/27 20:52:15  lewisg
  better exception checking, use of AutoPtr, command line parsing

  Revision 1.12  2004/05/21 21:41:03  gorelenk
  Added PCH ncbi_pch.hpp

  Revision 1.11  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.10  2004/03/16 22:09:11  gorelenk
  Changed include for private header.

  Revision 1.9  2004/03/01 18:24:08  lewisg
  better mod handling

  Revision 1.8  2003/12/04 23:39:09  lewisg
  no-overlap hits and various bugfixes

  Revision 1.7  2003/11/20 15:40:53  lewisg
  fix hitlist bug, change defaults

  Revision 1.6  2003/11/18 18:16:04  lewisg
  perf enhancements, ROCn adjusted params made default

  Revision 1.5  2003/11/13 19:07:38  lewisg
  bugs: iterate completely over nr, don't initialize blastdb by iteration

  Revision 1.4  2003/11/10 22:24:12  lewisg
  allow hitlist size to vary

  Revision 1.3  2003/10/27 20:10:55  lewisg
  demo program to read out omssa results

  Revision 1.2  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.1  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.10  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.9  2003/07/21 20:25:03  lewisg
  fix missing peak bug

  Revision 1.8  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.7  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.6  2003/05/01 14:52:10  lewisg
  fixes to scoring

  Revision 1.5  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.4  2003/04/04 18:43:51  lewisg
  tax cut

  Revision 1.3  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.2  2003/02/10 19:37:56  lewisg
  perf and web page cleanup

  Revision 1.1  2003/02/03 20:39:06  lewisg
  omssa cgi



*/
