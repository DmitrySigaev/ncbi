#ifndef OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP
#define OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */


#include <corelib/ncbimtx.hpp>
#include <objmgr/data_loader.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CID1Client;

class NCBI_XLOADER_TRACE_EXPORT CTraceChromatogramLoader : public CDataLoader
{
public:
    static CTraceChromatogramLoader* RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CTraceChromatogramLoader();

    ~CTraceChromatogramLoader();

    void GetRecords(const CSeq_id_Handle& idh, EChoice choice);

private:
    CTraceChromatogramLoader(const string& loader_name);

    // cached map of resolved chromatograms
    // here, the int is the trace ID, or 'ti'
    typedef map<int, CRef<CSeq_entry> > TTraceEntries;
    TTraceEntries m_Entries;

    // client for data retrieval
    CRef<CID1Client> m_Client;

    // mutex guarding input into the map
    CMutex m_Mutex;


    // retrieve our ID1 client
    CID1Client& x_GetClient();

    // forbidden
    CTraceChromatogramLoader(CTraceChromatogramLoader&);
    CTraceChromatogramLoader& operator=(CTraceChromatogramLoader&);
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.1  2004/03/25 14:19:59  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // OBJTOOLS_DATA_LOADERS_TRACE___TRACE_CHGR__HPP
