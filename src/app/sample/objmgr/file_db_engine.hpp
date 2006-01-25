#ifndef __FILE_DB_ENGINE__HPP
#define __FILE_DB_ENGINE__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>

#include <serial/serialdef.hpp>

#include <objmgr/edits_db_engine.hpp>


#include <string>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFileDBEngine : public IEditsDBEngine
{
public:
    CFileDBEngine(const string& db_path);
    virtual ~CFileDBEngine();

    void CreateBlob(const string& blobid);


    virtual bool HasBlob(const string& blobid) const;
    virtual bool FindSeqId(const CSeq_id_Handle& id, string& blobid) const;
    virtual void NotifyIdsChanged(const TChangedIds&);

    virtual void SaveCommand(const CSeqEdit_Cmd& cmd);
    virtual void GetCommands(const string& blob_id, TCommands& cmds) const;

    void CleanDB();
private:

    string m_DBPath;
    map<string, int> m_CmdCount;
    ESerialDataFormat m_DataFormat;

    TChangedIds m_ChangedIds;
    
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/01/25 19:00:55  didenko
 * Redisigned bio objects edit facility
 *
 * ===========================================================================
 */

#endif // __FILE_DB_ENGINE__HPP
