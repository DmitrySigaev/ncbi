#ifndef READER_PUBSEQ__HPP_INCLUDED
#define READER_PUBSEQ__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from Pubseq_OS
*
*/

#include <corelib/ncbiobj.hpp>
#include <objects/objmgr/reader.hpp>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class I_DriverContext;

BEGIN_SCOPE(objects)


class CPubseqReader;
class CPubseqBlob;


class NCBI_XOBJMGR_EXPORT CPubseqReader : public CReader
{
public:
    CPubseqReader(TConn parallelLevel = 2,
                  const string& server = "PUBSEQ_OS",
                  const string& user="anyone",
                  const string& pswd = "allowed");

    ~CPubseqReader();

    virtual bool RetrieveSeqrefs(TSeqrefs& sr,
                                 const CSeq_id& seqId,
                                 TConn conn = 0);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn);
    CDB_Connection* GetConnection(TConn);
    virtual void Reconnect(TConn);

private:
    bool x_RetrieveSeqrefs(TSeqrefs& sr, const CSeq_id& seqId, TConn conn);

    CDB_Connection *NewConn();
  
    string                    m_Server;
    string                    m_User;
    string                    m_Password;
    auto_ptr<I_DriverContext> m_Context;
    vector<CDB_Connection *>  m_Pool;
};



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.13  2003/04/15 15:30:14  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.12  2003/04/15 14:24:07  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.11  2003/03/26 16:11:06  vasilche
* Removed redundant const modifier from integral return types.
*
* Revision 1.10  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/11 17:40:18  kimelman
* recovery from bad commit
*
* Revision 1.5  2002/04/11 17:32:21  butanaev
* Switched to using dbapi driver manager.
*
* Revision 1.4  2002/04/10 22:47:54  kimelman
* added pubseq_reader as default one
*
* Revision 1.3  2002/04/09 18:48:14  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.2  2002/04/08 23:07:50  vakatov
* #include <vector>
* get rid of the "using ..." directive in the header
*
* Revision 1.1  2002/04/08 20:52:08  butanaev
* Added PUBSEQ reader.
*
* Revision 1.7  2002/04/05 20:53:29  butanaev
* Added code to detect database api availability.
*
* Revision 1.6  2002/04/03 18:37:33  butanaev
* Replaced DBLink with dbapi.
*
* Revision 1.5  2001/12/19 19:42:13  butanaev
* Implemented construction of PUBSEQ blob stream, CPubseqReader family  interfaces.
*
* Revision 1.4  2001/12/14 20:48:08  butanaev
* Implemented fetching Seqrefs from PUBSEQ.
*
* Revision 1.3  2001/12/13 17:50:34  butanaev
* Adjusted for new interface changes.
*
* Revision 1.2  2001/12/12 22:08:58  butanaev
* Removed duplicate code, created frame for CPubseq* implementation.
*
*/

#endif // READER_PUBSEQ__HPP_INCLUDED
