#ifndef NCBI_OBJMGR_SPLIT_CACHE__HPP
#define NCBI_OBJMGR_SPLIT_CACHE__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

#include <memory>
#include <set>

#include <serial/serialdef.hpp>

#include "blob_splitter_params.hpp"

BEGIN_NCBI_SCOPE

class CBDB_BLOB_Cache;

BEGIN_SCOPE(objects)

class CObjectManager;
class CSeq_id;
class CScope;
class CBioseq_Handle;
class CDataLoader;
class CCachedId1Reader;
class CGBDataLoader;
class CSeqref;

/////////////////////////////////////////////////////////////////////////////
//
//  Application class
//


class CLevelGuard
{
public:
    typedef size_t TLevel;
    CLevelGuard(TLevel& level)
        : m_Level(level)
        {
            ++m_Level;
        }
    ~CLevelGuard(void)
        {
            --m_Level;
        }

    operator TLevel(void) const
        {
            return m_Level;
        }

private:
    TLevel& m_Level;
};


#define WAIT_LINE CLog line(this); line
#define LINE(Msg) do { WAIT_LINE << Msg; } while(0)


class CSplitCacheApp : public CNcbiApplication
{
public:
    CSplitCacheApp(void);
    ~CSplitCacheApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void SetupCache(void);
    void Process(void);

    void ProcessSeqId(const CSeq_id& seq_id);
    void ProcessGi(int gi);
    void ProcessBlob(const CSeqref& seqref);

    CNcbiOstream& Info(void) const;

    template<class C>
    void Dump(const C& obj, ESerialDataFormat format,
              const string& key, const string& suffix = kEmptyStr)
    {
        string ext;
        switch ( format ) {
        case eSerial_AsnText:   ext = "asn"; break;
        case eSerial_AsnBinary: ext = "asb"; break;
        case eSerial_Xml:       ext = "xml"; break;
        }
        string file_name = GetFileName(key, suffix, ext);
        WAIT_LINE << "Dumping to " << file_name << " ...";
        AutoPtr<CObjectOStream> out(CObjectOStream::Open(file_name, format));
        *out << obj;
    }

    enum EDataType
    {
        eDataType_MainBlob = 0,
        eDataType_SplitInfo = 1,
        eDataType_Chunk = 2
    };

    template<class C>
    void DumpData(const C& obj, EDataType data_type,
                  const string& key, const string& suffix = kEmptyStr)
    {
        string file_name = GetFileName(key, suffix, "bin");
        WAIT_LINE << "Storing to " << file_name << " ...";
        CSplitDataMaker data(m_SplitterParams, data_type);
        data << obj;
        AutoPtr<CObjectOStream> out
            (CObjectOStream::Open(file_name, eSerial_AsnBinary));
        *out << data.GetData();
    }
    template<class C>
    void StoreToCache(const C& obj, EDataType data_type,
                      const CSeqref& seqref, const string& suffix = kEmptyStr)
    {
        string key = m_Reader->GetBlobKey(seqref) + suffix;
        WAIT_LINE << "Storing to cache " << key << " ...";
        CNcbiOstrstream stream;
        {{
            CSplitDataMaker data(m_SplitterParams, data_type);
            data << obj;
            AutoPtr<CObjectOStream> out
                (CObjectOStream::Open(eSerial_AsnBinary, stream));
            *out << data.GetData();
        }}
        size_t size = stream.pcount();
        line << setiosflags(ios::fixed) << setprecision(2) <<
            " " << setw(7) << (size/1024.0) << " KB";
        const char* data = stream.str();
        stream.freeze(false);
        m_Cache->Store(key, seqref.GetVersion(), data, size);
    }

protected:
    CConstRef<CSeqref> GetSeqref(CBioseq_Handle bh);

    typedef set< pair<int, int> > TProcessed;

private:
    // parameters
    bool m_DumpAsnText;
    bool m_DumpAsnBinary;
    bool m_Resplit;
    SSplitterParams m_SplitterParams;
    
    // splitter loaders/managers
    auto_ptr<CBDB_BLOB_Cache>   m_Cache;
    CCachedId1Reader*           m_Reader;
    CRef<CGBDataLoader>         m_Loader;
    CRef<CObjectManager>        m_ObjMgr;
    CRef<CScope>                m_Scope;

    // splitter process state
    size_t      m_RecursionLevel;
    TProcessed  m_Processed;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/12/02 19:12:24  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.3  2003/11/26 23:05:00  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:03  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:32  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_CACHE__HPP
