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
 * File Description:  BDB library cursor implementations.
 *
 */

#include <bdb/bdb_cursor.hpp>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
// Internal class used by CBDB_FileCursor to represent search
// condition criteries.
//

class CBDB_FC_Condition
{
public:
    // Get field buffer reference (non-const)
    CBDB_BufferManager&       GetBuffer()       { return m_Buf; }
    // Get field buffer reference (const)
    const CBDB_BufferManager& GetBuffer() const { return m_Buf; }

    // Get number of fields assigned to condition
    unsigned int GetFieldsAssigned() const { return m_FieldsAssigned; }

    // +1 increment of number of assigned fields.
    // Return new fields count
    unsigned int IncFieldsAssigned()
    {
        _ASSERT(m_Buf.FieldCount()-1 >= m_FieldsAssigned);
        m_Cursor.ResetFirstFetched();
        return ++m_FieldsAssigned;
    }

    // Return TRUE if all search criteria fileds have been assigned
    bool IsComplete() const
    {
        return m_KeyBuf.FieldCount() == GetFieldsAssigned();
    }

    // Return next unassigned field
    IBDB_FieldConvert& GetUnassignedField()
    {
        _ASSERT(m_FieldsAssigned < m_Buf.FieldCount());
        return m_Cursor.GetFieldConvert(m_Buf, m_FieldsAssigned);
    }

    ~CBDB_FC_Condition() { DeleteFields(m_Buf); }

protected:
    enum EIncompleteVal
    {
        eAssignMinVal,
        eAssignMaxVal
    };

    // Constructor. key_buf - key buffer of the main table
    CBDB_FC_Condition(const CBDB_BufferManager& key_buf,
                      CBDB_FileCursor&          cursor)
        : m_KeyBuf(key_buf),
          m_Cursor(cursor),
          m_FieldsAssigned(0)
    {
        m_Buf.DuplicateStructureFrom(key_buf);
        m_Buf.Construct();
    }

    // Set incomplete (non assigned) fields to min(max) possible values
    void InitUnassignedFields(EIncompleteVal incv)
    {
        if (incv == eAssignMinVal) {
            m_Buf.SetMinVal(m_FieldsAssigned, m_Buf.FieldCount());
        } else {  // eAssignMaxVal
            m_Buf.SetMaxVal(m_FieldsAssigned, m_Buf.FieldCount());
        }
    }

    // Set number of assigned fields to zero
    void ResetUnassigned()
    {
        m_FieldsAssigned = 0;
    }

private:
    CBDB_FC_Condition(const CBDB_FC_Condition&);
    const CBDB_FC_Condition&
               operator= (const CBDB_FC_Condition&);

private:
    // Reference on "parent file" key buffer
    const CBDB_BufferManager&   m_KeyBuf;
    // Reference on parent cursor
    CBDB_FileCursor&            m_Cursor;

    // Field buffer. Correspond to
    CBDB_BufferManager          m_Buf;
    // Number of fields assigned (for multi-segment) prefix searches
    unsigned int                m_FieldsAssigned;

    friend class CBDB_FileCursor;
    friend class CBDB_ConditionHandle;
};


/////////////////////////////////////////////////////////////////////////////
//  CBDB_ConditionHandle::
//



CBDB_ConditionHandle::CBDB_ConditionHandle(CBDB_FC_Condition& cond)
 : m_Condition(cond)
{}


CBDB_ConditionHandle::~CBDB_ConditionHandle()
{
    delete &m_Condition;
}



/////////////////////////////////////////////////////////////////////////////
//  CBDB_FileCursor::
//



CBDB_FileCursor::CBDB_FileCursor(CBDB_File& dbf)
: m_Dbf(dbf),
  From( *(new CBDB_FC_Condition(*dbf.m_KeyBuf, *this))  ),
  To( *(new CBDB_FC_Condition(*dbf.m_KeyBuf, *this)) ),
  m_CondFrom(eFirst),
  m_CondTo(eLast),
  m_FetchDirection(eForward),
  m_FirstFetched(false)
{
    m_DBC = m_Dbf.CreateCursor();
}


CBDB_FileCursor::~CBDB_FileCursor()
{
}


void CBDB_FileCursor::SetCondition(ECondition cond_from, ECondition cond_to)
{
    m_FetchDirection = eForward;

    if (cond_from == eGT  ||  cond_from == eGE) {
        if (cond_to == eGT  ||  cond_to == eGE) {
            cond_to = eNotSet;
        }
    }
    else
    if (cond_from == eLT  ||  cond_from == eLE) {
        if (cond_to == eLT  ||  cond_to  == eLE) {
            cond_to = eNotSet;
        }
        m_FetchDirection = eBackward;
    }
    else
    if (cond_from == eEQ) {
        if (cond_to != eNotSet)
            cond_to = eNotSet;
    }
    else
    if (cond_from == eLast) {
        m_FetchDirection = eBackward;
    }
    else
    if (cond_from == eNotSet) {
        BDB_THROW(eIdxSearch, "Cursor search 'FROM' parameter must be set");
    }

    if (cond_to == eEQ)
        BDB_THROW(eIdxSearch, "Cursor search 'TO' parameter cannot be EQ");

    m_CondFrom     = cond_from;
    m_CondTo       = cond_to;
    m_FirstFetched = false;

    From.m_Condition.ResetUnassigned();
    To.m_Condition.ResetUnassigned();
}


