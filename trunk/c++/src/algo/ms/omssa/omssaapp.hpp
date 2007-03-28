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
 *    base omssa application class
 *
 *
 * ===========================================================================
 */
#ifndef OMSSAAPP__HPP
#define OMSSAAPP__HPP


#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>
#include <objects/omssa/omssa__.hpp>
#include <string>



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


/////////////////////////////////////////////////////////////////////////////
//
//  COMSSABase
//
//  Main application
//


class COMSSABase : public CNcbiApplication {
public:
    COMSSABase();

    /**
     * Set the out files
     * @param IncludeSpectra should we include the spectra in
     *                       output?
     * @param FileFormat the output file format
     * @param FileName output file name
     * @param Setting search settings
     */
    void SetOutFile(bool IncludeSpectra,
                    EMSSerialDataFormat FileFormat,
                    std::string FileName,
                    CRef<CMSSearchSettings> &Settings);

    virtual int Run() = 0;

    virtual void Init();

    /**
     * application specific initialization
     * 
     * @param argDesc argument descriptions
     */
    virtual void AppInit(CArgDescriptions *argDesc);

    void PrintMods(CRef <CMSModSpecSet> Modset);

    void PrintEnzymes(void);

    void PrintIons(void);

    /**
     * Set search settings given args
     */
    void SetSearchSettings(CArgs& args, CRef<CMSSearchSettings> Settings);
};

/**
 * some friendly names for ion series
 */
char const * const kIonLabels[eMSIonType_max] = { 
    "a",
    "b", 
    "c",
    "x", 
    "y", 
    "z",
    "p",
    "n",
    "i",
    "u"
};

/**
 * template for inserting elements of comma delimited string of
 * ints into a list
 * 
 * @param T type of list
 * @param List the comma delimited string
 * @param ToInsert the list
 * @param error error output
 */
template <class T> void 
InsertList(const string& Input, 
           T& ToInsert,
           string error) 
{
    typedef list <string> TStringList;
    TStringList List;
    NStr::Split(Input, ",", List);

    TStringList::iterator iList(List.begin());
    int Num;

    for (;iList != List.end(); iList++) {
        try {
            Num = NStr::StringToInt(*iList);
        }
        catch (CStringException &e) {
            ERR_POST(Info << error << ": " << e.what());
            continue;
        }
        ToInsert.push_back(Num);
    }
}

/**
 * template for inserting elements of comma delimited string of
 * strings into a list
 * 
 * @param T type of list
 * @param List the comma delimited string
 * @param ToInsert the list
 * @param error error output
 */
template <class T> void 
InsertListString(const string& Input, 
           T& ToInsert,
           string error) 
{
    typedef list <string> TStringList;
    TStringList List;
    NStr::Split(Input, ",", List);

    TStringList::iterator iList(List.begin());

    for (;iList != List.end(); iList++) {
        ToInsert.push_back(*iList);
    }
}

END_NCBI_SCOPE
END_SCOPE(objects)
END_SCOPE(omssa)

#endif
