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
* Author:  Lewis Geer
*
* File Description:
*   sample simple html program
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/03/15 16:14:27  vasilche
* Fixed new CHTML_form constructor.
*
* Revision 1.1  1999/01/14 21:37:43  lewisg
* added html demo
*
*
* ===========================================================================
*/

// toolkit public headers
#include <corelib/ncbicgir.hpp>
#include <html/html.hpp>
// project private headers

USING_NCBI_SCOPE;  // this turns on the ncbi namespace


// extern "C" main(...) is necessary to make main() visible
// outside of the ncbi namespace

extern "C" int main() 
{
    CCgiResponse Response; // used to write out the html
    CHTML_html * Html; // the following are tags used in the page.
    CHTML_body * Body;
    CHTML_form * Form;

    try {
	// write out the Content-type header
	Response.WriteHeader();

	// create the tags
	Html = new CHTML_html;
	Body = new CHTML_body;
	Form = new CHTML_form("cgidemo", CHTML_form::eGet);

	// stick them together
	Html->AppendChild(Body);
	Body->AppendChild(Form);
	Form->AppendChild(new CHTML_text("name"));
	Form->AppendChild(new CHTML_submit("Submit"));

	// print out the results
	Html->Print(Response.out());
	Response.Flush();

	return 0;  
    }
    // check to see if there were any errors
    catch (exception exc) { 
	// deallocate memory in case of error
	delete Html;
	delete Body;
	delete Form;
        NcbiCerr << "\n" << exc.what() << NcbiEndl;
    }

}
