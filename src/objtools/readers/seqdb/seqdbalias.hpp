#ifndef OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP

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

/// @file seqdbalias.hpp
/// Defines database alias file access classes.
///
/// Defines classes:
///     CSeqDB_AliasWalker
///     CSeqDBAliasNode
///     CSeqDBAliasFile
///
/// Implemented for: UNIX, MS-Windows

#include <iostream>

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfile.hpp"
#include "seqdbvol.hpp"
#include "seqdbvolset.hpp"
#include "seqdbgeneral.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

/// CSeqDBAliasWalker class
/// 
/// Derivatives of this abstract class can be used to gather summary
/// data from the entire include tree of alias files.  For details of
/// the traversal order, see the WalkNodes documentation.

class CSeqDB_AliasWalker {
public:
    /// Destructor
    virtual ~CSeqDB_AliasWalker() {}
    
    /// Override to provide the alias file KEY name for the type of
    /// summary data you want to gather, for example "NSEQ".
    virtual const char * GetFileKey() const = 0;
    
    /// This will be called with each CVolume that is in the alias
    /// file tree structure.
    virtual void Accumulate(const CSeqDBVol &) = 0;
    
    /// This will be called with the value associated with this key in
    /// the alias file.
    virtual void AddString (const string &) = 0;
};

class CSeqDBAliasNode : public CObject {
public:
    CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl);
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void GetVolumeNames(vector<string> & vols) const;
    
    // Compute title by recursive appending of subnodes values until
    // value specification or volume is reached.
    string GetTitle(const CSeqDBVolSet & volset) const;
    
    // Compute sequence count by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint4 GetNumSeqs(const CSeqDBVolSet & volset) const;
    
    // Compute sequence count by recursive appending of subnodes
    // values; this version disables alias file overrides.
    Uint4 GetNumOIDs(const CSeqDBVolSet & volset) const;
    
    // Compute total length by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const;
    
    // Compute total length by recursive appending of subnodes
    // values; this version disables alias file overrides.
    Uint8 GetVolumeLength(const CSeqDBVolSet & volset) const;
    
    // Get the membership bit if there is one.
    Uint4 GetMembBit(const CSeqDBVolSet & volset) const;
    
    void WalkNodes(CSeqDB_AliasWalker * walker,
                   const CSeqDBVolSet & volset) const;
    
    void SetMasks(CSeqDBVolSet & volset);
    
private:
    // To be called only from this class.  Note that the recursion
    // prevention list is passed by value.
    CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                    const string   & dbpath,
                    const string   & dbname,
                    char             prot_nucl,
                    set<string>      recurse,
                    CSeqDBLockHold & locked);
    
    // Actual construction of the node
    // Reads file as a list of values.
    void x_ReadValues(const string & fn, CSeqDBLockHold & locked);
    
    // Reads one line, if it is a value pair it is added to the list.
    void x_ReadLine(const char * bp, const char * ep);
    
    // Expand all DB members as aliases if they exist.
    void x_ExpandAliases(const string   & this_name,
                         char             ending,
                         set<string>    & recurse,
                         CSeqDBLockHold & locked);
    
    string x_MkPath(const string & dir, const string & name, char prot_nucl) const
    {
        return SeqDB_CombinePath(dir, name) + "." + prot_nucl + "al";
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void x_GetVolumeNames(set<string> & vols) const;
    
    void x_ResolveNames(string & dbname_list,
                        string & dbname_path,
                        char     prot_nucl);
    
    /// Add an OID list filter to a volume
    /// 
    /// This method stores the OID mask filename specified in this
    /// alias file in the volume associated with this alias file.  The
    /// specified begin and end will also be applied as an OID range
    /// filter (0 and ULONG_MAX are used to indicate nonexistance of
    /// an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    void x_SetOIDMask(CSeqDBVolSet & volset, Uint4 begin, Uint4 end);

    /// Add a GI list filter to a volume
    /// 
    /// This method stores the GI list filename specified in this
    /// alias file in the volume associated with this alias file.  The
    /// specified begin and end will also be applied as an OID range
    /// filter (0 and ULONG_MAX are used to indicate nonexistance of
    /// an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    void x_SetGiListMask(CSeqDBVolSet & volset, Uint4 begin, Uint4 end);

    /// Add an OID range to a volume
    /// 
    /// This method applies the specified begin and end as an OID
    /// range filter (0 and ULONG_MAX are used to indicate
    /// nonexistance of an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    void x_SetOIDRange(CSeqDBVolSet & volset, Uint4 begin, Uint4 end);
    
    // --- Data ---
    
    typedef map<string, string>             TVarList;
    typedef vector<string>                  TVolNames;
    typedef vector< CRef<CSeqDBAliasNode> > TSubNodeList;
    
    CSeqDBAtlas & m_Atlas;
    string        m_DBPath;
    TVarList      m_Values;
    TVolNames     m_VolNames;
    TSubNodeList  m_SubNodes;
};


class CSeqDBAliasFile {
public:
    CSeqDBAliasFile(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl)
        : m_Node (atlas, name_list, prot_nucl)
    {
        m_Node.GetVolumeNames(m_VolumeNames);
    }
    
    const vector<string> & GetVolumeNames() const
    {
        return m_VolumeNames;
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    string GetTitle(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTitle(volset);
    }
    
    // Add our volumes and our subnode's seq counts.
    Uint4 GetNumSeqs(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetNumSeqs(volset);
    }
    
    // Add our volumes and our subnode's seq counts.
    Uint4 GetNumOIDs(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetNumOIDs(volset);
    }
    
    // Add our volumes and our subnode's base lengths.
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTotalLength(volset);
    }
    
    // Add our volumes and our subnode's base lengths.
    Uint8 GetVolumeLength(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetVolumeLength(volset);
    }
    
    // Get the membership bit if there is one.
    Uint4 GetMembBit(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetMembBit(volset);
    }
    
    void SetMasks(CSeqDBVolSet & volset)
    {
        m_Node.SetMasks(volset);
    }
    
private:
    CSeqDBAliasNode m_Node;
    vector<string>  m_VolumeNames;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP


