#if defined(OBJSTACK__HPP)  &&  !defined(OBJSTACK__INL)
#define OBJSTACK__INL

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
* Revision 1.1  2000/05/24 20:08:15  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

inline
void CObjectStackFrame::End(void)
{
    _ASSERT(!m_Ended);
    m_Ended = true;
}

inline
void CObjectStackFrame::Begin(void)
{
    _ASSERT(m_Ended);
    m_Ended = false;
}

inline
void CObjectStackFrame::Push(CObjectStack& stack, EFrameType frameType, bool ended)
{
    m_Previous = stack.m_Top;
    stack.m_Top = this;
    m_FrameType = frameType;
    m_Ended = ended;
}

inline
void CObjectStackFrame::Pop(void)
{
    _ASSERT(m_Stack.m_Top == this);
    m_Stack.m_Top = m_Previous;
}

inline
void CObjectStackFrame::SetNoName(void)
{
    m_NameType = eNameNone;
}

inline
void CObjectStackFrame::SetName(const char* name)
{
    m_NameType = eNameCharPtr;
    m_NameCharPtr = name;
}

inline
void CObjectStackFrame::SetName(char name)
{
    m_NameType = eNameChar;
    m_NameChar = name;
}

inline
void CObjectStackFrame::SetName(const string& name)
{
    m_NameType = eNameString;
    m_NameString = &name;
}

inline
void CObjectStackFrame::SetName(TTypeInfo type)
{
    m_NameType = eNameTypeInfo;
    m_NameTypeInfo = type;
}

inline
void CObjectStackFrame::SetName(const CMemberId& name)
{
    m_NameType = eNameId;
    m_NameId = &name;
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     bool ended)
    : m_Stack(stack)
{
    Push(stack, frameType, ended);
    SetNoName();
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     const string& name)
    : m_Stack(stack)
{
    Push(stack, frameType);
    SetName(name);
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     const char* name)
    : m_Stack(stack)
{
    Push(stack, frameType);
    SetName(name);
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     char name, bool ended)
    : m_Stack(stack)
{
    Push(stack, frameType, ended);
    SetName(name);
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     const CMemberId& name)
    : m_Stack(stack)
{
    Push(stack, frameType);
    SetName(name);
}

inline
CObjectStackFrame::CObjectStackFrame(CObjectStack& stack, EFrameType frameType,
                                     TTypeInfo type)
    : m_Stack(stack)
{
    Push(stack, frameType);
    SetName(type);
}

inline
CObjectStackFrame::~CObjectStackFrame(void)
{
    if ( m_Ended )
        Pop();
    else
        PopUnended();
}

inline
CObjectStackNamedFrame::CObjectStackNamedFrame(CObjectStack& stack,
                                               TTypeInfo type)
    : CObjectStackFrame(stack, eFrameNamed, type)
{
}

inline
CObjectStackBlock::CObjectStackBlock(CObjectStack& stack, EFrameType frameType,
                                     TTypeInfo typeInfo, bool randomOrder)
    : CObjectStackFrame(stack, frameType, typeInfo),
      m_Empty(true), m_RandomOrder(randomOrder)
{
}

inline
CObjectStackArray::CObjectStackArray(CObjectStack& stack,
                                     TTypeInfo arrayType, bool randomOrder)
    : CObjectStackBlock(stack, eFrameArray, arrayType, randomOrder)
{
}

inline
CObjectStackArrayElement::CObjectStackArrayElement(CObjectStackArray& array,
                                                   TTypeInfo elementType)
    : CObjectStackFrame(array, eFrameArrayElement, 'E', true),
      m_ElementType(elementType)
{
}

inline
CObjectStackClass::CObjectStackClass(CObjectStack& stack,
                                     TTypeInfo classInfo, bool randomOrder)
    : CObjectStackBlock(stack, eFrameClass, classInfo, randomOrder)
{
}

inline
CObjectStackClassMember::CObjectStackClassMember(CObjectStackClass& cls)
    : CObjectStackFrame(cls, eFrameClassMember, true)
{
}

inline
CObjectStackClassMember::CObjectStackClassMember(CObjectStackClass& cls,
                                                 const CMemberId& id)
    : CObjectStackFrame(cls, eFrameClassMember, id)
{
}

inline
CObjectStackChoice::CObjectStackChoice(CObjectStack& stack,
                                       TTypeInfo choiceInfo)
    : CObjectStackFrame(stack, eFrameChoice, choiceInfo)
{
}

inline
CObjectStackChoiceVariant::CObjectStackChoiceVariant(CObjectStackChoice& ch)
    : CObjectStackFrame(ch, eFrameChoiceVariant)
{
}

inline
CObjectStackChoiceVariant::CObjectStackChoiceVariant(CObjectStackChoice& ch,
                                                     const CMemberId& id)
    : CObjectStackFrame(ch, eFrameChoiceVariant, id)
{
}

#endif /* def OBJSTACK__HPP  &&  ndef OBJSTACK__INL */
