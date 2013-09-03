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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Compound IDs are Base64-encoded string that contain application-specific
 *   information to identify and/or locate objects.
 *
 */

/// @file compound_id.cpp
/// Implementations of CCompoundIDPool, CCompoundID, and CCompoundIDField.

#include <ncbi_pch.hpp>

#include "compound_id_impl.hpp"

#include <connect/ncbi_socket.hpp>

#define CIC_GENERICID_CLASS_NAME "CompoundID"
#define CIC_NETCACHEKEY_CLASS_NAME "NetCacheKey"
#define CIC_NETSCHEDULEKEY_CLASS_NAME "NetScheduleKey"
#define CIC_NETSTORAGEFILEID_CLASS_NAME "NetStorageFileID"

#define CIT_ID_TYPE_NAME "id"
#define CIT_INTEGER_TYPE_NAME "int"
#define CIT_SERVICE_NAME_TYPE_NAME "service"
#define CIT_DATABASE_NAME_TYPE_NAME "database"
#define CIT_TIMESTAMP_TYPE_NAME "time"
#define CIT_RANDOM_TYPE_NAME "rand"
#define CIT_IPV4_ADDRESS_TYPE_NAME "ipv4_addr"
#define CIT_HOST_TYPE_NAME "host"
#define CIT_PORT_TYPE_NAME "port"
#define CIT_IPV4_SOCK_ADDR_TYPE_NAME "ipv4_sock_addr"
#define CIT_PATH_TYPE_NAME "path"
#define CIT_STRING_TYPE_NAME "str"
#define CIT_BOOLEAN_TYPE_NAME "bool"
#define CIT_FLAGS_TYPE_NAME "flags"
#define CIT_TAG_TYPE_NAME "tag"
#define CIT_NUMERIC_TAG_TYPE_NAME "num_tag"
#define CIT_SEQ_ID_TYPE_NAME "seq_id"
#define CIT_TAX_ID_TYPE_NAME "tax_id"
#define CIT_NESTED_CID_TYPE_NAME "nested"

BEGIN_NCBI_SCOPE

static const char* s_ClassNames[eCIC_NumberOfClasses] = {
    /* eCIC_GenericID           */  CIC_GENERICID_CLASS_NAME,
    /* eCIC_NetCacheKey         */  CIC_NETCACHEKEY_CLASS_NAME,
    /* eCIC_NetScheduleKey      */  CIC_NETSCHEDULEKEY_CLASS_NAME,
    /* eCIC_NetStorageFileID    */  CIC_NETSTORAGEFILEID_CLASS_NAME
};

static const char* s_TypeNames[eCIT_NumberOfTypes] = {
    /* eCIT_ID                  */  CIT_ID_TYPE_NAME,
    /* eCIT_Integer             */  CIT_INTEGER_TYPE_NAME,
    /* eCIT_ServiceName         */  CIT_SERVICE_NAME_TYPE_NAME,
    /* eCIT_DatabaseName        */  CIT_DATABASE_NAME_TYPE_NAME,
    /* eCIT_Timestamp           */  CIT_TIMESTAMP_TYPE_NAME,
    /* eCIT_Random              */  CIT_RANDOM_TYPE_NAME,
    /* eCIT_IPv4Address         */  CIT_IPV4_ADDRESS_TYPE_NAME,
    /* eCIT_Host                */  CIT_HOST_TYPE_NAME,
    /* eCIT_Port                */  CIT_PORT_TYPE_NAME,
    /* eCIT_IPv4SockAddr        */  CIT_IPV4_SOCK_ADDR_TYPE_NAME,
    /* eCIT_Path                */  CIT_PATH_TYPE_NAME,
    /* eCIT_String              */  CIT_STRING_TYPE_NAME,
    /* eCIT_Boolean             */  CIT_BOOLEAN_TYPE_NAME,
    /* eCIT_Flags               */  CIT_FLAGS_TYPE_NAME,
    /* eCIT_Tag                 */  CIT_TAG_TYPE_NAME,
    /* eCIT_NumericTag          */  CIT_NUMERIC_TAG_TYPE_NAME,
    /* eCIT_SeqID               */  CIT_SEQ_ID_TYPE_NAME,
    /* eCIT_TaxID               */  CIT_TAX_ID_TYPE_NAME,
    /* eCIT_NestedCID           */  CIT_NESTED_CID_TYPE_NAME
};

