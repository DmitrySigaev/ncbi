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
*   The components that go on the pages.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  1999/01/21 21:12:58  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* Revision 1.21  1999/01/21 16:18:05  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.20  1999/01/20 18:12:44  vasilche
* Added possibility to change label of buttons.
*
* Revision 1.19  1999/01/19 21:17:41  vasilche
* Added CPager class
*
* Revision 1.18  1999/01/15 17:47:55  vasilche
* Changed CButtonList options: m_Name -> m_SubmitName, m_Select ->
* m_SelectName. Added m_Selected.
* Fixed CIDs Encode/Decode.
*
* Revision 1.17  1999/01/14 21:25:19  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.16  1999/01/07 17:06:35  vasilche
* Added default query text in CQueryBox.
* Added query text width in CQueryBox.
*
* Revision 1.15  1999/01/07 16:41:56  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* Revision 1.14  1999/01/07 14:53:57  vasilche
* CButtonList displays nothing if list is empty.
*
* Revision 1.13  1999/01/06 21:35:37  vasilche
* Avoid use of Clone.
* Fixed default CHTML_text width.
*
* Revision 1.12  1999/01/05 21:47:14  vasilche
* Added 'current page' to CPageList.
* CPageList doesn't display forward/backward if empty.
*
* Revision 1.11  1999/01/04 20:06:13  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.10  1998/12/28 23:29:09  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 16:48:08  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.8  1998/12/23 21:21:03  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.7  1998/12/21 22:25:02  vasilche
* A lot of cleaning.
*
* Revision 1.6  1998/12/11 22:52:01  lewisg
* added docsum page
*
* Revision 1.5  1998/12/11 18:12:45  lewisg
* frontpage added
*
* Revision 1.4  1998/12/09 23:00:53  lewisg
* use new cgiapp class
*
* Revision 1.3  1998/12/08 00:33:42  lewisg
* cleanup
*
* Revision 1.2  1998/11/23 23:44:28  lewisg
* toolpages.cpp
*
* Revision 1.1  1998/10/29 16:13:07  lewisg
* version 2
*
* ===========================================================================
*/

#include <html/components.hpp>
#include <html/nodemap.hpp>

BEGIN_NCBI_SCOPE

CSubmitDescription::CSubmitDescription(void)
{
}

CSubmitDescription::CSubmitDescription(const string& name)
    : m_Name(name)
{
}

CSubmitDescription::CSubmitDescription(const string& name, const string& label)
    : m_Name(name), m_Label(label)
{
}

CNCBINode* CSubmitDescription::CreateComponent(void) const
{
    if ( m_Name.empty() )
        return 0;

    if ( m_Label.empty() )
        return new CHTML_submit(m_Name);
    else
        return new CHTML_submit(m_Name, m_Label);
}

CNCBINode* COptionDescription::CreateComponent(const string& def) const
{
    if ( m_Value.empty() )
        return new CHTML_option(m_Label, m_Label == def);
    else if ( m_Label.empty() )
        return new CHTML_option(m_Value, m_Value == def);
    else
        return new CHTML_option(m_Label, m_Value, m_Value == def);
}

CSelectDescription::CSelectDescription(void)
{
}

CSelectDescription::CSelectDescription(const string& name)
    : m_Name(name)
{
}

void CSelectDescription::Add(const string& value)
{
    m_List.push_back(COptionDescription(value));
}

void CSelectDescription::Add(const string& value, const string& label)
{
    m_List.push_back(COptionDescription(value, label));
}

CNCBINode* CSelectDescription::CreateComponent(void) const
{
    if ( m_Name.empty() || m_List.empty() )
        return 0;

    CNCBINode* select = new CHTML_select(m_Name);
    for ( list<COptionDescription>::const_iterator i = m_List.begin();
          i != m_List.end(); ++i ) {
        select->AppendChild(i->CreateComponent(m_Default));
    }
    if ( !m_TextBefore.empty() || !m_TextAfter.empty() ) {
        CNCBINode* combine = new CNCBINode;
        if ( !m_TextBefore.empty() )
            combine->AppendChild(new CHTMLPlainText(m_TextBefore));
        combine->AppendChild(select);
        if ( !m_TextAfter.empty() )
            combine->AppendChild(new CHTMLPlainText(m_TextAfter));
        select = combine;
    }
    return select;
}

CTextInputDescription::CTextInputDescription(void)
    : m_Width(0)
{
}

CTextInputDescription::CTextInputDescription(const string& name)
    : m_Name(name), m_Width(0)
{
}

CNCBINode* CTextInputDescription::CreateComponent(void) const
{
    if ( m_Name.empty() )
        return 0;

    if ( m_Width )
        return new CHTML_text(m_Name, m_Width, m_Value);
    else
        return new CHTML_text(m_Name, m_Value);
}

CQueryBox::CQueryBox(const string& URL)
    : CParent(URL),
      m_Submit("cmd", "Search"), m_Database("db"),
      m_Term("term"), m_DispMax("dispmax")
{
    m_Database.m_TextBefore = "Search ";
    m_Database.m_TextAfter = "for";
    m_DispMax.m_TextBefore = "Show ";
    m_DispMax.m_TextAfter = "documents per page";
}

