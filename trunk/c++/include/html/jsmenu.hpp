#ifndef JSMENU__HPP
#define JSMENU__HPP

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
 * Author:  Vladimir Ivanov
 *
 *
 */

/// @file jsmenu.hpp 
/// JavaScript popup menu support.


#include <corelib/ncbistd.hpp>
#include <html/node.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Popup menu attribute.
///
/// If attribute not define for menu with function SetAttribute(), 
/// then it have some default value dependent on menu type.
/// All attributes have effect only for specified menu type, otherwise it
/// will be ignored.

enum EHTML_PM_Attribute {
    //                               Using by     Value example
    //
    //                               S  - eSmith 
    //                               K  - eKurdin 
    //                               KS - eKurdinSide
    
    eHTML_PM_enableTracker,          // S         "true"
    eHTML_PM_disableHide,            // S   KS    "false"
    eHTML_PM_menuWidth,              //     KS    "100"
    eHTML_PM_peepOffset,             //     KS    "20"
    eHTML_PM_topOffset,              //     KS    "10"
    eHTML_PM_fontSize,               // S         "14"
    eHTML_PM_fontWeigh,              // S         "plain"
    eHTML_PM_fontFamily,             // S         "arial,helvetica"
    eHTML_PM_fontColor,              // S         "black"
    eHTML_PM_fontColorHilite,        // S         "#ffffff"
    eHTML_PM_menuBorder,             // S         "1"
    eHTML_PM_menuItemBorder,         // S         "0"
    eHTML_PM_menuItemBgColor,        // S         "#cccccc"
    eHTML_PM_menuLiteBgColor,        // S         "white"
    eHTML_PM_menuBorderBgColor,      // S         "#777777"
    eHTML_PM_menuHiliteBgColor,      // S         "#000084"
    eHTML_PM_menuContainerBgColor,   // S         "#cccccc"
    eHTML_PM_childMenuIcon,          // S         "images/arrows.gif"
    eHTML_PM_childMenuIconHilite,    // S         "images/arrows2.gif"
    eHTML_PM_bgColor,                // S K       "#555555"
    eHTML_PM_titleColor,             //   K       "#FFFFFF"
    eHTML_PM_borderColor,            //   K       "black"
    eHTML_PM_alignH,                 //   K       "left" or "right"
    eHTML_PM_alignV,                 //   K       "bottom" or "top"
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHTMLPopupMenu --
///
/// Define for support JavaScript popup menues.
///
/// For successful work menu in HTML pages next steps required:
///    - File with popup menu JavaScript library -- "*.js".
///      By default using menu with URL, defined in constant
///      kJSMenuDefaultURL*, defined in the jsmenu.cpp.
///    - Define menues and add it to HTML page (AppendChild()).
///    - Call EnablePopupMenu() (member function of CHTML_html and CHTMLPage).
/// 
/// NOTE: We must add menues to a BODY only, otherwise menu not will be work.
/// NOTE: Menues of eKurdinSide type must be added (using AppendChild) only
///       to a HEAD node. And menu of this type must be only one on the page!

class CHTMLPopupMenu : public CNCBINode
{
    typedef CNCBINode CParent;
    friend class CHTMLPage;
    friend class CHTMLNode;
public:

    /// Popup menu type.
    enum EType {
        eSmith,             ///< Smith's menu (ncbi_menu*.js)
        eKurdin,            ///< Sergey Kurdin's popup menu (popupmenu2*.js)
        eKurdinSide,        ///< Sergey Kurdin's side menu (sidemenu*.js)

        ePMFirst = eSmith,
        ePMLast  = eKurdinSide
    };

    /// Constructor.
    ///
    /// Construct menu with name "name" (JavaScript variable name).
    CHTMLPopupMenu(const string& name, EType type = eSmith);

    /// Destructor.
    virtual ~CHTMLPopupMenu(void);

    /// Get menu name.
    string GetName(void) const;
    /// Get menu type.
    EType  GetType(void) const;


    // Add new item to current menu
    //
    // NOTE: action - can be also URL type like "http://..."
    // NOTE: Parameters have some restrictions according to menu type:
    //       title  - can be text or HTML-code (for eSmith menu type only)
    //       color  - will be ignored for eKurdin menu type