const char* CCompoundIDException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidType:
        return "eInvalidType";
    default:
        return CException::GetErrCodeString();
    }
}

void SCompoundIDFieldImpl::DeleteThis()
{
    m_CID = NULL;
}

ECompoundIDFieldType CCompoundIDField::GetType()
{
    return m_Impl->m_Type;
}

CCompoundIDField CCompoundIDField::GetNextNeighbor()
{
    SCompoundIDFieldImpl* next = TFieldList::GetNext(m_Impl);
    next->m_CID = m_Impl->m_CID;
    return next;
}

CCompoundIDField CCompoundIDField::GetNextHomogeneous()
{
    SCompoundIDFieldImpl* next = THomogeneousFieldList::GetNext(m_Impl);
    next->m_CID = m_Impl->m_CID;
    return next;
}

#define CIF_GET_IMPL(ret_type, method, type, member) \
    ret_type CCompoundIDField::method() const \
    { \
        if (m_Impl->m_Type != type) { \
            NCBI_THROW_FMT(CCompoundIDException, eInvalidType, \
                    "Compound ID field type mismatch (requested: " << \
                    s_TypeNames[type] << "; actual: " << \
                    s_TypeNames[m_Impl->m_Type] << ')'); \
        } \
        return (ret_type) m_Impl->member; \
    }

CIF_GET_IMPL(Uint8, GetID, eCIT_ID, m_Uint8Value);
CIF_GET_IMPL(Int8, GetInteger, eCIT_Integer, m_Int8Value);
CIF_GET_IMPL(string, GetServiceName, eCIT_ServiceName, m_StringValue);
CIF_GET_IMPL(string, GetDatabaseName, eCIT_DatabaseName, m_StringValue);
CIF_GET_IMPL(Int8, GetTimestamp, eCIT_Timestamp, m_Int8Value);
CIF_GET_IMPL(Uint4, GetRandom, eCIT_Random, m_Uint4Value);
CIF_GET_IMPL(Uint4, GetIPv4Address, eCIT_IPv4Address &&
        m_Impl->m_Type != eCIT_IPv4SockAddr, m_Uint4Value);
CIF_GET_IMPL(string, GetHost, eCIT_Host, m_StringValue);
CIF_GET_IMPL(Uint2, GetPort, eCIT_Port &&
        m_Impl->m_Type != eCIT_IPv4SockAddr, m_Uint2Value);
CIF_GET_IMPL(string, GetPath, eCIT_Path, m_StringValue);
CIF_GET_IMPL(string, GetString, eCIT_String, m_StringValue);
CIF_GET_IMPL(bool, GetBoolean, eCIT_Boolean, m_BoolValue);
CIF_GET_IMPL(Uint8, GetFlags, eCIT_Flags, m_Uint8Value);
CIF_GET_IMPL(string, GetTag, eCIT_Tag, m_StringValue);
CIF_GET_IMPL(Uint8, GetNumericTag, eCIT_NumericTag, m_Uint8Value);
CIF_GET_IMPL(string, GetSeqID, eCIT_SeqID, m_StringValue);
CIF_GET_IMPL(Uint8, GetTaxID, eCIT_TaxID, m_Uint8Value);
CIF_GET_IMPL(const CCompoundID&, GetNestedCID, eCIT_NestedCID, m_NestedCID);


SCompoundIDFieldImpl* SCompoundIDImpl::AppendField(
        ECompoundIDFieldType field_type)
{
    SCompoundIDFieldImpl* new_entry = m_Pool->m_FieldPool.Alloc();
    m_FieldList.Append(new_entry);
    m_HomogeneousFields[field_type].Append(new_entry);
    ++m_Length;
    return new_entry;
}

void SCompoundIDImpl::DeleteThis()
{
    SCompoundIDFieldImpl* field = m_FieldList.m_Head;
    while (field != NULL) {
        SCompoundIDFieldImpl* next_field = TFieldList::GetNext(field);
        m_Pool->m_FieldPool.ReturnToPool(field);
        field = next_field;
    }
    m_Pool->m_CompoundIDPool.ReturnToPool(this);
    m_Pool = NULL;
}

void CCompoundIDField::Remove()
{
    m_Impl->m_CID->Remove(m_Impl);
    m_Impl->m_CID = NULL;
}

ECompoundIDClass CCompoundID::GetClass() const
{
    return m_Impl->m_Class;
}

bool CCompoundID::IsEmpty() const
{
    return m_Impl->m_Length == 0;
}

