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
 * File Description:   Page Classes
 *
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
    return;
}

CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application),
      m_Style(style)
{
    return;
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

// template struct TagMapper<CHTMLPage>;

CHTMLPage::CHTMLPage(const string& title)
    : m_Title(title)
{
    Init();
}


CHTMLPage::CHTMLPage(const string& title, const string& template_file)
    : m_Title(title)
{
    Init();
    SetTemplateFile(template_file);
}


CHTMLPage::CHTMLPage(const string& title, const istream& template_stream)
    : m_Title(title)
{
    Init();
    SetTemplateStream(template_stream);
}


CHTMLPage::CHTMLPage(const string& /*title*/,
                     const void* template_buffer, size_t size)
{
    Init();
    SetTemplateBuffer(template_buffer, size);
}


CHTMLPage::CHTMLPage(CCgiApplication* application, int style,
                     const string& title, const string& template_file)
    : CParent(application, style),
      m_Title(title)
{
    Init();
    SetTemplateFile(template_file);
}


void CHTMLPage::Init(void)
{
    // Template sources
    m_TemplateFile       = kEmptyStr;
    m_TemplateStream     = 0;
    m_TemplateBuffer     = 0;
    m_TemplateBufferSize = 0;
    
    m_UsePopupMenus      = false;

    AddTagMap("TITLE", CreateTagMapper(this, &CHTMLPage::CreateTitle));
    AddTagMap("VIEW",  CreateTagMapper(this, &CHTMLPage::CreateView));
}


void CHTMLPage::CreateSubNodes(void)
{
    AppendChild(CreateTemplate());
}


CNCBINode* CHTMLPage::CreateTemplate(void) 
{
    // Get template stream
    if ( !m_TemplateFile.empty() ) {
        CNcbiIfstream is(m_TemplateFile.c_str());
        return x_CreateTemplate(is);
    } else if ( m_TemplateStream ) {
        return x_CreateTemplate(*m_TemplateStream);
    } else if ( m_TemplateBuffer ) {
        CNcbiIstrstream is((char*)m_TemplateBuffer, m_TemplateBufferSize);
        return x_CreateTemplate(is);
    } else {
        return new CHTMLText(kEmptyStr);
    }
}

CNCBINode* CHTMLPage::x_CreateTemplate(CNcbiIstream& is)
{
    string str;
    char   buf[1024];

    if ( !is.good() ) {
        // tmp diagnostics to find error //
        if( errno > 0 ) { // #include <errno.h>
            ERR_POST( "CHTMLPage::CreateTemplate: errno: " << strerror(errno) );
        }

        ERR_POST( "CHTMLPage::CreateTemplate: rdstate: " << int(is.rdstate()));
        THROW1_TRACE(runtime_error, "CHTMLPage::CreateTemplate():  \
                     failed to open template");
    }

    while ( is ) {
        is.read(buf, sizeof(buf));
        str.append(buf, is.gcount());
    }

    if ( !is.eof() ) {
        THROW1_TRACE(runtime_error, "CHTMLPage::CreateTemplate():  \
                     error reading template");
    }

    // Insert code in end of <HEAD> and <BODY> blocks for support popup menus
    if ( m_UsePopupMenus ) {
        // a "do ... while (false)" lets us avoid a goto
        do {
            string strl = str;
            NStr::ToLower(strl);

            // Search </HEAD> tag
            SIZE_TYPE pos = strl.find("/head");
            if ( pos == NPOS) {
                break;
            }
            pos = strl.rfind("<", pos);
            if ( pos == NPOS) {
                break;
            }

            SIZE_TYPE script_length = 0;
            // Insert code for load popup menu library
            for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    string script = CHTMLPopupMenu::GetCodeHead(type, info->second.m_Url);
                    script_length += script.length();
                    str.insert(pos, script);
                }
            }

            // Search <BODY> tag
            pos = strl.find("<body", pos);
            if ( pos == NPOS) {
                break;
            }
            pos = strl.find(">", pos);
            if ( pos == NPOS) {
                break;
            }
            for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    if ( CHTMLPopupMenu::GetCodeBodyTagHandler(type) != kEmptyStr ) {
                        string script = " "+CHTMLPopupMenu::GetCodeBodyTagHandler(type) + "=\"" +
                                        CHTMLPopupMenu::GetCodeBodyTagAction(type) + "\"" ;
                        str.insert(pos + script_length, script);
                        script_length += script.length();
                    }
                }
            }

            // Search </BODY> tag
            pos = strl.rfind("/body");
            if ( pos == NPOS) {
                break;
            }
            pos = strl.rfind("<", pos);
            if ( pos == NPOS) {
                break;
            }

            // Insert code for init popup menus
            for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    string script = CHTMLPopupMenu::GetCodeBody(type,
                        info->second.m_UseDynamicMenu);
                    str.insert(pos + script_length, script);
                }
            }
        }
        while (false);
    }

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


void CHTMLPage::EnablePopupMenu(CHTMLPopupMenu::EType type,
                                 const string& menu_script_url,
                                 bool use_dynamic_menu)
{
    SPopupMenuInfo info(menu_script_url, use_dynamic_menu);
    m_PopupMenus[type] = info;
}


static bool s_CheckUsePopupMenus(const CNCBINode* node, CHTMLPopupMenu::EType type)
{
    if ( !node  ||  !node->HaveChildren() ) {
        return false;
    }
    ITERATE ( CNCBINode::TChildren, i, node->Children() ) {
        const CNCBINode* cnode = node->Node(i);
        if ( dynamic_cast<const CHTMLPopupMenu*>(cnode) ) {
            const CHTMLPopupMenu* menu = dynamic_cast<const CHTMLPopupMenu*>(cnode);
            if ( menu->GetType() == type )
                return true;
        }
        if ( cnode->HaveChildren()  &&  s_CheckUsePopupMenus(cnode, type)) {
            return true;
        }
    }
    return false;
}


void CHTMLPage::AddTagMap(const string& name, CNCBINode* node)
{
    CParent::AddTagMap(name, node);

    for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ ) {
        CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
        if ( m_PopupMenus.find(type) == m_PopupMenus.end() ) {
            if ( s_CheckUsePopupMenus(node, type) ) {
                EnablePopupMenu(type);
                m_UsePopupMenus = true;
            }
        } else {
            m_UsePopupMenus = true;
        }
    }
}


void CHTMLPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    CParent::AddTagMap(name,mapper);
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.32  2003/03/11 15:28:57  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.31  2002/12/09 22:11:59  ivanov
 * Added support for Sergey Kurdin's popup menu
 *
 * Revision 1.30  2002/09/16 22:24:52  vakatov
 * Formal fix to get rid of an "unused func arg" warning
 *
 * Revision 1.29  2002/09/11 16:09:27  dicuccio
 * fixed memory leak in CreateTemplate(): added x_CreateTemplate() to get
 * around heap allocation of stream.
 * moved cvs log to the bottom of the page.
 *
 * Revision 1.28  2002/08/09 21:12:02  ivanov
 * Added stuff to read template from a stream and string
 *
 * Revision 1.27  2002/02/23 04:08:25  vakatov
 * Commented out "// template struct TagMapper<CHTMLPage>;" to see if it's
 * still needed for any compiler
 *
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