    void AddItem(const string& title,                  // Text or HTML-code
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    void AddItem(const char*   title,                  // Text or HTML-code
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    // NOTE: The "node" will be convert to a string inside function, so
    //       the node's Print() method must not change a node structure.
    void AddItem(CNCBINode& node,
                 const string& action    = kEmptyStr,  // JS code
                 const string& color     = kEmptyStr,
                 const string& mouseover = kEmptyStr,  // JS code
                 const string& mouseout  = kEmptyStr); // JS code

    /// Add item's separator.
    ///
    /// NOTE: do nothing for eKurdin menu type
    void AddSeparator(void); 

    /// Set menu attribute.
    void SetAttribute(EHTML_PM_Attribute attribute, const string& value);

    /// Get JavaScript code for menu call.
    string ShowMenu(void) const;

    /// Get HTML code for inserting into end of the HEAD and BODY blocks.
    /// If "menu_lib_url" is not defined, then use default URL.
    static string GetCodeHead(EType type = eSmith,
                              const string& menu_lib_url = kEmptyStr);
    static string GetCodeBody(EType type = eSmith,
                              bool use_dynamic_menu = false);
    /// Get HTML code for inserting into end of the BODY block.
    static string GetCodeBodyTagHandler(EType type);
    static string GetCodeBodyTagAction(EType type);

private:
    /// Get code for menu items in text format (internal JS function code).
    string GetCodeMenuItems(void) const;

    /// Print menu.
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

    /// Menu item type.
    struct SItem {
        /// Constructor for menu item.
        SItem(const string& title, const string& action, const string& color,
              const string& mouseover, const string& mouseout);
        /// Constructor for separator.
        SItem(void);

        string title;      ///< Menu item title.
        string action;     ///< JS action on press item.
        string color;      ///< ? (not used in JSMenu script).
        string mouseover;  ///< JS action on mouse over event for item.
        string mouseout;   ///< JS action on mouse out event for item.
    };
    typedef list<SItem> TItems;

    /// Menu attribute type.
    typedef map<EHTML_PM_Attribute, string> TAttributes;

    /// Get attribute value.
    string GetMenuAttributeValue(EHTML_PM_Attribute attribute) const;

    /// Get attribute name
    string GetMenuAttributeName(EHTML_PM_Attribute attribute) const;

    string       m_Name;   ///< Menu name
    EType        m_Type;   ///< Menu type
    TItems       m_Items;  ///< Menu items
    TAttributes  m_Attrs;  ///< Menu attributes
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.11  2003/10/01 15:55:04  ivanov
 * Added support for Sergey Kurdin's side menu
 *
 * Revision 1.10  2003/04/25 13:45:32  siyan
 * Added doxygen groupings
 *
 * Revision 1.9  2002/12/12 17:20:21  ivanov
 * Renamed GetAttribute() -> GetMenuAttributeValue,
 *         GetAttributeName() -> GetMenuAttributeName().
 *
 * Revision 1.8  2002/12/09 22:12:45  ivanov
 * Added support for Sergey Kurdin's popup menu.
 *
 * Revision 1.7  2002/04/29 15:48:16  ucko
 * Make CHTMLPopupMenu::GetName const for consistency with CNcbiNode (and
 * because there's no reason for it not to be const).
 *
 * Revision 1.6  2002/02/13 20:15:39  ivanov
 * Added support of dynamic popup menues. Changed GetCodeBody().
 *
 * Revision 1.5  2001/11/29 16:05:16  ivanov
 * Changed using menu script name "menu.js" -> "ncbi_menu.js"
 *
 * Revision 1.4  2001/10/15 23:16:19  vakatov
 * + AddItem(const char* title, ...) to avoid "string/CNCBINode" ambiguity
 *
 * Revision 1.3  2001/08/15 19:43:30  ivanov
 * Added AddMenuItem( node, ...)
 *
 * Revision 1.2  2001/08/14 16:52:47  ivanov
 * Changed parent class for CHTMLPopupMenu.
 * Changed means for init JavaScript popup menu & add it to HTML document.
 *
 * Revision 1.1  2001/07/16 13:45:33  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif // JSMENU__HPP