unsigned CCompoundID::GetLength() const
{
    return m_Impl->m_Length;
}

CCompoundIDField CCompoundID::GetFirstField()
{
    SCompoundIDFieldImpl* first = m_Impl->m_FieldList.m_Head;
    first->m_CID = m_Impl;
    return first;
}

CCompoundIDField CCompoundID::GetFirst(ECompoundIDFieldType field_type)
{
    _ASSERT(field_type < eCIT_NumberOfTypes);

    SCompoundIDFieldImpl* first =
            m_Impl->m_HomogeneousFields[field_type].m_Head;
    first->m_CID = m_Impl;
    return first;
}

#define CID_APPEND_IMPL(method, type, arg_type) \
    void CCompoundID::method(arg_type value) \
    { \
        m_Impl->AppendField(type)->Reset(type, value); \
    }

CID_APPEND_IMPL(AppendID, eCIT_ID, Uint8);
CID_APPEND_IMPL(AppendInteger, eCIT_Integer, Int8);
CID_APPEND_IMPL(AppendServiceName, eCIT_ServiceName, const string&);
CID_APPEND_IMPL(AppendDatabaseName, eCIT_DatabaseName, const string&);
CID_APPEND_IMPL(AppendTimestamp, eCIT_Timestamp, Int8);

void CCompoundID::AppendCurrentTime()
{
    AppendTimestamp(time(NULL));
}

CID_APPEND_IMPL(AppendRandom, eCIT_Random, Uint4);

void CCompoundID::AppendRandom()
{
    AppendRandom(m_Impl->m_Pool->GetRand());
}

CID_APPEND_IMPL(AppendIPv4Address, eCIT_IPv4Address, Uint4);
CID_APPEND_IMPL(AppendHost, eCIT_Host, const string&);
CID_APPEND_IMPL(AppendPort, eCIT_Port, Uint2);

void CCompoundID::AppendIPv4SockAddr(Uint4 ipv4_address, Uint2 port_number)
{
    SCompoundIDFieldImpl* new_field = m_Impl->AppendField(eCIT_IPv4SockAddr);
    new_field->Reset(eCIT_IPv4SockAddr, ipv4_address);
    new_field->m_Uint2Value = port_number;
}

CID_APPEND_IMPL(AppendPath, eCIT_Path, const string&);
CID_APPEND_IMPL(AppendString, eCIT_String, const string&);
CID_APPEND_IMPL(AppendBoolean, eCIT_Boolean, bool);
CID_APPEND_IMPL(AppendFlags, eCIT_Flags, Uint8);
CID_APPEND_IMPL(AppendTag, eCIT_Tag, const string&);
CID_APPEND_IMPL(AppendNumericTag, eCIT_NumericTag, Uint8);
CID_APPEND_IMPL(AppendSeqID, eCIT_SeqID, const string&);
CID_APPEND_IMPL(AppendTaxID, eCIT_TaxID, Uint8);
CID_APPEND_IMPL(AppendNestedCID, eCIT_NestedCID, const CCompoundID&);

CCompoundIDField CCompoundID::GetLastField()
{
    SCompoundIDFieldImpl* last = m_Impl->m_FieldList.m_Tail;
    last->m_CID = m_Impl;
    return last;
}

string CCompoundID::ToString()
{
    return m_Impl->PackV0();
}

static void s_Indent(CNcbiOstrstream& sstr,
        int indent_depth, const char* indent)
{
    while (--indent_depth >= 0)
        sstr << indent;
}