EBDB_ErrCode CBDB_FileCursor::FetchFirst()
{
    m_FirstFetched = true;

    // If cursor from buffer contains not all key fields
    // (prefix search) we set all remaining fields to max.
    // possible value for GT condition
    From.m_Condition.InitUnassignedFields(m_CondFrom == eGT ?
                                 CBDB_FC_Condition::eAssignMaxVal
                                 :
                                 CBDB_FC_Condition::eAssignMinVal);

    m_Dbf.m_KeyBuf->CopyFieldsFrom(From.m_Condition.GetBuffer());


    To.m_Condition.InitUnassignedFields(m_CondTo == eLE ?
                               CBDB_FC_Condition::eAssignMaxVal
                               :
                               CBDB_FC_Condition::eAssignMinVal);

    // Incomplete == search transformed into >= search with incomplete
    // fields set to min
    ECondition cond_from;
    if (m_CondFrom == eEQ  &&  !From.m_Condition.IsComplete()) {
        cond_from = eGE;
    } else {
        cond_from = m_CondFrom;
    }

    unsigned int flag;

    switch ( cond_from ) {
        case eFirst:
            flag = DB_FIRST;       // first record retrieval
            break;
        case eLast:                // last record
            flag = DB_LAST;
            break;
        case eEQ:
            flag = DB_SET;         // precise shot
            break;
        case eGT:
        case eGE:
        case eLT:
        case eLE:
            flag = DB_SET_RANGE;   // permits partial key and range searches
            break;
        default:
            BDB_THROW(eIdxSearch, "Invalid FROM condition type");
    }

    EBDB_ErrCode ret = m_Dbf.ReadCursor(m_DBC, flag);
    if (ret != eBDB_Ok)
        return ret;

    // Berkeley DB does not support "<" ">" conditions, so we need to scroll
    // up or down to reach the interval criteria.
    if (m_CondFrom == eGT) {
        while (m_Dbf.m_KeyBuf->Compare(From.m_Condition.m_Buf) == 0) {
            ret = m_Dbf.ReadCursor(m_DBC, DB_NEXT);
            if (ret != eBDB_Ok)
                return ret;
        }
    }
    else
    if (m_CondFrom == eLT) {
        while (m_Dbf.m_KeyBuf->Compare(From.m_Condition.m_Buf) == 0) {
            ret = m_Dbf.ReadCursor(m_DBC, DB_PREV);
            if (ret != eBDB_Ok)
                return ret;
        }
    }
    else
    if (m_CondFrom == eEQ && !From.m_Condition.IsComplete()) {
        int cmp =
            m_Dbf.m_KeyBuf->Compare(From.m_Condition.GetBuffer(),
                                    From.m_Condition.GetFieldsAssigned());
        if (cmp != 0) {
            return eBDB_NotFound;
        }
    }

    if ( !TestTo() ) {
        ret = eBDB_NotFound;
    }

    return ret;
}



EBDB_ErrCode CBDB_FileCursor::Fetch(EFetchDirection fdir)
{
    if ( !m_FirstFetched )
        return FetchFirst();

    if (fdir == eDefault)
        fdir = m_FetchDirection;

    unsigned int flag = (fdir == eForward) ? DB_NEXT : DB_PREV;
    EBDB_ErrCode ret;

    while (1) {
        ret = m_Dbf.ReadCursor(m_DBC, flag);
        if (ret != eBDB_Ok) {
            ret = eBDB_NotFound;
            break;
        }

        if ( !TestTo() ) {
            ret = eBDB_NotFound;
            break;
        }

        // Check if we have fallen out of the FROM range
        if (m_CondFrom == eEQ) {
            int cmp =
                m_Dbf.m_KeyBuf->Compare(From.m_Condition.GetBuffer(),
                                        From.m_Condition.GetFieldsAssigned());
            if (cmp != 0) {
                ret = eBDB_NotFound;
            }
        }
        break;

    } // while

    if (ret != eBDB_Ok)
    {
        From.m_Condition.ResetUnassigned();
        To.m_Condition.ResetUnassigned();
    }

    return ret;
}



bool CBDB_FileCursor::TestTo() const
{
    switch (m_CondTo) {
        case eEQ:
            return (m_Dbf.m_KeyBuf->Compare(To.m_Condition.GetBuffer()) == 0);
        case eGT:
            return (m_Dbf.m_KeyBuf->Compare(To.m_Condition.GetBuffer()) >  0);
        case eGE:
            return (m_Dbf.m_KeyBuf->Compare(To.m_Condition.GetBuffer()) >= 0);
        case eLT:
            return (m_Dbf.m_KeyBuf->Compare(To.m_Condition.GetBuffer())  < 0);
        case eLE:
            return (m_Dbf.m_KeyBuf->Compare(To.m_Condition.GetBuffer()) <= 0);
        default:
            break;
    }
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//
//  Operators
//


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (int val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetInt(val);
    m_Condition.IncFieldsAssigned();
    return *this;
}


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (unsigned int val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetUint(val);
    m_Condition.IncFieldsAssigned();
    return *this;
}


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (float val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetFloat(val);
    m_Condition.IncFieldsAssigned();
    return *this;
}


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (double val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetDouble(val);
    m_Condition.IncFieldsAssigned();
    return *this;
}


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (const char* val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetString(val);
    m_Condition.IncFieldsAssigned();
    return *this;
}


CBDB_ConditionHandle& CBDB_ConditionHandle::operator<< (const string& val)
{
    IBDB_FieldConvert& cnv = m_Condition.GetUnassignedField();
    cnv.SetString(val.c_str());
    m_Condition.IncFieldsAssigned();
    return *this;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/05/01 13:42:12  kuznets
 * Bug fix
 *
 * Revision 1.4  2003/04/30 20:25:42  kuznets
 * Bug fix
 *
 * Revision 1.3  2003/04/29 19:07:22  kuznets
 * Cosmetics..
 *
 * Revision 1.2  2003/04/28 14:51:55  kuznets
 * #include directives changed to conform the NCBI policy
 *
 * Revision 1.1  2003/04/24 16:34:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

