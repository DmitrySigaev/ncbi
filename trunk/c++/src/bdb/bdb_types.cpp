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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary types implementations.
 *
 */

#include <bdb/bdb_types.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CBDB_BufferManager::
//


CBDB_BufferManager::CBDB_BufferManager()
  : m_BufferSize(0),
    m_PackedSize(0),
    m_Packable(false),
    m_Nullable(false),
    m_NullSetSize(0)
{
}


void CBDB_BufferManager::Bind(CBDB_Field* field, ENullable is_nullable)
{
    m_Fields.push_back(field);
    m_Ptrs.push_back(0);

    unsigned field_idx = m_Fields.size() - 1;
    field->SetBufferIdx(field_idx);

    if ( !m_Packable ) {
        // If we bind any var length field, then record becomes packable
        m_Packable = field->IsVariableLength();
    }

    if (is_nullable == eNullable)
        field->SetNullable();
}


size_t CBDB_BufferManager::ComputeBufferSize() const
{
    size_t buf_len = 0;
    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        const CBDB_Field& field = *m_Fields[i];
        buf_len += field.GetBufferSize();
    }
    return buf_len;
}


void CBDB_BufferManager::CheckNullConstraint() const
{
    if ( !IsNullable() )
        return;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        const CBDB_Field& fld = *m_Fields[i];
        if (!fld.IsNullable()  &&  TestNullBit(i)) {
            string message("NULL field in database operation.");
            const string& field_name = fld.GetName();
            if ( !field_name.empty() ) {
                message.append("(Field:");
                message.append(field_name);
                message.append(")");
            }
            BDB_THROW(eNull, message);
        }
    }
}


void CBDB_BufferManager::Construct()
{
    _ASSERT(m_Fields.size());

    // Buffer construction: fields size calculation.
    m_BufferSize = ComputeBufferSize();

    if ( IsNullable() ) {
        m_NullSetSize = ComputeNullSetSize();
        m_BufferSize += m_NullSetSize;
    }

    m_Buffer = auto_ptr<char>(new char[m_BufferSize]);
    ::memset(m_Buffer.get(), 0, m_BufferSize);

    // Record construction: set element offsets(pointers)
    char*  buf_ptr = (char*) m_Buffer.get();
    buf_ptr += m_NullSetSize;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        m_Ptrs[i] = buf_ptr;

        df.SetBufferManager(this);
        df.SetBuffer(buf_ptr);

        buf_ptr += df.GetBufferSize();
    }

    m_PackedSize = 0;  // not packed
}


void CBDB_BufferManager::ArrangePtrsPacked()
{
    _ASSERT(m_Fields.size());

    if ( !IsPackable() ) {
        m_PackedSize = m_BufferSize;
        return;
    }

    char* buf_ptr = m_Buffer.get();
    buf_ptr += m_NullSetSize;
    m_PackedSize = m_NullSetSize;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        df.SetBuffer(buf_ptr);
        size_t len = df.GetDataLength(buf_ptr);
        buf_ptr += len;
        m_PackedSize += len;
    }
}


unsigned int CBDB_BufferManager::Pack()
{
    _ASSERT(m_Fields.size());
    if (m_PackedSize != 0)
        return m_PackedSize;
    if ( !IsPackable() ) {
        m_PackedSize = m_BufferSize;
        return m_PackedSize;
    }

    char* new_ptr = m_Buffer.get();
    new_ptr += m_NullSetSize;
    m_PackedSize = m_NullSetSize;

    for (size_t i = 0;  i < m_Fields.size();  ++i) {
        CBDB_Field& df = *m_Fields[i];
        size_t actual_len = df.GetLength();
        void* old_ptr = m_Ptrs[i];

        if (new_ptr != old_ptr) {
            ::memmove(new_ptr, old_ptr, actual_len);
            df.SetBuffer(new_ptr);
        }

        if ( m_NullSetSize ) {
            if (df.IsVariableLength()  &&  TestNullBit(i)) {
                actual_len = 1;
                *new_ptr = '\0'; // for string it will guarantee it is empty
            }
        }

        new_ptr      += actual_len;
        m_PackedSize += actual_len;
    }

    return m_PackedSize;
}


unsigned int CBDB_BufferManager::Unpack()
{
    _ASSERT(m_Fields.size());
    if (m_PackedSize == 0)
        return m_BufferSize;
    if ( !IsPackable() ) {
        m_PackedSize = 0;
        return m_PackedSize;
    }

    _ASSERT(!m_Fields.empty());
    for (size_t i = m_Fields.size() - 1;  true;  --i) {
        CBDB_Field& df = *m_Fields[i];
        size_t actual_len = df.GetLength();
        void* new_ptr = m_Ptrs[i];
        const void* old_ptr = df.GetBuffer();
        if (new_ptr != old_ptr) {
            ::memmove(new_ptr, old_ptr, actual_len);
            df.SetBuffer(new_ptr);
        }
        m_PackedSize -= actual_len;

        if (i == 0)
            break;
    }

    m_PackedSize -= m_NullSetSize;

    _ASSERT(m_PackedSize == 0);
    return m_BufferSize;
}


int CBDB_BufferManager::Compare(const CBDB_BufferManager& buf_mgr,
                                unsigned int              field_count)
    const
{
    if ( !field_count ) {
        field_count = FieldCount();
    }
    _ASSERT(field_count <= FieldCount());

    for (unsigned int i = 0;  i < field_count;  ++i) {
        const CBDB_Field& df1 = GetField(i);
        const CBDB_Field& df2 = buf_mgr.GetField(i);

        int ret = df1.CompareWith(df2);
        if ( ret )
            return ret;
    }

    return 0;
}


void CBDB_BufferManager::DuplicateStructureFrom(const CBDB_BufferManager& buf_mgr)
{
    _ASSERT(FieldCount() == 0);
    for (unsigned int i = 0;  i < buf_mgr.FieldCount();  ++i) {
        const CBDB_Field& src_fld = buf_mgr.GetField(i);
        auto_ptr<CBDB_Field> dst_fld(src_fld.Construct(0));
        dst_fld->SetName(src_fld.GetName().c_str());
        Bind(dst_fld.get());
        dst_fld.release();
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/05/02 14:12:11  kuznets
 * Bug fix
 *
 * Revision 1.4  2003/04/29 19:07:22  kuznets
 * Cosmetics..
 *
 * Revision 1.3  2003/04/29 19:04:47  kuznets
 * Fixed a bug in buffers management
 *
 * Revision 1.2  2003/04/28 14:51:55  kuznets
 * #include directives changed to conform the NCBI policy
 *
 * Revision 1.1  2003/04/24 16:34:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
