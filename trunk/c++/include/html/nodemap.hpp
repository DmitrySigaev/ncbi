#ifndef NODEMAP__HPP
#define NODEMAP__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1998/12/24 16:15:37  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.3  1998/12/23 21:20:59  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.2  1998/12/22 16:39:11  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.1  1998/12/21 22:24:58  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

#include <ncbistd.hpp>

//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  !!! PUT YOUR OTHER #include's HERE !!!
//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


BEGIN_NCBI_SCOPE

class CNCBINode;

struct BaseTagMapper
{
    virtual ~BaseTagMapper(void);

    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const = 0;
};

struct StaticTagMapper : BaseTagMapper
{
    StaticTagMapper(CNCBINode* (*function)(void));
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(void);
};

struct StaticTagMapperByName : BaseTagMapper
{
    StaticTagMapperByName(CNCBINode* (*function)(const string& name));
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(const string& name);
};

template<class C>
struct StaticTagMapperByNode : BaseTagMapper
{
    StaticTagMapperByNode(CNCBINode* (*function)(C* node));
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(C* node);
};

template<class C>
struct StaticTagMapperByNodeAndName : BaseTagMapper
{
    StaticTagMapperByNodeAndName(CNCBINode* (*function)(C* node, const string& name));
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(C* node, const string& name);
};

struct ReadyTagMapper : BaseTagMapper
{
    ReadyTagMapper(CNCBINode* node);

    ~ReadyTagMapper(void);

    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* m_Node;
};

template<class C>
struct TagMapper : BaseTagMapper
{
    TagMapper(CNCBINode* (C::*method)(void));

    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (C::*m_Method)(void);
};

template<class C>
struct TagMapperByName : BaseTagMapper
{
    TagMapperByName(CNCBINode* (C::*method)(const string& name));

    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (C::*m_Method)(const string& name);
};

#include <nodemap.inl>

END_NCBI_SCOPE

#endif  /* NODEMAP__HPP */
