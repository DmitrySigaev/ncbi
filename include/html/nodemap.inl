#if defined(HTML___NODEMAP__HPP)  &&  !defined(HTML___NODEMAP__INL)
#define HTML___NODEMAP__INL

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
 * Author:  Eugene Vasilchenko
 *
 */


template<class C>
StaticTagMapperByNode<C>::StaticTagMapperByNode(
    CNCBINode* (*function)(C* node))
    : m_Function(function)
{
    return;
}


template<class C>
CNCBINode* StaticTagMapperByNode<C>::MapTag(
    CNCBINode* _this, const string&) const
{
    return m_Function(dynamic_cast<C*>(_this));
}


template<class C>
StaticTagMapperByNodeAndName<C>::StaticTagMapperByNodeAndName(
    CNCBINode* (*function)(C* node, const string& name))
    : m_Function(function)
{
    return;
}


template<class C>
CNCBINode* StaticTagMapperByNodeAndName<C>::MapTag(
    CNCBINode* _this, const string& name) const
{
    return m_Function(dynamic_cast<C*>(_this), name);
}


template<class C, typename T>
StaticTagMapperByNodeAndData<C,T>::StaticTagMapperByNodeAndData(
    CNCBINode* (*function)(C* node, T data), T data)
    : m_Function(function), m_Data(data)
{
    return;
}


template<class C, typename T>
CNCBINode* StaticTagMapperByNodeAndData<C,T>::MapTag(
    CNCBINode* _this, const string&) const
{
    return m_Function(dynamic_cast<C*>(_this), m_Data);
}


template<class C, typename T>
StaticTagMapperByNodeAndDataAndName<C,T>::StaticTagMapperByNodeAndDataAndName(
    CNCBINode* (*function)(C* node, T data, const string &name), T data)
    : m_Function(function), m_Data(data)
{
    return;
}


template<class C, typename T>
CNCBINode* StaticTagMapperByNodeAndDataAndName<C,T>::MapTag(
    CNCBINode* _this, const string& name) const
{
    return m_Function(dynamic_cast<C*>(_this), m_Data, name);
}


template<class C>
TagMapper<C>::TagMapper(CNCBINode* (C::*method)(void))
    : m_Method(method)
{
    return;
}


template<class C>
CNCBINode* TagMapper<C>::MapTag(CNCBINode* _this, const string&) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)();
}


template<class C>
TagMapperByName<C>::TagMapperByName(
    CNCBINode* (C::*method)(const string& name)
    ) : m_Method(method)
{
    return;
}


template<class C>
CNCBINode* TagMapperByName<C>::MapTag(
    CNCBINode* _this, const string& name) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)(name);
}


template<class C, typename T>
TagMapperByData<C,T>::TagMapperByData(
    CNCBINode* (C::*method)(T data), T data)
    : m_Method(method), m_Data(data)
{
    return;
}


template<class C, typename T>
CNCBINode* TagMapperByData<C,T>::MapTag(
    CNCBINode* _this, const string&) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)(m_Data);
}


template<class C, typename T>
TagMapperByDataAndName<C,T>::TagMapperByDataAndName(
    CNCBINode* (C::*method)(T data, const string& name), T data)
    : m_Method(method), m_Data(data)
{
    return;
}


template<class C, typename T>
CNCBINode* TagMapperByDataAndName<C,T>::MapTag(
    CNCBINode* _this, const string& name) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)(m_Data, name);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/02/02 14:14:15  ivanov
 * Added TagMapper to functons and class methods which used a data parameter
 *
 * Revision 1.8  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.7  1999/10/28 13:40:31  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.6  1999/05/28 16:32:10  vasilche
 * Fixed memory leak in page tag mappers.
 *
 * Revision 1.5  1999/04/27 14:49:58  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.4  1998/12/24 16:15:38  vasilche
 * Added CHTMLComment class.
 * Added TagMappers from static functions.
 *
 * Revision 1.3  1998/12/23 21:20:59  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.2  1998/12/22 16:39:12  vasilche
 * Added ReadyTagMapper to map tags to precreated nodes.
 *
 * Revision 1.1  1998/12/21 22:24:58  vasilche
 * A lot of cleaning.
 *
 * ===========================================================================
 */

#endif /* def HTML___NODEMAP__HPP  &&  ndef HTML___NODEMAP__INL */