static void s_DumpCompoundID(CNcbiOstrstream& sstr, SCompoundIDImpl* cid_impl,
        int indent_depth, const char* indent)
{
    sstr << s_ClassNames[cid_impl->m_Class] << '\n';
    s_Indent(sstr, indent_depth, indent);
    sstr << "{\n";

    SCompoundIDFieldImpl* field = cid_impl->m_FieldList.m_Head;

    if (field != NULL)
        for (;;) {
            s_Indent(sstr, indent_depth + 1, indent);
            sstr << s_TypeNames[field->m_Type] << ' ';
            switch (field->m_Type) {
            case eCIT_ID:
            case eCIT_NumericTag:
            case eCIT_TaxID:
                sstr << field->m_Uint8Value;
                break;
            case eCIT_Integer:
            case eCIT_Timestamp:
                sstr << field->m_Int8Value;
                break;
            case eCIT_ServiceName:
            case eCIT_DatabaseName:
            case eCIT_Host:
            case eCIT_Path:
            case eCIT_String:
            case eCIT_Tag:
            case eCIT_SeqID:
                sstr << '"' <<
                        NStr::PrintableString(field->m_StringValue) << '"';
                break;
            case eCIT_Random:
                sstr << field->m_Uint4Value;
                break;
            case eCIT_IPv4Address:
                sstr << CSocketAPI::ntoa(field->m_Uint4Value);
                break;
            case eCIT_Port:
                sstr << field->m_Uint2Value;
                break;
            case eCIT_IPv4SockAddr:
                sstr << CSocketAPI::ntoa(field->m_Uint4Value) <<
                        ':' << field->m_Uint2Value;
                break;
            case eCIT_Boolean:
                sstr << (field->m_BoolValue ? "true" : "false");
                break;
            case eCIT_Flags:
                /* TODO */
                sstr << field->m_Uint8Value;
                break;
            case eCIT_NestedCID:
                s_DumpCompoundID(sstr, field->m_NestedCID,
                        indent_depth + 1, indent);
                break;

            case eCIT_NumberOfTypes:
                _ASSERT(0);
                break;
            }
            field = cid_impl->m_FieldList.GetNext(field);
            if (field != NULL)
                sstr << ",\n";
            else {
                sstr << '\n';
                break;
            }
        }

    s_Indent(sstr, indent_depth, indent);
    sstr << '}';
}

string CCompoundID::Dump()
{
    CNcbiOstrstream sstr;
    s_DumpCompoundID(sstr, m_Impl, 0, "    ");
    sstr << '\n' << NcbiEnds;
    return sstr.str();
}

CCompoundIDPool::CCompoundIDPool() : m_Impl(new SCompoundIDPoolImpl)
{
    m_Impl->m_RandomGen.Randomize();
}

CCompoundID CCompoundIDPool::NewID(ECompoundIDClass new_id_class)
{
    CCompoundID new_cid(m_Impl->m_CompoundIDPool.Alloc());
    new_cid->Reset(m_Impl, new_id_class);
    return new_cid;
}

CCompoundID CCompoundIDPool::FromString(const string& cid)
{
    return m_Impl->UnpackV0(cid);
}

CCompoundID CCompoundIDPool::FromDump(const string& cid_dump)
{
    /* TODO */
    return NULL;
}

#define SCRAMBLE_PASS() \
    pos = seq; \
    counter = seq_len - 1; \
    do { \
        pos[1] ^= *pos ^ length_factor--; \
        ++pos; \
    } while (--counter > 0);

static void s_Scramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = ((unsigned char) seq_len << 1) - 1;
        unsigned char* pos;
        size_t counter;

        SCRAMBLE_PASS();

        *seq ^= *pos ^ length_factor--;

        SCRAMBLE_PASS();
    }
}

void g_PackID(void* binary_id, size_t binary_id_len, string& packed_id)
{
    s_Scramble((unsigned char*) binary_id, binary_id_len);

    size_t packed_id_len;

    base64url_encode(NULL, binary_id_len, NULL, 0, &packed_id_len);

    packed_id.resize(packed_id_len);

    packed_id[0] = '\0';

    base64url_encode(binary_id, binary_id_len,
            const_cast<char*>(packed_id.data()), packed_id_len, NULL);
}

#define UNSCRAMBLE_PASS() \
    counter = seq_len - 1; \
    do { \
        *pos ^= pos[-1] ^ ++length_factor; \
        --pos; \
    } while (--counter > 0);

static void s_Unscramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = 0;
        unsigned char* pos = seq + seq_len - 1;
        size_t counter;

        UNSCRAMBLE_PASS();

        pos = seq + seq_len - 1;

        *seq ^= *pos ^ ++length_factor;

        UNSCRAMBLE_PASS();
    }
}

bool g_UnpackID(const string& packed_id, string& binary_id)
{
    size_t binary_id_len;

    base64url_decode(NULL, packed_id.length(), NULL, 0, &binary_id_len);

    binary_id.resize(binary_id_len);
    binary_id[0] = '\0';

    unsigned char* ptr = (unsigned char*) const_cast<char*>(binary_id.data());

    if (base64url_decode(packed_id.data(), packed_id.length(),
            ptr, binary_id_len, NULL) != eBase64_OK)
        return false;

    s_Unscramble(ptr, binary_id_len);

    return true;
}

END_NCBI_SCOPE
