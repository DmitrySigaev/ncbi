#ifndef QUERYPAGES__HPP
#define QUERYPAGES__HPP

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
*   Pages for pubmed query program
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  1999/04/15 22:08:00  vakatov
* CPmDocSumPage:: use "enum { kNo..., };" rather than "static const int kNo...;"
*
* Revision 1.8  1998/12/28 23:29:04  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.7  1998/12/28 21:48:14  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.6  1998/12/28 16:48:06  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.5  1998/12/22 16:39:13  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.4  1998/12/21 22:25:00  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/12/12 00:06:56  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/12/11 22:53:41  lewisg
* added docsum page
*
* Revision 1.1  1998/12/11 18:13:51  lewisg
* frontpage added
*
* ===========================================================================
*/

#include <html/page.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////
///// Basic Search


class CPmFrontPage: public CHTMLPage {
public:
    CPmFrontPage();
    static CHTMLBasicPage * New(void) { return new CPmFrontPage;}

    virtual CNCBINode* CreateView(void);

protected:
    // Clone support
    CNCBINode* CloneSelf(void) const;
};


class CPmDocSumPage: public CHTMLPage
{
public:
    CPmDocSumPage();

    enum flags {
        kNoQUERYBOX = 0x8,
        // only one pager for now until I make copy constructor.
        kNoPAGER    = 0x10
    };

    virtual CNCBINode* CreateView(void);
    virtual CNCBINode* CreatePager(void);
    virtual CNCBINode* CreateQueryBox(void);

    static CHTMLBasicPage * New(void) { return new CPmDocSumPage;}

protected:
    // Clone support
    CNCBINode* CloneSelf(void) const;
};

END_NCBI_SCOPE

#endif
