#ifndef OBJECTS_OBJMGR___DATA_LOADER__HPP
#define OBJECTS_OBJMGR___DATA_LOADER__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Data loader base class for object manager
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/seq_id_handle.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
class CSeq_loc;
class CSeq_entry;
class CDataSource;
class CHandleRangeMap;
class CTSE_Info;


////////////////////////////////////////////////////////////////////
//
//  CDataLoader::
//


class NCBI_XOBJMGR_EXPORT CDataLoader : public CObject
{
protected:
    CDataLoader(void);
public:
    CDataLoader(const string& loader_name);
    virtual ~CDataLoader(void);

public:
    enum EChoice {
        eBlob,        // whole seqentry
        eBioseq,      // whole bioseq
        eCore,        // everything except bioseqs & annotations
        eBioseqCore,  // bioseq without seqdata and annotations
        eSequence,    // seq data 
        eFeatures,    // SeqFeatures
        eGraph,       // SeqGraph 
        eAlign,       // SeqAlign 
        eExternal,    // external features
        eAll          // whatever fits location
    };
    
    // Request from a datasource for data specified in "choice".
    // The data loaded will be sent back to the datasource through
    // CDataSource::AppendXXX() methods.
    //### virtual bool GetRecords(const CSeq_loc& loc, EChoice choice) = 0;
    
    typedef CConstRef<CTSE_Info> TTSE_Lock;
    typedef set<TTSE_Lock>       TTSE_LockSet;

    // Request from a datasource using handles and ranges instead of seq-loc
    // The TSEs loaded in this call will be added to the tse_set.
    virtual bool GetRecords(const CHandleRangeMap& hrmap,
                            const EChoice choice) = 0;
    
    // 
    virtual bool DropTSE(const CTSE_Info& tse_info);
    
    // Specify datasource to send loaded data to.
    void SetTargetDataSource(CDataSource& data_source);
    
    string GetName(void) const;
    
    // Resolve TSE conflict
    // *select the best TSE from the set of dead TSEs.
    // *select the live TSE from the list of live TSEs
    //  and mark the others one as dead.
    virtual CConstRef<CTSE_Info> ResolveConflict(const CSeq_id_Handle&,
                                                 const TTSE_LockSet&);
    virtual bool IsLive(const CTSE_Info& tse);

    virtual void GC(void);
    virtual void DebugDump(CDebugDumpContext, unsigned int) const;

protected:
    void SetName(const string& loader_name);
    CDataSource* GetDataSource(void);
    
    const CSeq_id* x_GetSeq_id(const CSeq_id_Handle& handle) const;

private:
    CDataLoader(const CDataLoader&);
    CDataLoader& operator=(const CDataLoader&);

    string       m_Name;
    CDataSource* m_DataSource;
    
    friend class CObjectManager;
};


inline
const CSeq_id* CDataLoader::x_GetSeq_id(const CSeq_id_Handle& handle) const
{
    return handle.x_GetSeqId();
}


END_SCOPE(objects)
END_NCBI_SCOPE



/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.21  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.20  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.19  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.18  2003/05/06 18:54:06  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.17  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.16  2003/04/25 14:23:46  vasilche
* Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
*
* Revision 1.15  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.13  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.12  2002/08/28 17:05:13  vasilche
* Remove virtual inheritance
*
* Revision 1.11  2002/06/30 03:27:38  vakatov
* Get rid of warnings, ident the code, move CVS logs to the end of file
*
* Revision 1.10  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.9  2002/05/14 20:06:23  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.8  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/03/30 19:37:05  kimelman
* gbloader MT test
*
* Revision 1.6  2002/03/20 04:50:34  kimelman
* GB loader added
*
* Revision 1.5  2002/03/18 23:05:18  kimelman
* comments
*
* Revision 1.4  2002/03/18 17:26:32  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.3  2002/03/11 21:10:11  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.2  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___DATA_LOADER__HPP
