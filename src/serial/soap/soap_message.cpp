
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
*   Hold the content of, send and receive SOAP messages
*/

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objostrxml.hpp>
#include <serial/soap/soap_message.hpp>
#include "soap_envelope.hpp"
#include "soap_body.hpp"
#include "soap_header.hpp"
#include "soap_writehook.hpp"
#include "soap_readhook.hpp"
#include <algorithm>

BEGIN_NCBI_SCOPE

const string& CSoapMessage::ms_SoapNamespace =
    "http://www.w3.org/2002/12/soap-envelope";

CSoapMessage::CSoapMessage(void)
    : m_Prefix("env")
{
}

CSoapMessage::~CSoapMessage(void)
{
}

const string& CSoapMessage::GetSoapNamespace(void)
{
    return ms_SoapNamespace;
}

void CSoapMessage::SetSoapNamespacePrefix(const string& prefix)
{
    m_Prefix = prefix;
}

const string& CSoapMessage::GetSoapNamespacePrefix(void) const
{
    return m_Prefix;
}


void CSoapMessage::AddObject(const CSerialObject& obj,
                              EMessagePart eDestination)
{
    if (eDestination == eMsgHeader) {
        m_Header.push_back( CConstRef<CSerialObject>(&obj));
    } else {
        m_Body.push_back( CConstRef<CSerialObject>(&obj));
    }
}


void CSoapMessage::Write(CObjectOStream& out) const
{
    CObjectOStreamXml* os = 0;
    bool schema = false, loc = false;
    ESerialDataFormat fmt = out.GetDataFormat();
    if (fmt == eSerial_Xml) {
        os = dynamic_cast<CObjectOStreamXml*>(&out);
        if (os) {
            schema = os->GetReferenceSchema();
            os->SetReferenceSchema();
            loc = os->GetUseSchemaLocation();
            os->SetUseSchemaLocation(false);
        }
    }

    CSoapEnvelope env;
    env.SetNamespaceName(GetSoapNamespace());
    env.SetNamespacePrefix(GetSoapNamespacePrefix());

    if (!m_Header.empty()) {
// This is to make the stream think the Header was not empty.
// Since Header is optional, we do not have to make it *always*
        CRef<CSoapHeader::C_E> h(new CSoapHeader::C_E);
        h->SetAnyContent(*(new CAnyContentObject));
        env.SetHeader().Set().push_back(h);
    }

// This is to make the stream think the Body was not empty.
// Body is mandatory
    CRef<CSoapBody::C_E> h(new CSoapBody::C_E);
    h->SetAnyContent(*(new CAnyContentObject));
    env.SetBody().Set().push_back(h);

    CObjectTypeInfo typeH = CType<CSoapHeader::C_E>();
    typeH.SetLocalWriteHook(out, new CSoapWriteHook(m_Header));

    CObjectTypeInfo typeB = CType<CSoapBody::C_E>();
    typeB.SetLocalWriteHook(out, new CSoapWriteHook(m_Body));

    out << env;

    if (os) {
        os->SetReferenceSchema(schema);
        os->SetUseSchemaLocation(loc);
    }
}


void CSoapMessage::Read(CObjectIStream& in)
{
    Reset();
    CSoapEnvelope env;

    CObjectTypeInfo typeH = CType<CSoapHeader::C_E>();
    typeH.SetLocalReadHook(in, new CSoapReadHook(m_Header,m_Types));

    CObjectTypeInfo typeB = CType<CSoapBody::C_E>();
    typeB.SetLocalReadHook(in, new CSoapReadHook(m_Body,m_Types));

    in >> env;
}


void CSoapMessage::RegisterObjectType(TTypeInfoGetter typeGetter)
{
    RegisterObjectType(*typeGetter());
}

void CSoapMessage::RegisterObjectType(const CTypeInfo& type)
{
    if (find(m_Types.begin(), m_Types.end(), &type) == m_Types.end()) {
        m_Types.push_back(&type);
    }
}

void CSoapMessage::RegisterObjectType(const CSerialObject& obj)
{
    RegisterObjectType(*obj.GetThisTypeInfo());
}


void CSoapMessage::Reset(void)
{
    m_Header.clear();
    m_Body.clear();
}

const CSoapMessage::TSoapContent&
CSoapMessage::GetContent(EMessagePart eSource) const
{
    if (eSource == eMsgHeader) {
        return m_Header;
    } else {
        return m_Body;
    }
}

CConstRef<CSerialObject>
CSoapMessage::GetSerialObject(const string& typeName,
                               EMessagePart eSource) const
{
    const TSoapContent& src = GetContent(eSource);
    TSoapContent::const_iterator it;
    for (it= src.begin(); it != src.end(); ++it) {
        if ((*it)->GetThisTypeInfo()->GetName() == typeName) {
            return (*it);
        }
    }
    return CConstRef<CSerialObject>(0);
}

CConstRef<CAnyContentObject>
CSoapMessage::GetAnyContentObject(const string& name,
                                   EMessagePart eSource) const
{
    const TSoapContent& src = GetContent(eSource);
    TSoapContent::const_iterator it;
    for (it= src.begin(); it != src.end(); ++it) {
        const CAnyContentObject* obj =
            dynamic_cast<const CAnyContentObject*>(it->GetPointer());
        if (obj && obj->GetName() == name) {
            return CConstRef<CAnyContentObject>(obj);
        }
    }
    return CConstRef<CAnyContentObject>(0);
}


END_NCBI_SCOPE



/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/22 21:00:04  gouriano
* Initial revision
*
*
* ===========================================================================
*/
