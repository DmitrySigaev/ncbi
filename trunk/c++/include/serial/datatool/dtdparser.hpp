#ifndef DTDPARSER_HPP
#define DTDPARSER_HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   DTD parser
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include <serial/datatool/aparser.hpp>
#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/dtdaux.hpp>
#include <serial/datatool/moduleset.hpp>
#include <map>
#include <stack>

BEGIN_NCBI_SCOPE

class CFileModules;
class CDataModule;
class CDataType;
class CDataMemberContainerType;
class CDataValue;
class CDataMember;
class CEnumDataType;
class CEnumDataTypeValue;



/////////////////////////////////////////////////////////////////////////////
// DTDParser

class DTDParser : public AbstractParser
{
public:
    DTDParser( DTDLexer& lexer);
    virtual ~DTDParser(void);

    AutoPtr<CFileModules> Modules(const string& fileName);

protected:
    AutoPtr<CDataTypeModule> Module(const string& name);

    void BuildDocumentTree(void);

    void BeginElementContent(void);
    void ParseElementContent(const string& name, bool embedded);
    void ConsumeElementContent(DTDElement& node);
    void AddElementContent(DTDElement& node, string& id_name,
                           char separator=0);
    void EndElementContent(DTDElement& node);
    void FixEmbeddedNames(DTDElement& node);

    void BeginEntityContent(void);
    void ParseEntityContent(const string& name);
    void PushEntityLexer(const string& name);
    bool PopEntityLexer(void);

    void BeginAttributesContent(void);
    void ParseAttributesContent(const string& name);
    void ConsumeAttributeContent(DTDElement& node, const string& id_name);
    void ParseEnumeratedList(DTDAttribute& attrib);

    void GenerateDataTree(CDataTypeModule& module);
    void ModuleType(CDataTypeModule& module, const DTDElement& node);
    AutoPtr<CDataType> Type(const DTDElement& node,
                            DTDElement::EOccurrence occ,
                            bool fromInside, bool ignoreAttrib=false);
    CDataType* x_Type(const DTDElement& node,
                      DTDElement::EOccurrence occ,
                      bool fromInside, bool ignoreAttrib=false);
    CDataType* TypesBlock(CDataMemberContainerType* containerType,
                          const DTDElement& node, bool ignoreAttrib=false);
    CDataType* CompositeNode(const DTDElement& node,
                             DTDElement::EOccurrence occ);
    void AddAttributes(AutoPtr<CDataMemberContainerType>& container,
                       const DTDElement& node);
    CDataType* AttribBlock(const DTDElement& node);
    CDataType* x_AttribType(const DTDAttribute& att);
    CDataType* EnumeratedBlock(const DTDAttribute& att,
                               CEnumDataType* enumType);

#ifdef _DEBUG
    void PrintDocumentTree(void);
    void PrintEntities(void);
    void PrintDocumentNode(const string& name, const DTDElement& node);
    void PrintNodeAttributes(const DTDElement& node);
#endif
    map<string,DTDElement> m_MapElement;
    map<string,DTDEntity>  m_MapEntity;
    stack<AbstractLexer*>  m_StackLexer;
    stack<string>          m_StackPath;
};

END_NCBI_SCOPE

#endif // DTDPARSER_HPP


/*
 * ==========================================================================
 * $Log$
 * Revision 1.5  2002/11/26 21:59:37  gouriano
 * added unnamed lists of sequences (or choices) as container elements
 *
 * Revision 1.4  2002/11/14 21:07:10  gouriano
 * added support of XML attribute lists
 *
 * Revision 1.3  2002/10/21 16:10:55  gouriano
 * added parsing of external entities
 *
 * Revision 1.2  2002/10/18 14:35:42  gouriano
 * added parsing of internal parsed entities
 *
 * Revision 1.1  2002/10/15 13:50:15  gouriano
 * DTD lexer and parser. first version
 *
 *
 * ==========================================================================
 */
