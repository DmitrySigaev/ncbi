#ifndef OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdboidlist.hpp
/// The SeqDB oid filtering layer.
/// 
/// Defines classes:
///     CSeqDBOIDList
/// 
/// Implemented for: UNIX, MS-Windows

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

/// CSeqDBOIDList
/// 
/// This class defines a set of included oids over the entire oid
/// range.  The underlying implementation is a large bit map.  If the
/// database has one volume, which uses an OID mask file, this object
/// will memory map that file and use it directly.  Otherwise, an area
/// of memory will be allocated (one bit per OID), and the relevant
/// bits will be turned on in that space.  This information may come
/// from memory mapped oid lists, or it may come from GI lists, which
/// are converted to OIDs using ISAM indices.  Because of these two
/// modes of operation, care must be taken to insure that the
/// placement of the bits exactly corresponds to the layout of the
/// memory mappable oid mask files.

class CSeqDBOIDList : public CObject {
public:
    /// A large enough type to span all OIDs.
    typedef Uint4 TOID;
    
    /// Constructor
    /// 
    /// All processing to build the oid mask array is done in the
    /// constructor.  The volumes will be queried for information on
    /// how many and what filter files to apply to each volume, and
    /// these files will be used to build the oid bit array.
    ///
    /// @param atlas
    ///   The CSeqDBAtlas object.
    /// @param volumes
    ///   The set of database volumes.
    CSeqDBOIDList(CSeqDBAtlas & atlas, CSeqDBVolSet & volumes);
    
    /// Destructor
    /// 
    /// All resources will be freed (returned to the atlas).  This
    /// class uses the atlas to get the memory it needs, so the space
    /// for the oid bit array is counted toward the memory bound.
    ~CSeqDBOIDList();
    
    /// Find an included oid from the specified point.
    /// 
    /// This call tests whether the specified oid is included in the
    /// map.  If it is, true is returned and the argument is not
    /// modified.  If it is not included, but a subsequent oid is, the
    /// argument is adjusted to the next included oid, and true is
    /// returned.  If no oids exist from here to the end of the array,
    /// false is returned.
    /// 
    /// @param next_oid
    ///   The oid to check, and also the returned oid.
    /// @return
    ///   True if an oid was found.
    bool CheckOrFindOID(TOID & next_oid) const
    {
        if (x_IsSet(next_oid)) {
            return true;
        }
        return x_FindNext(next_oid);
    }
    
    /// Deallocate the memory ranges owned by this object.
    /// 
    /// This object may hold a lease on a file owned by the atlas.  If
    /// so, this method will release that memory.  It should only be
    /// called during destruction, since this class has no facilities
    /// for reacquiring the memory lease.
    void UnLease()
    {
        m_Lease.Clear();
    }
    
private:
    /// Shorthand type to clarify code that iterates over memory.
    typedef const unsigned char TCUC;
    
    /// Shorthand type to clarify code that iterates over memory.
    typedef unsigned char TUC;
    
    /// Check if a bit is set.
    /// 
    /// Returns true if the specified oid is included.
    ///
    /// @param oid
    ///   The oid to check.
    /// @return
    ///   true if the oid is included.
    inline bool x_IsSet(TOID oid) const;
    
    /// Include the specified oid.
    /// 
    /// Set the inclusion bit for the specified oid to true.
    ///
    /// @param oid
    ///   The oid to adjust.
    void x_SetBit(TOID oid);
    
    /// Find the next OID.
    /// 
    /// This method gets the next included oid.  The semantics are
    /// exactly the same as CheckOrFindOID(), but this method is
    /// always only called if the specified oid was not found by
    /// x_IsSet().  This contains optimizations and should be many
    /// times more efficient that calling CheckOrFindOID() in a loop.
    /// 
    /// @param oid
    ///   The oid to check and possibly adjust
    /// @return
    ///   true if an included oid was found.
    bool x_FindNext(TOID & oid) const;
    
    /// Map a pre-built oid mask from a file.
    /// 
    /// This method maps a file specified by filename, which contains
    /// a pre-built OID bit mask.  The atlas is used to get a lease on
    /// the file, which is held by m_Lease.  This method is only
    /// called when there is one OID mask which spans the entire OID
    /// range.
    /// 
    /// @param filename
    ///   The name of the mask file to use.
    /// @param locked
    ///   The lock hold object for this thread.
    void x_Setup(const string   & filename,
                 CSeqDBLockHold & locked);
    
    /// Build an oid mask in memory.
    /// 
    /// This method allocates an oid bit array which spans the entire
    /// oid range in use.  It then maps all OID mask files and GI list
    /// files.  It copies the bit data from the oid mask files into
    /// this array, translates all GI lists into OIDs and enables the
    /// associated bits, and sets all bits to 1 for any "fully
    /// included" volumes.  This up-front work is intended to make
    /// access to the data as fast as possible later on.  In some
    /// cases, this is not the most efficient way to do this.  Faster
    /// and more efficient storage methods are possible in cases where
    /// very sparse GI lists are used.  More efficient storage is
    /// possible in cases where small masked databases are mixed with
    /// large, "fully-in" volumes.  This is a compromise solution
    /// which may need to be revisited.
    /// 
    /// @param filename
    ///   The name of the mask file to use.
    /// @param locked
    ///   The lock hold object for this thread.
    void x_Setup(CSeqDBVolSet   & volset,
                 CSeqDBLockHold & locked);
    
    void x_OrMaskBits(const string   & mask_fname,
                      Uint4            oid_start,
                      Uint4            oid_end,
                      CSeqDBLockHold & locked);
    
    void x_OrGiFileBits(const string    & gilist_fname,
                        const CSeqDBVol * volp,
                        Uint4             oid_start,
                        Uint4             oid_end,
                        CSeqDBLockHold  & locked);
    
    void x_ReadBinaryGiList(CSeqDBRawFile  & gilist,
                            CSeqDBMemLease & lease,
                            Uint4            start, // 8
                            Uint4            num_gis,
                            vector<Uint4>  & gis,
                            CSeqDBLockHold & locked);
    
    void x_ReadTextGiList(CSeqDBRawFile  & gilist,
                          CSeqDBMemLease & lease,
                          vector<Uint4>  & gis,
                          CSeqDBLockHold & locked);
    
    void x_SetBitRange(Uint4 oid_start, Uint4 oid_end);
    
    // Data
    
    CSeqDBAtlas    & m_Atlas;
    CSeqDBMemLease   m_Lease;
    Uint4            m_NumOIDs;
    
    TUC            * m_Bits;
    TUC            * m_BitEnd;
    
    bool             m_BitOwner;
};

inline bool
CSeqDBOIDList::x_IsSet(TOID oid) const
{
    TCUC * bp = m_Bits + (oid >> 3);
    
    Int4 bitnum = (oid & 7);
    
    if (bp < m_BitEnd) {
        if (*bp & (0x80 >> bitnum)) {
            return true;
        }
    }
    
    return false;
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

