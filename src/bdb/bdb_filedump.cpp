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
 * File Description:  BDB File covertion into text.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_filedump.hpp>

BEGIN_NCBI_SCOPE

CBDB_FileDumper::CBDB_FileDumper(const string& col_separator)
: m_ColumnSeparator(col_separator),
  m_PrintNames(ePrintNames),
  m_ValueFormatting(eNoQuotes)
{
}

CBDB_FileDumper::CBDB_FileDumper(const CBDB_FileDumper& fdump)
: m_ColumnSeparator(fdump.m_ColumnSeparator),
  m_PrintNames(fdump.m_PrintNames),
  m_ValueFormatting(fdump.m_ValueFormatting)
{
}

CBDB_FileDumper& CBDB_FileDumper::operator=(const CBDB_FileDumper& fdump)
{
    m_ColumnSeparator = fdump.m_ColumnSeparator;
	m_PrintNames = fdump.m_PrintNames;
	m_ValueFormatting = fdump.m_ValueFormatting;
    return *this;
}

void CBDB_FileDumper::Dump(const string& dump_file_name, CBDB_File& db)
{
    CNcbiOfstream out(dump_file_name.c_str());
    if (!out) {
        string err = "Cannot open text file:";
        err.append(dump_file_name);
        BDB_THROW(eInvalidValue, err);
    }

    Dump(out, db);
}

void CBDB_FileDumper::Dump(CNcbiOstream& out, CBDB_File& db)
{
    // Print values
    CBDB_FileCursor cur(db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
	
    Dump(out, cur);
}

void CBDB_FileDumper::Dump(CNcbiOstream& out, CBDB_FileCursor& cur)
{
    CBDB_File& db = cur.GetDBFile();
	
    const CBDB_BufferManager* key  = db.GetKeyBuffer();
    const CBDB_BufferManager* data = db.GetDataBuffer();
	
    vector<unsigned> key_quote_flags;
    vector<unsigned> data_quote_flags;

    if (key) {
        x_SetQuoteFlags(&key_quote_flags, *key);
    }
    if (data) {
        x_SetQuoteFlags(&data_quote_flags, *data);
    }
	
	
    // Print header
    if (m_PrintNames == ePrintNames) {
        PrintHeader(out, key, data);
    }
	
    while (cur.Fetch() == eBDB_Ok) {
        if (key) {
            x_DumpFields(out, *key, key_quote_flags, true/*key*/);
        }

        if (data) {
            x_DumpFields(out, *data, data_quote_flags, false/*not key*/);
        }
        out << NcbiEndl;
    }
	
}

static const string kNullStr = "NULL";

void CBDB_FileDumper::x_DumpFields(CNcbiOstream&             out, 
                                   const CBDB_BufferManager& bman, 
                                   const vector<unsigned>&   quote_flags, 
                                   bool                      is_key)
{
    for (unsigned i = 0; i < bman.FieldCount(); ++i) {
        const CBDB_Field& fld = bman.GetField(i);
        if (is_key) {
            if (i != 0)
                out << m_ColumnSeparator;
        } else {
            out << m_ColumnSeparator;
        }
        
	unsigned qf = quote_flags[i];
    	if (qf) {
	    out << '"';
    	}
        out << (fld.IsNull() ? kNullStr : fld.GetString());
        
    	if (qf) {    
	    out << '"';
    	}
    }
}        


void CBDB_FileDumper::PrintHeader(CNcbiOstream& out,
		                          const CBDB_BufferManager* key,
				 				  const CBDB_BufferManager* data)
{
    if (key) {
        for (unsigned i = 0; i < key->FieldCount(); ++i) {
            const CBDB_Field& fld = key->GetField(i);
            if (i != 0)
                out << m_ColumnSeparator;
            out << fld.GetName();
        }
    }

    if (data) {
        for (unsigned i = 0; i < data->FieldCount(); ++i) {
            const CBDB_Field& fld = data->GetField(i);
            out << m_ColumnSeparator << fld.GetName();
        }
    }
    out << NcbiEndl;
}		

void CBDB_FileDumper::x_SetQuoteFlags(vector<unsigned>*         flags, 
 		                              const CBDB_BufferManager& bman)
{
    flags->resize(0);
    for (unsigned i = 0; i < bman.FieldCount(); ++i) {
        switch (m_ValueFormatting) {
        case eNoQuotes:
            flags->push_back(0);
            break;
        case eQuoteAll:
            flags->push_back(1);
            break;
        case eQuoteStrings:
            {
            const CBDB_Field& fld = bman.GetField(i);
            const CBDB_FieldStringBase* bstr = 
		            dynamic_cast<const CBDB_FieldStringBase*>(&fld);
            flags->push_back(bstr ? 1 : 0);
            }
            break;
        default:
            _ASSERT(0);
        } // switch

    } // for
	
}
		
		
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/06/21 15:08:46  kuznets
 * file dumper changed to work with cursors
 *
 * Revision 1.3  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/10/28 14:57:13  kuznets
 * Implemeneted field names printing
 *
 * Revision 1.1  2003/10/27 14:18:22  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
