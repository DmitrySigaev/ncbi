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
*   Page Classes
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2002/02/13 20:16:45  ivanov
* Added support of dynamic popup menus. Changed EnablePopupMenu().
*
* Revision 1.25  2001/08/14 16:56:42  ivanov
* Added support for work HTML templates with JavaScript popup menu.
* Renamed type Flags -> ETypes. Moved all code from "page.inl" to header file.
*
* Revision 1.24  2000/03/31 17:08:43  kans
* cast ifstr.rdstate() to int
*
* Revision 1.23  1999/10/28 13:40:36  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.22  1999/09/27 16:17:18  vasilche
* Fixed several incompatibilities with Windows
*
* Revision 1.21  1999/09/23 15:51:42  vakatov
* Added <unistd.h> for the getcwd() proto
*
* Revision 1.20  1999/09/17 14:16:09  sandomir
* tmp diagnostics to find error
*
* Revision 1.19  1999/09/15 15:04:47  sandomir
* minor memory leak in tag mapping
*
* Revision 1.18  1999/07/19 21:05:02  pubmed
* minor change in CHTMLPage::CreateTemplate() - show file name
*
* Revision 1.17  1999/05/28 20:43:10  vakatov
* ::~CHTMLBasicPage(): MSVC++ 6.0 SP3 cant compile:  DeleteElements(m_TagMap);
*
* Revision 1.16  1999/05/28 16:32:16  vasilche
* Fixed memory leak in page tag mappers.
*
* Revision 1.15  1999/05/27 21:46:25  vakatov
* CHTMLPage::CreateTemplate():  throw exception if cannot open or read
* the page template file specified by user
*
* Revision 1.14  1999/04/28 16:52:45  vasilche
* Restored optimized code for reading from file.
*
* Revision 1.13  1999/04/27 16:48:44  vakatov
* Rollback of the buggy "optimization" in CHTMLPage::CreateTemplate()
*
* Revision 1.12  1999/04/26 21:59:31  vakatov
* Cleaned and ported to build with MSVC++ 6.0 compiler
*
* Revision 1.11  1999/04/19 16:51:36  vasilche
* Fixed error with member pointers detected by GCC.
*
* Revision 1.10  1999/04/15 22:10:43  vakatov
* Fixed "class TagMapper<>" to "struct ..."
*
* Revision 1.9  1998/12/28 23:29:10  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.8  1998/12/28 21:48:17  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.7  1998/12/28 16:48:09  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.6  1998/12/22 16:39:15  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.3  1998/12/01 19:10:39  lewisg
* uses CCgiApplication and new page factory
* ===========================================================================
*/

#include <html/components.hpp>
#include <html/page.hpp>
#include <html/jsmenu.hpp>
#include <corelib/ncbiutil.hpp>

#include <errno.h>

BEGIN_NCBI_SCOPE
 

/////////////////////////////////////////////////////////////////////////////
// CHTMLBasicPage

CHTMLBasicPage::CHTMLBasicPage(void)
    : m_CgiApplication(0),
      m_Style(0)
{
}

CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application),
      m_Style(style)
{
}

CHTMLBasicPage::~CHTMLBasicPage(void)
{
    // BW_02:  the following does not compile on MSVC++ 6.0 SP3:
    // DeleteElements(m_TagMap);

    for (TTagMap::iterator i = m_TagMap.begin();  i != m_TagMap.end();  ++i) {
        delete i->second;
    }

}

void CHTMLBasicPage::SetApplication(CCgiApplication* App)
{
    m_CgiApplication = App;
}

void CHTMLBasicPage::SetStyle(int style)
{
    m_Style = style;
}

CNCBINode* CHTMLBasicPage::MapTag(const string& name)
{
    map<string, BaseTagMapper*>::iterator i = m_TagMap.find(name);
    if ( i != m_TagMap.end() ) {
        return (i->second)->MapTag(this, name);
    }
    return CParent::MapTag(name);
}

void CHTMLBasicPage::AddTagMap(const string& name, CNCBINode* node)
{
    AddTagMap(name, CreateTagMapper(node));
}

void CHTMLBasicPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    delete m_TagMap[name];
    m_TagMap[name] = mapper;
}


/////////////////////////////////////////////////////////////////////////////
// CHTMLPage

template struct TagMapper<CHTMLPage>;

CHTMLPage::CHTMLPage(const string& title, const string& template_file)
    : m_Title(title),
      m_TemplateFile(template_file)
{
    Init();
}

