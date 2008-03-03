#ifndef RBLOADER_HPP
#define RBLOADER_HPP

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
*  ===========================================================================
*
*  Author: Kevin Bealer
*
*  File Description:
*   Data loader implementation that uses the CRemoteBlast API.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/blastdb/remotedb.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////////
//
// CRemoteBlastDataLoader
//   Data loader implementation that uses the blast databases.
//

// Parameter names used by loader factory

const string kCFParam_RemoteBlast_DbName = "DbName"; // = string
const string kCFParam_RemoteBlast_DbType = "DbType"; // = EDbType (e.g. "Protein")

#define NCBI_XLOADER_REMOTEBLAST_EXPORT NCBI_XLOADER_BLASTDB_EXPORT

class NCBI_XLOADER_REMOTEBLAST_EXPORT CRemoteBlastDataLoader : public CDataLoader
{
    /// The sequence data will sliced into pieces of this size.
    enum {
        kSequenceSliceSize = 65536*2
    };
    
public:

    /// Describes the type of blast database to use
    enum EDbType {
        eNucleotide = 0,    ///< nucleotide database
        eProtein = 1,       ///< protein database
        eUnknown = 2        ///< protein is attempted first, then nucleotide
    };

    struct SBlastDbParam
    {
        SBlastDbParam(const string& db_name = "nr",
                      EDbType       dbtype = eUnknown)
            : m_DbName(db_name), m_DbType(dbtype) {}
        string  m_DbName;
        EDbType m_DbType;
    };

    typedef SRegisterLoaderInfo<CRemoteBlastDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dbname = "nr",
        const EDbType dbtype = eUnknown,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    static string GetLoaderNameFromArgs(const SBlastDbParam& param);

    static string GetLoaderNameFromArgs(const string& dbname = "nr",
                                        const EDbType dbtype = eUnknown)
    {
        return GetLoaderNameFromArgs(SBlastDbParam(dbname, dbtype));
    }
    
    virtual ~CRemoteBlastDataLoader();
    
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;


    /// Load TSE
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    
    /// Load a description or data chunk.
    virtual void GetChunk(TChunk chunk);


    /// Gets the blob id for a given sequence.
    ///
    /// Given a Seq_id_Handle, this method finds the corresponding top
    /// level Seq-entry (TSE) and returns a blob corresponding to it.
    /// The BlobId is initialized with a pointer to that CSeq_entry if
    /// the sequence is known to this data loader, which will be true
    /// if GetRecords() was called for this sequence.
    ///
    /// @param idh
    ///   Indicates the sequence for which to get a blob id.
    /// @return
    ///   A TBlobId corresponding to the provided Seq_id_Handle.
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    
    /// Test method for GetBlobById feature.
    ///
    /// The caller will use this method to determine whether this data
    /// loader allows blobs to be managed by ID.
    ///
    /// @return
    ///   Returns true to indicate that GetBlobById() is available.
    virtual bool CanGetBlobById() const;
    
    /// For a given TBlobId, get the TTSE_Lock.
    ///
    /// If the provided TBlobId is known to this code, the
    /// corresponding TTSE_Lock data will be fetched and returned.
    /// Otherwise, an empty valued TTSE_Lock is returned.
    ///
    /// @param blob_id
    ///   Indicates which data to get.
    /// @return
    ///   The returned data.
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);
    
private:
    /// TPlace is a Seq-id or an integer id, this data loader uses the former.
    typedef CTSE_Chunk_Info::TPlace         TPlace;
    
    /// A list of 'chunk' objects, generic sequence related data elements.
    typedef vector< CRef<CTSE_Chunk_Info> > TChunks;
    
    typedef CParamLoaderMaker<CRemoteBlastDataLoader, SBlastDbParam> TMaker;
    friend class CParamLoaderMaker<CRemoteBlastDataLoader, SBlastDbParam>;

    CRemoteBlastDataLoader(const string&        loader_name,
                           const SBlastDbParam& param);
    
    /// Prevent automatic copy constructor generation
    CRemoteBlastDataLoader(const CRemoteBlastDataLoader &);
    
    /// Prevent automatic assignment operator generation
    CRemoteBlastDataLoader & operator=(const CRemoteBlastDataLoader &);
    
