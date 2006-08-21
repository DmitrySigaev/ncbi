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
#include <corelib/ncbifile.hpp>

#include <fstream>
#include <string>
#include <list>
#include <stdio.h>

#include "omssa.hpp"
#include "SpectrumSet.hpp"
#include "Mod.hpp"


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
    void PrintMods(CRef <CMSModSpecSet> Modset);
    void PrintEnzymes(void);
    void PrintIons(void);

    template <class T> void InsertList(const string& List, T& ToInsert, string error); 

    /**
     * Set search settings given args
     */
    void SetSearchSettings(CArgs& args, CRef<CMSSearchSettings> Settings);
};

COMSSA::COMSSA()
{
    SetVersion(CVersionInfo(2, 0, 0));
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
void COMSSA::PrintMods(CRef <CMSModSpecSet> Modset)
{
    typedef multimap <string, int> TModMap;
    TModMap ModMap;
    int i;
    for(i = 0; i < eMSMod_max; i++) {
        if(Modset->GetModMass(i) != 0.0)
            ModMap.insert(TModMap::value_type(Modset->GetModName(i), i));
    }

    cout << " # : Name" << endl;
    ITERATE (TModMap, iModMap, ModMap) {
        cout << setw(3) << iModMap->second << ": " << iModMap->first << endl;
    }
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
    argDesc->PrintUsageIfNoArgs();

    argDesc->AddDefaultKey("d", "blastdb", "Blast sequence library to search. Do not include .p* filename suffixes.",
			   CArgDescriptions::eString, "nr");
    argDesc->AddDefaultKey("f", "infile", "single dta file to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fx", "xmlinfile", "multiple xml-encapsulated dta files to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fb", "dtainfile", "multiple dta files separated by blank lines to search",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fp", "pklinfile", "pkl formatted file",
                CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fm", "pklinfile", "mgf formatted file",
                CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("foms", "omsinfile", "omssa oms file",
                 CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("fomx", "omxinfile", "omssa omx file",
                  CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("o", "textasnoutfile", "filename for text asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ob", "binaryasnoutfile", "filename for binary asn.1 formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("ox", "xmloutfile", "filename for xml formatted search results",
			   CArgDescriptions::eString, "");
    argDesc->AddDefaultKey("oc", "csvfile", "filename for csv formatted search summary",
                CArgDescriptions::eString, "");
    argDesc->AddFlag("w", "include spectra and search params in search results");
    argDesc->AddDefaultKey("to", "pretol", "product ion mass tolerance in Da",
			   CArgDescriptions::eDouble, "0.8");
    argDesc->AddDefaultKey("te", "protol", "precursor ion  mass tolerance in Da",
			   CArgDescriptions::eDouble, "2.0");
    argDesc->AddDefaultKey("tom", "promass", "product ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tem", "premass", "precursor ion search type (0 = mono, 1 = avg, 2 = N15, 3 = exact)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("tez", "prozdep", "charge dependency of precursor mass tolerance (0 = none, 1 = linear)",
                CArgDescriptions::eInteger, "1");
    argDesc->AddDefaultKey("tex", "exact", 
                   "threshold in Da above which the mass of neutron should be added in exact mass search",
                   CArgDescriptions::eDouble, 
                   "1446.94");

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
    argDesc->AddDefaultKey("cp", "precursorcull", 
                "eliminate charge reduced precursors in spectra (0=no, 1=yes)",
                CArgDescriptions::eInteger, "0");
    argDesc->AddDefaultKey("v", "cleave", 
			   "number of missed cleavages allowed",
			   CArgDescriptions::eInteger, "1");
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
			   "maximum number of hits retained per precursor charge state per spectrum",
			   CArgDescriptions::eInteger, "30");
    argDesc->AddDefaultKey("ht", "tophitnum", 
			   "number of m/z values corresponding to the most intense peaks that must include one match to the theoretical peptide",
			   CArgDescriptions::eInteger, "6");
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
    argDesc->AddDefaultKey("mx", "modinputfile", 
			   "file containing modification data",
			   CArgDescriptions::eString,
			   "mods.xml");
    argDesc->AddDefaultKey("mux", "usermodinputfile", 
                "file containing user modification data",
                CArgDescriptions::eString,
                "usermods.xml");
    argDesc->AddDefaultKey("mm", "maxmod", 
			   "the maximum number of mass ladders to generate per database peptide",
			   CArgDescriptions::eInteger, /*NStr::IntToString(MAXMOD2)*/ "128");
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
				"1");
    argDesc->AddDefaultKey("zoh", "maxprodcharge", 
                  "maximum product charge to search",
                  CArgDescriptions::eInteger, 
                  "2");
	argDesc->AddDefaultKey("zt", "chargethresh", 
				 "minimum precursor charge to start considering multiply charged products",
				 CArgDescriptions::eInteger, 
				 "3");
    argDesc->AddDefaultKey("z1", "plusone", 
                  "fraction of peaks below precursor used to determine if spectrum is charge 1",
                  CArgDescriptions::eDouble, 
                  "0.95");
    argDesc->AddDefaultKey("zc", "calcplusone", 
                  "should charge plus one be determined algorithmically? (1=yes)",
                  CArgDescriptions::eInteger, 
                  NStr::IntToString(eMSCalcPlusOne_calc));
    argDesc->AddDefaultKey("zcc", "calccharge", 
                   "how should precursor charges be determined? (1=believe the input file, 2=use a range)",
                   CArgDescriptions::eInteger, 
                   "2");
    argDesc->AddDefaultKey("pc", "pseudocount", 
                  "minimum number of precursors that match a spectrum",
                  CArgDescriptions::eInteger, 
                  "1");
    argDesc->AddDefaultKey("sb1", "searchb1", 
                   "should first forward (b1) product ions be in search (1=no)",
                   CArgDescriptions::eInteger, 
                   "1");
    argDesc->AddDefaultKey("sct", "searchcterm", 
                   "should c terminus ions be searched (1=no)",
                   CArgDescriptions::eInteger, 
                   "0");
    argDesc->AddDefaultKey("sp", "productnum", 
                    "max number of ions in each series being searched (0=all)",
                    CArgDescriptions::eInteger, 
                    "100");
    argDesc->AddDefaultKey("scorr", "corrscore", 
                     "turn off correlation correction to score (1=off, 0=use correlation)",
                     CArgDescriptions::eInteger, 
                     "1");
    argDesc->AddDefaultKey("scorp", "corrprob", 
                      "probability of consecutive ion (used in correlation correction)",
                      CArgDescriptions::eDouble, 
                      "0.5");
    argDesc->AddDefaultKey("no", "minno", 
                     "minimum size of peptides for no-enzyme and semi-tryptic searches",
                     CArgDescriptions::eInteger, 
                     "4");
    argDesc->AddDefaultKey("nox", "maxno", 
                      "maximum size of peptides for no-enzyme and semi-tryptic searches (0=none)",
                      CArgDescriptions::eInteger, 
                      "40");
    argDesc->AddDefaultKey("is", "subsetthresh", 
                           "evalue threshold to include a sequence in the iterative search, 0 = all",
                           CArgDescriptions::eDouble, 
                           "0.0");
    argDesc->AddDefaultKey("ir", "replacethresh", 
                            "evalue threshold to replace a hit, 0 = only if better",
                            CArgDescriptions::eDouble, 
                            "0.0");
    argDesc->AddDefaultKey("ii", "iterativethresh", 
                            "evalue threshold to iteratively search a spectrum again, 0 = always",
                            CArgDescriptions::eDouble, 
                            "0.01");
    argDesc->AddFlag("ni", "don't print informational messages");
    argDesc->AddFlag("ns", "depreciated flag"); // no longer has an effect, replaced by "os"
    argDesc->AddFlag("os", "use omssa 1.0 scoring");


    SetupArgDescriptions(argDesc.release());

    // allow info posts to be seen
    SetDiagPostLevel(eDiag_Info);
}

int main(int argc, const char* argv[]) 
{
    COMSSA theTestApp;
    return theTestApp.AppMain(argc, argv, 0, eDS_Default, 0);
}


void COMSSA::SetSearchSettings(CArgs& args, CRef<CMSSearchSettings> Settings)
{
    // set up input files

    CRef <CMSInFile> Infile; 
    Infile = new CMSInFile;

    if(args["fx"].AsString().size() != 0) {
        Infile->SetInfile(args["fx"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dtaxml);
    }
    else if(args["f"].AsString().size() != 0) {
        Infile->SetInfile(args["f"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dta);
    }
    else if(args["fb"].AsString().size() != 0)  {
        Infile->SetInfile(args["fb"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_dtablank);
    }
    else if(args["fp"].AsString().size() != 0)  {
        Infile->SetInfile(args["fp"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_pkl);
    }
    else if(args["fm"].AsString().size() != 0) {
        Infile->SetInfile(args["fm"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_mgf);
    }
    else if(args["fomx"].AsString().size() != 0) {
        Infile->SetInfile(args["fomx"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_omx);
    }
    else if(args["foms"].AsString().size() != 0) {
        Infile->SetInfile(args["foms"].AsString());
        Infile->SetInfiletype(eMSSpectrumFileType_oms);
    }

    Settings->SetInfiles().push_back(Infile);


    // set up output files

    CRef <CMSOutFile> Outfile;
    Outfile = new CMSOutFile;

    if(args["o"].AsString() != "") {
        Outfile->SetOutfile(args["o"].AsString());
        Outfile->SetOutfiletype(eMSSerialDataFormat_asntext);
    }
    if(args["ob"].AsString() != "") {
        Outfile->SetOutfile(args["ob"].AsString());
        Outfile->SetOutfiletype(eMSSerialDataFormat_asnbinary);
    }
    if(args["ox"].AsString() != "") {
        Outfile->SetOutfile(args["ox"].AsString());
        Outfile->SetOutfiletype(eMSSerialDataFormat_xml);
    }
    if(args["oc"].AsString() != "") {
        Outfile->SetOutfile(args["oc"].AsString());
        Outfile->SetOutfiletype(eMSSerialDataFormat_csv);
    }

    if(args["w"]) {
        Outfile->SetIncluderequest(true);
    }
    else {
        Outfile->SetIncluderequest(false);
    }

    Settings->SetOutfiles().push_back(Outfile);

    // set up rest of settings

	Settings->SetPrecursorsearchtype(args["tem"].AsInteger());
	Settings->SetProductsearchtype(args["tom"].AsInteger());
	Settings->SetPeptol(args["te"].AsDouble());
	Settings->SetMsmstol(args["to"].AsDouble());
    Settings->SetZdep(args["tez"].AsInteger());
    Settings->SetExactmass(args["tex"].AsDouble());

	InsertList(args["i"].AsString(), Settings->SetIonstosearch(), "unknown ion");
	Settings->SetCutlo(args["cl"].AsDouble());
	Settings->SetCuthi(args["ch"].AsDouble());
	Settings->SetCutinc(args["ci"].AsDouble());
    Settings->SetPrecursorcull(args["cp"].AsInteger());
	Settings->SetSinglewin(args["w1"].AsInteger());
	Settings->SetDoublewin(args["w2"].AsInteger());
	Settings->SetSinglenum(args["h1"].AsInteger());
	Settings->SetDoublenum(args["h2"].AsInteger());
	Settings->SetEnzyme(args["e"].AsInteger());
	Settings->SetMissedcleave(args["v"].AsInteger());
	InsertList(args["mv"].AsString(), Settings->SetVariable(), "unknown variable mod");
	InsertList(args["mf"].AsString(), Settings->SetFixed(), "unknown fixed mod");
	Settings->SetDb(args["d"].AsString());
	Settings->SetHitlistlen(args["hl"].AsInteger());
	Settings->SetTophitnum(args["ht"].AsInteger());
	Settings->SetMinhit(args["hm"].AsInteger());
	Settings->SetMinspectra(args["hs"].AsInteger());
    Settings->SetScale(MSSCALE); // presently ignored
	Settings->SetCutoff(args["he"].AsDouble());
	Settings->SetMaxmods(args["mm"].AsInteger());
    Settings->SetPseudocount(args["pc"].AsInteger());
    Settings->SetSearchb1(args["sb1"].AsInteger());
    Settings->SetSearchctermproduct(args["sct"].AsInteger());
    Settings->SetMaxproductions(args["sp"].AsInteger());
    Settings->SetNocorrelationscore(args["scorr"].AsInteger());
    Settings->SetProbfollowingion(args["scorp"].AsDouble());

    Settings->SetMinnoenzyme(args["no"].AsInteger());
    Settings->SetMaxnoenzyme(args["nox"].AsInteger());

	if(args["x"].AsString() != "0") {
	    InsertList(args["x"].AsString(), Settings->SetTaxids(), "unknown tax id");
	}

	Settings->SetChargehandling().SetCalcplusone(args["zc"].AsInteger());
	Settings->SetChargehandling().SetConsidermult(args["zt"].AsInteger());
	Settings->SetChargehandling().SetMincharge(args["zl"].AsInteger());
	Settings->SetChargehandling().SetMaxcharge(args["zh"].AsInteger());
    Settings->SetChargehandling().SetPlusone(args["z1"].AsDouble());
    Settings->SetChargehandling().SetMaxproductcharge(args["zoh"].AsInteger());
    Settings->SetChargehandling().SetCalccharge(args["zcc"].AsInteger());

    Settings->SetIterativesettings().SetResearchthresh(args["ii"].AsDouble());
    Settings->SetIterativesettings().SetSubsetthresh(args["is"].AsDouble());
    Settings->SetIterativesettings().SetReplacethresh(args["ir"].AsDouble());

	// validate the input

    list <string> ValidError;
	if(Settings->Validate(ValidError) != 0) {
	    list <string>::iterator iErr;
	    for(iErr = ValidError.begin(); iErr != ValidError.end(); iErr++)
		ERR_POST(Warning << *iErr);
	    ERR_POST(Fatal << "Unable to validate settings");
	}
     return;
}


int COMSSA::Run()
{    

    try {

	CArgs args = GetArgs();
    CRef <CMSModSpecSet> Modset(new CMSModSpecSet);

    // turn off informational messages if requested
    if(args["ni"])
       SetDiagPostLevel(eDiag_Error);

    // read in modifications
    if(CSearchHelper::ReadModFiles(args["mx"].AsString(), args["mux"].AsString(),
                            GetProgramExecutablePath(), Modset))
        return 1;

	// print out the modification list
	if(args["ml"]) {
        Modset->CreateArrays();
	    PrintMods(Modset);
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
    
    CSearch SearchEngine;

    // set up rank scoring
    if(args["os"]) SearchEngine.SetRankScore() = false;
    else SearchEngine.SetRankScore() = true;

	int retval = SearchEngine.InitBlast(args["d"].AsString().c_str());
	if(retval) {
	    ERR_POST(Fatal << "omssacl: unable to initialize blastdb, error " 
		     << retval);
	    return 1;
	}

    CMSSearch MySearch;

    CRef <CMSRequest> Request (new CMSRequest);
    MySearch.SetRequest().push_back(Request);
    CRef <CMSResponse> Response (new CMSResponse);
    MySearch.SetResponse().push_back(Response);


    // which search settings to use
    CRef <CMSSearchSettings> SearchSettings;

    // set up search settings
    MySearch.SetUpSearchSettings(SearchSettings, 
                                 SearchEngine.GetIterative());

    SetSearchSettings(args, SearchSettings);

    int FileRetVal(1);

    if(SearchSettings->GetInfiles().size() == 1) {
        FileRetVal = 
            CSearchHelper::LoadAnyFile(MySearch,
                                       *(SearchSettings->GetInfiles().begin()), SearchEngine);
        if(FileRetVal == -1) {
            ERR_POST(Fatal << "omssacl: too many spectra in input file");
            return 1;
        }
        else if(FileRetVal == 1) {
            ERR_POST(Fatal << "omssacl: unable to read spectrum file -- incorrect file type?");
            return 1;
        }
    }
	else {
	    ERR_POST(Fatal << "omssacl: input file not given or too many input files given.");
	    return 1;
	}


	_TRACE("omssa: search begin");
	SearchEngine.Search(*MySearch.SetRequest().begin(),
                  *MySearch.SetResponse().begin(), 
                  Modset,
                  SearchSettings);
	_TRACE("omssa: search end");


#if _DEBUG
	// read out hits
	CMSResponse::THitsets::const_iterator iHits;
	iHits = (*MySearch.SetResponse().begin())->GetHitsets().begin();
	for(; iHits != (*MySearch.SetResponse().begin())->GetHitsets().end(); iHits++) {
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
		    ERR_POST(Info << ((*iPephit)->CanGetGi()?(*iPephit)->GetGi():0) << 
                     ": " << (*iPephit)->GetStart() << "-" << (*iPephit)->GetStop() << 
                     ":" << (*iPephit)->GetDefline());
		}
    
	    }
	}
#endif

	// Check to see if there is a hitset

	if(!(*MySearch.SetResponse().begin())->CanGetHitsets()) {
	  ERR_POST(Fatal << "No results found");
	}
    
    CSearchHelper::SaveAnyFile(MySearch, 
                               SearchSettings->GetOutfiles(),
                               Modset);
    
    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Fatal << "Exception in COMSSA::Run: " << e.what());
    }

    return 0;
}