CHTMLPage::CHTMLPage(CCgiApplication* application, int style,
                     const string& title, const string& template_file)
    : CParent(application, style),
      m_Title(title),
      m_TemplateFile(template_file)
{
    Init();
}

void CHTMLPage::Init(void)
{
    // Init popup menu variables
    m_UsePopupMenu = false;
    m_UsePopupMenuDynamic = false;
    m_PopupMenuLibUrl = kEmptyStr;

    AddTagMap("TITLE", CreateTagMapper(this, &CHTMLPage::CreateTitle));
    AddTagMap("VIEW",  CreateTagMapper(this, &CHTMLPage::CreateView));
}

void CHTMLPage::CreateSubNodes(void)
{
    AppendChild(CreateTemplate());
}


CNCBINode* CHTMLPage::CreateTemplate(void) 
{
    if (m_TemplateFile.compare(NcbiEmptyString) == 0)
        return new CHTMLText(NcbiEmptyString);

    string        str;
    char          buf[1024];
    CNcbiIfstream ifstr(m_TemplateFile.c_str());
    if ( !ifstr.good() ) {
	  // tmp diagnostics to find error //
	  if( errno > 0 ) { // #include <errno.h>
		ERR_POST( "CHTMLPage::CreateTemplate: errno: " << strerror( errno ) );
	  }
	  
	  ERR_POST( "CHTMLPage::CreateTemplate: rdstate: " 
                << int(ifstr.rdstate()) );	  
	  
      THROW1_TRACE(runtime_error, "\
CHTMLPage::CreateTemplate():  failed to open template file " + m_TemplateFile);
    }

    while ( ifstr ) {
        ifstr.read(buf, sizeof(buf));
        str.append(buf, ifstr.gcount());
    }

    if ( !ifstr.eof() ) {
        THROW1_TRACE(runtime_error, "\
CHTMLPage::CreateTemplate():  error reading template file" + m_TemplateFile);
    }

    // Insert code in end of <HEAD> and <BODY> blocks for support popup menus
    if ( m_UsePopupMenu ) {
        string strl = str;
        NStr::ToLower(strl);
        // Search </HEAD> tag
        SIZE_TYPE pos = strl.find("/head");
        if ( pos == NPOS) goto done;
        pos = strl.rfind("<", pos);
        if ( pos == NPOS) goto done;
        // Insert code for load popup menu library
        string script = CHTMLPopupMenu::GetCodeHead(m_PopupMenuLibUrl);
        SIZE_TYPE length = script.length();
        str.insert(pos, script);
        
        // Search </BODY> tag
        pos = strl.rfind("/body");
        if ( pos == NPOS) goto done;
        pos = strl.rfind("<", pos);
        if ( pos == NPOS) goto done;
        // Insert code for init popup menus
        script = CHTMLPopupMenu::GetCodeBody(m_UsePopupMenuDynamic);
        str.insert(pos+length, script);
    }
done:
    return new CHTMLText(str);
}


CNCBINode* CHTMLPage::CreateTitle(void) 
{
    if ( GetStyle() & fNoTITLE )
        return 0;

    return new CHTMLText(m_Title);
}

CNCBINode* CHTMLPage::CreateView(void) 
{
    return 0;
}


void CHTMLPage::EnablePopupMenu(const string& menu_script_url,
                                bool use_dynamic_menu)
{
    m_UsePopupMenu = true;
    m_PopupMenuLibUrl = menu_script_url;
    m_UsePopupMenuDynamic = use_dynamic_menu;
}


static bool s_CheckUsePopupMenus(CNCBINode* node)
{
    if ( !node  ||  !node->HaveChildren() ) {
        return false;
    }
    non_const_iterate ( CNCBINode::TChildren, i, node->Children() ) {
        CNCBINode* cnode = node->Node(i);
        if ( dynamic_cast<CHTMLPopupMenu*>(cnode) ) {
            return true;
        }
        if ( cnode->HaveChildren()  &&  s_CheckUsePopupMenus(cnode)) {
            return true;
        }
    }
    return false;
}


void CHTMLPage::AddTagMap(const string& name, CNCBINode* node)
{
    CParent::AddTagMap(name, node);

    // If already defined usage popup menus
    if ( m_UsePopupMenu ) {
        return;
    }
    // If "node" contains popup menu, then automaticly enable it
    if ( s_CheckUsePopupMenus(node) ) {
        EnablePopupMenu();
    }
}


void CHTMLPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    CParent::AddTagMap(name,mapper);
}


END_NCBI_SCOPE