CNCBINode* CQueryBox::CloneSelf(void) const
{
    return new CQueryBox(*this);
}

void CQueryBox::CreateSubNodes()
{
    for ( map<string, string>::iterator i = m_HiddenValues.begin();
          i != m_HiddenValues.end(); ++i ) {
        AddHidden(i->first, i->second);
    }

    AppendChild(m_Database.CreateComponent());
   
    CHTML_table* table = new CHTML_table();
    table->SetCellSpacing(0)->SetCellPadding(5)->SetBgColor(m_BgColor)->SetWidth(m_Width);
    AppendChild(table);

    table->InsertAt(0, 0, m_Term.CreateComponent());
    table->InsertAt(0, 0, m_Submit.CreateComponent());
    table->InsertAt(1, 0, m_DispMax.CreateComponent()); 
}

CNCBINode* CQueryBox::CreateComments(void)
{
    return 0;
}


// pager

CButtonList::CButtonList(void)
{
}

CNCBINode* CButtonList::CloneSelf(void) const
{
    return new CButtonList(*this);
}

void CButtonList::CreateSubNodes()
{
    CNCBINode* select = m_List.CreateComponent();
    if ( select ) {
        AppendChild(m_Button.CreateComponent());
        AppendChild(select);
    }
}

CPageList::CPageList(void)
    : m_Current(-1)
{
    SetCellSpacing(2);
}

void CPageList::x_AddImageString(CNCBINode* node, const string& name, int number,
                                 const string& imageStart, const string& imageEnd)
{
    string s = NStr::IntToString(number);
    
    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_image(name, imageStart + s[i] + imageEnd, 0));
    }
}

void CPageList::x_AddInactiveImageString(CNCBINode* node, const string& name,
                                         int number,
                                         const string& imageStart,
                                         const string& imageEnd)
{
    string s = NStr::IntToString(number);
    
    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_img(imageStart + s[i] + imageEnd));
    }
}

void CPageList::CreateSubNodes()
{
    int column = 0;
    if ( !m_Backward.empty() )
        InsertAt(0, column++, new CHTML_image(m_Backward, "/images/prev.gif", 0));
    
    for (map<int, string>::iterator i = m_Pages.begin();
         i != m_Pages.end(); ++i ) {
        if ( i->first == m_Current ) {
            // current link
            x_AddInactiveImageString(Cell(0, column++), i->second, i->first, "/images/black_", ".gif");
        }
        else {
            // normal link
            x_AddImageString(Cell(0, column++), i->second, i->first, "/images/", ".gif");
        }
    }

    if ( !m_Forward.empty() )
        InsertAt(0, column++, new CHTML_image(m_Forward, "/images/next.gif", 0));
}

// Pager box
CPagerBox::CPagerBox(void)
    : m_NumResults(0), m_Width(460)
    , m_TopButton(new CButtonList), m_LeftButton(new CButtonList), m_RightButton(new CButtonList)
    , m_PageList(new CPageList), m_BgColor("#c0c0c0")
{
}

void CPagerBox::CreateSubNodes(void)
{
    CHTML_table* table;
    CHTML_table* tableTop;
    CHTML_table* tableBot;

    table = new CHTML_table();
    table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)->SetWidth(m_Width)->SetAttribute("border", "0");
    AppendChild(table);

    tableTop = new CHTML_table();
    tableTop->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);
    tableBot = new CHTML_table();
    tableBot->SetCellSpacing(0)->SetCellPadding(0)->SetWidth(m_Width);

    table->InsertAt(0, 0, tableTop);
    table->InsertAt(1, 0, tableBot);
    tableTop->InsertAt(0, 0, m_TopButton);
    tableTop->InsertAt(0, 1, m_PageList);
    tableBot->InsertAt(0, 0, m_LeftButton);
    tableBot->InsertAt(0, 1, m_RightButton);
    tableBot->InsertAt(0, 2, new CHTMLText(NStr::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
}

CNCBINode* CPagerBox::CloneSelf(void) const
{
    return new CPagerBox(*this);
}

CSmallPagerBox::CSmallPagerBox()
    : m_NumResults(0), m_Width(460), m_PageList(0)
{
}

CNCBINode* CSmallPagerBox::CloneSelf(void) const
{
    return new CSmallPagerBox(*this);
}

void CSmallPagerBox::CreateSubNodes()
{
    CHTML_table * Table;

    try {
        Table = new CHTML_table();
        Table->SetCellSpacing(0)->SetCellPadding(0)->SetBgColor(m_BgColor)->SetWidth(m_Width)->SetAttribute("border", 0);
        AppendChild(Table);
        
        Table->InsertAt(0, 0, new CPageList);
        Table->InsertAt(0, 1, new CHTMLText(NStr::IntToString(m_NumResults) + ((m_NumResults==1)?" result":" results")));
    }
    catch (...) {
        delete Table;
    }
}

END_NCBI_SCOPE
