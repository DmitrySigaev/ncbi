#ifndef OBJOSTRASN__HPP
#define OBJOSTRASN__HPP

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
* Revision 1.1  1999/06/15 16:20:04  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.2  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.1  1999/06/07 19:30:18  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStreamAsn : public CObjectOStream
{
public:
    typedef unsigned char TByte;
    typedef map<string, TIndex> TStrings;

    CObjectOStreamAsn(CNcbiOstream& out);
    virtual ~CObjectOStreamAsn(void);

    virtual void WriteStd(const char& data);
    virtual void WriteStd(const unsigned char& data);
    virtual void WriteStd(const signed char& data);
    virtual void WriteStd(const short& data);
    virtual void WriteStd(const unsigned short& data);
    virtual void WriteStd(const int& data);
    virtual void WriteStd(const unsigned int& data);
    virtual void WriteStd(const long& data);
    virtual void WriteStd(const unsigned long& data);
    virtual void WriteStd(const float& data);
    virtual void WriteStd(const double& data);
    virtual void WriteStd(const string& data);
    virtual void WriteStd(char* const& data);
    virtual void WriteStd(const char* const& data);

    virtual void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo);

    void WriteNull(void);
    void WriteIndex(TIndex index);
    void WriteSize(unsigned size);
    void WriteString(const string& str);
    void WriteChar(char c);
    void WriteId(const string& str);
    void WriteMemberName(const string& str);

protected:

    virtual void Begin(FixedBlock& block, unsigned size);
    virtual void Next(FixedBlock& block);
    virtual void End(FixedBlock& block);
    virtual void Begin(VarBlock& block);
    virtual void Next(VarBlock& block);
    virtual void End(VarBlock& block);

    void WriteNewLine(void);
    void WriteBlockBegin(void);
    void WriteBlockSeparator(const Block& block);
    void WriteBlockEnd(void);

private:
    CNcbiOstream& m_Output;

    int m_Ident;
};

//#include <serial/objostrasn.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASN__HPP */