public:    
    
    /// Manages a chunk of sequence data.
    class CSeqChunkData {
    public:
        /// Construct a chunk of sequence data.
        ///
        /// This constructor stores the components needed to build a
        /// range of sequence data (the Seq-literal).  The literal is
        /// not built here, and will not be built until/unless the
        /// user requests the indicated portion of the sequence.
        /// 
        /// @param rdb
        ///   CRemoteDb object to use for this chunk.
        /// @param sih
        ///   Identifies the original sequence to build a piece of.
        /// @param oid
        ///   Locates the sequence within the CSeqDB database.
        /// @param begin
        ///   The coordinate of the start of the region to build.
        /// @param end
        ///   The coordinate of the end of the region to build.
        CSeqChunkData(CRef<CRemoteDb>        rdb,
                      const CSeq_id_Handle & sih,
                      int                    oid,
                      TSeqPos                begin,
                      TSeqPos                end)
            : m_Remote(rdb), m_OID(oid), m_SIH(sih), m_Begin(begin), m_End(end)
        {
        }
        
        /// Default constructor; required by std::map.
        CSeqChunkData()
            : m_Begin(kInvalidSeqPos)
        {
        }
        
        /// Test whether the Seq-literal has already been built.
        /// @return
        ///   true if the literal is present here.
        bool HasLiteral()
        {
            return m_Literal.NotEmpty();
        }
        
        /// Code to actually build the literal.
        void BuildLiteral();
        
        /// Fetch the Seq-literal, which must already exist.
        /// @return
        ///   The sequence literal.
        CRef<CSeq_literal> GetLiteral()
        {
            _ASSERT(m_Literal.NotEmpty());
            return m_Literal;
        }
        
        /// Get the Seq-id-handle identifying this sequence.
        CSeq_id_Handle GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        /// Get the starting offset of this sequence.
        TSeqPos GetPosition()
        {
            return m_Begin;
        }
        
    private:
        /// A reference to the remote database access object.
        CRef<CRemoteDb> m_Remote;
        
        /// Locates the sequence within the CSeqDB database.
        int m_OID;
        
        /// Identifies the original sequence to build a piece of.
        CSeq_id_Handle m_SIH;
        
        /// The coordinate of the start of the region to build.
        TSeqPos m_Begin;
        
        /// The coordinate of the end of the region to build.
        TSeqPos m_End;
        
        /// Holds the sequence literal once it is built.
        CRef<CSeq_literal> m_Literal;
    };
    
    /// A mapping from sequence identifier to blob ids.
    typedef map< CSeq_id_Handle, int > TIds;
    
    int GetOid(const CSeq_id_Handle& idh);
    int GetOid(const TBlobId& blob_id) const;
    
    /// Manages a sequence and its subordinate chunks.
    class CCachedSeqData : public CObject {
    public:
        /// Construct and process the sequence.
        ///
        /// This constructor starts the processing of the specified
        /// sequence.  It loads the bioseq (minus sequence data) and
        /// caches the sequence length.
        ///
        /// @param rdb
        ///   Remote blast database access object.
        /// @param idh
        ///   A handle to the sequence identifier.
        /// @param oid
        ///   Identifies which sequence to get.
        CCachedSeqData(CRemoteDb & rdb, const CSeq_id_Handle & idh, int oid);
        
        /// Get the top-level seq-entry.
        CRef<CSeq_entry> GetTSE()
        {
            return m_TSE;
        }
        
        /// Get the sequence identification.
        CSeq_id_Handle & GetSeqIdHandle()
        {
            return m_SIH;
        }
        
        /// Get the length of the sequence.
        TSeqPos GetLength()
        {
            return m_Length;
        }
        
        /// Build an object representing a range of sequence data.
        ///
        /// An object will be constructed to represent a range of this
        /// sequence's data.  This ids of the objects returned from
        /// this method are stored in this (CCachedSeqData) object.
        ///
        /// @param id
        ///   The chunk id for the new chunk.
        /// @param begin
        ///   The offset into the sequence of the start of this chunk.
        /// @param end
        ///   The offset into the sequence of the end of this chunk.
        /// @return
        ///   An object representing the specified range of sequence data.
        CSeqChunkData BuildDataChunk(int id, TSeqPos begin, TSeqPos end);

        /// Add an empty CDelta_seq to the Seq-entry in m_TSE.
        void AddDelta(TSeqPos begin, TSeqPos end);

        /// Add this sequence's identifiers to a lookup table.
        ///
        /// This method adds identifiers from this sequence to a
        /// lookup table so that the OID of the sequence can be found
        /// quickly during future processing of the same sequence.
        ///
        /// @param idmap
        ///   A map from CSeq_id_Handle to OID.
        void RegisterIds(TIds & idmap);
        
    private:
        /// SeqID handle.
        CSeq_id_Handle m_SIH;
        
        /// Seq entry.
        CRef<CSeq_entry> m_TSE;
        
        /// Sequence (strand) data.
        CRef<CSeq_data> m_SeqData;
        
        /// Sequence length in bases
        TSeqPos m_Length;
        
        /// Remote database interface.
        CRef<CRemoteDb> m_Remote;
        
        /// Identifies this sequence to remote db access objects.
        int m_OID;
    };
    
