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
 * File Description: Object manager compliant data loader for local data
 *                   storage (LDS).
 *
 */

#ifndef LDS_DATALOADER_HPP
#define LDS_DATALOADER_HPP

#include <objmgr/data_loader.hpp>

#include <objtools/lds/lds_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLDS_Database;

//////////////////////////////////////////////////////////////////
//
// CLDS_DataLoader.
// CDataLoader implementation for LDS.
//

class NCBI_XLOADER_LDS_EXPORT CLDS_DataLoader : public CDataLoader
{
public:
    // Construct dataloader, attach the external LDS database
    CLDS_DataLoader(CLDS_Database& lds_db);

    // Construct dataloader, with opening its own database
    CLDS_DataLoader(const string& db_path);

    virtual ~CLDS_DataLoader();

    virtual void GetRecords(const CSeq_id_Handle& idh,
                            const EChoice choice);
    
    virtual void DropTSE(const CTSE_Info& tse_info);

private:
    CLDS_Database&      m_LDS_db;        // Reference on the LDS database 
    CLDS_Set            m_LoadedObjects; // Set of already loaded objects
    bool                m_OwnDatabase;   // "TRUE" if datalaoder owns m_LDS_db
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/09/30 16:36:36  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.4  2003/07/30 16:35:38  kuznets
 * Fixed export macro NCBI_XLOADER_LDS_EXPORT
 *
 * Revision 1.3  2003/06/18 18:47:53  kuznets
 * Minor change: LDS Dataloader can now own the LDS database.
 *
 * Revision 1.2  2003/06/18 15:04:05  kuznets
 * Added dll export/import specs.
 *
 * Revision 1.1  2003/06/16 15:47:53  kuznets
 * Initial revision
 *
 *
 */

#endif