private:
    /// Load sequence data from cache or from the remote server.
    ///
    /// This checks the OID cache and loads the sequence data from
    /// there or if not found, from the CRemoteBlast interface.  When
    /// new data is built, the sequence is also split into chunks.  A
    /// description of what data is available will be returned in the
    /// "lock" parameter.
    ///
    /// @param idh
    ///   A handle to the sequence identifier.
    /// @param oid
    ///   Object id in BLAST DB
    /// @param lock
    ///   Information about the sequence data is returned here.
    void x_LoadData(const CSeq_id_Handle& idh, int oid, CTSE_LoadLock & lock);
    
    /// Split the sequence data.
    ///
    /// The sequence data is stored and a list of the available ranges
    /// of data is returned via the 'chunks' parameter.  These do not
    /// contain sequence data, but merely indicate what is available.
    ///
    /// @param chunks
    ///   The sequence data chunks will be returned here.
    /// @param seqdata
    ///   Object managing all data pieces for this sequence.
    void x_SplitSeqData(TChunks & chunks, CCachedSeqData & seqdata);
    
    /// Add a chunk of sequence data
    ///
    /// This method builds a description of a specific range of
    /// sequence data, returning it via the 'chunks' parameter.  The
    /// actual data is not built, just a description that identifies
    /// the sequence and the range of that sequence's data represented
    /// by this chunk.
    void x_AddSplitSeqChunk(TChunks&              chunks,
                            const CSeq_id_Handle& id,
                            TSeqPos               begin,
                            TSeqPos               end);
    
    const string    m_DBName;      ///< Blast database name.
    EDbType         m_DBType;      ///< Is this database protein or nucleotide?
    CRef<CRemoteDb> m_Remote;      ///< The remote sequence database API.
    
    TIds            m_Ids;         ///< ID to OID translation.
};

END_SCOPE(objects)


extern NCBI_XLOADER_REMOTEBLAST_EXPORT const string kDataLoader_RemoteBlast_DriverName;

extern "C"
{

NCBI_XLOADER_REMOTEBLAST_EXPORT
void NCBI_EntryPoint_DataLoader_RemoteBlast(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_REMOTEBLAST_EXPORT
void NCBI_EntryPoint_xloader_remoteblast(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif
