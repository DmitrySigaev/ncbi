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

/// @file seqdbalias.cpp
/// Implementation for CSeqDBAliasFile and several related classes,
/// which manage a hierarchical tree of alias file data.

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>

#include "seqdbalias.hpp"
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// Public Constructor
//
// This is the user-visible constructor, which builds the top level
// node in the dbalias node tree.  This design effectively treats the
// user-input database list as if it were an alias file containing
// only the DBLIST specification.

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                                 const string   & dbname_list,
                                 char             prot_nucl)
    : m_Atlas(atlas)
{
    CSeqDBLockHold locked(atlas);
    
    string new_names(dbname_list);
    x_ResolveNames(new_names, m_DBPath, prot_nucl);
    
    set<string> recurse;
    
    if (seqdb_debug_class & debug_alias) {
        cout << "user list((" << dbname_list << "))<>";
    }
    
    m_Values["DBLIST"] = new_names;
    
    x_ExpandAliases("-", prot_nucl, recurse, locked);
    
    m_Atlas.Unlock(locked);
}

// Private Constructor
// 
// This is the constructor for nodes other than the top-level node.
// As such it is private and only called from this class.
// 
// This constructor constructs subnodes by calling x_ExpandAliases,
// which calls this constructor again with the subnode's arguments.
// But no node should be its own ancestor.  To prevent this kind of
// recursive loop, each file adds its full path to a set of strings
// and does not create a subnode for any path already in that set.
// 
// The set (recurse) is passed BY VALUE so that two branches of the
// same file can contain equivalent nodes.  A more efficient method
// for allowing this kind of sharing might be to pass by reference,
// removing the current node path from the set after construction.

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                                 const string   & dbpath,
                                 const string   & dbname,
                                 char             prot_nucl,
                                 set<string>      recurse,
                                 CSeqDBLockHold & locked)
    : m_Atlas(atlas),
      m_DBPath(dbpath)
{
    if (seqdb_debug_class & debug_alias) {
        bool comma = false;
        
        cout << dbname << "<";
        for(set<string>::iterator i = recurse.begin(); i != recurse.end(); i++) {
            if (comma) {
                cout << ",";
            }
            comma = true;
            cout << SeqDB_GetFileName(*i);
        }
        cout << ">";
    }
    
    string full_filename( x_MkPath(m_DBPath, dbname, prot_nucl) );
    recurse.insert(full_filename);
    
    x_ReadValues(full_filename, locked);
    x_ExpandAliases(dbname, prot_nucl, recurse, locked);
}

// This takes the names in dbname_list, finds the path for each name,
// and recreates a space delimited version.  This is only done during
// topmost node construction; names supplied by the end user get this
// treatment, lower level nodes still need absolute or relative paths
// to specify the database locations.
// 
// After each name is resolved, the largest prefix is found and moved
// to the m_DBPath variable.
// 
// [I'm not sure if this is really worth while; it seemed like it
// would be and it wasn't too bad to write.  It could probably be
// omitted in the cliff notes version. -kmb]

void CSeqDBAliasNode::x_ResolveNames(string & dbname_list,
                                     string & dbname_path,
                                     char     prot_nucl)
{
    dbname_path = ".";
    
    vector<string> namevec;
    NStr::Tokenize(dbname_list, " ", namevec, NStr::eMergeDelims);
    
    Uint4 i = 0;
    
    for(i = 0; i < namevec.size(); i++) {
        string component = namevec[i];
        string search_path;
        
        namevec[i] =
            SeqDB_FindBlastDBPath(namevec[i], prot_nucl, & search_path, false);
        
        if (namevec[i].empty()) {
            string msg("No alias or index file found for component [");
            msg += component + "] in search path [" + search_path + "]";
            
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       msg);
        }
    }
    
    // Everything from here depends on namevec[0] existing.
    if (namevec.empty())
        return;
    
    Uint4 common = namevec[0].size();
    
    // Reduce common length to length of min db path.
    
    for(i = 1; common && (i < namevec.size()); i++) {
        if (namevec[i].size() < common) {
            common = namevec.size();
        }
    }
    
    if (common) {
        --common;
    }
    
    // Reduce common length to largest universal prefix.
    
    string & first = namevec[0];
    
    for(i = 1; common && (i < namevec.size()); i++) {
        // Reduce common prefix length until match is found.
        
        while(string(first, 0, common) != string(namevec[i], 0, common)) {
            --common;
        }
    }
    
    // Adjust back to whole path component.
    
    while(common && (first[common] != CFile::GetPathSeparator())) {
        --common;
    }
    
    if (common) {
        // Factor out common path components.
        
        dbname_path.assign(first, 0, common);
        
        for(i = 0; i < namevec.size(); i++) {
            namevec[i].erase(0, common+1);
        }
    }
    
    dbname_list = namevec[0];
    
    for(i = 1; i < namevec.size(); i++) {
        dbname_list += ' ';
        dbname_list += namevec[i];
    }
}

void CSeqDBAliasNode::x_ReadLine(const char * bp,
                                 const char * ep)
{
    const char * p = bp;
    
    // If first nonspace char is '#', line is a comment, so skip.
    if (*p == '#') {
        return;
    }
    
    // Find name
    const char * spacep = p;
    
    while((spacep < ep) && ((*spacep != ' ') && (*spacep != '\t')))
        spacep ++;
    
    string name(p, spacep);
    
    // Find value
    while((spacep < ep) && ((*spacep == ' ') || (*spacep == '\t')))
        spacep ++;
    
    string value(spacep, ep);
    
    for(Uint4 i = 0; i<value.size(); i++) {
        if (value[i] == '\t') {
            value[i] = ' ';
        }
    }
    
    // Store in this nodes' dictionary.
    m_Values[name] = value;
}

void CSeqDBAliasNode::x_ReadValues(const string   & fname,
                                   CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);
    
    CSeqDBAtlas::TIndx file_length(0);
    
    const char * bp = m_Atlas.GetFile(fname, file_length, locked);
    const char * ep = bp + file_length;
    const char * p  = bp;
    
    // Existence should already be verified.
    _ASSERT(bp);
    
    while(p < ep) {
        // Skip spaces
        while((p < ep) && (*p == ' ')) {
            p++;
        }
        
        const char * eolp = p;
        
        while((eolp < ep) && (*eolp != '\n')) {
            eolp++;
        }
        
        // Non-empty line, so read it.
        if (eolp != p) {
            x_ReadLine(p, eolp);
        }
        
        p = eolp + 1;
    }
    
    m_Atlas.RetRegion(bp);
}

void CSeqDBAliasNode::x_ExpandAliases(const string   & this_name,
                                      char             prot_nucl,
                                      set<string>    & recurse,
                                      CSeqDBLockHold & locked)
{
    vector<string> namevec;
    string dblist( m_Values["DBLIST"] );
    NStr::Tokenize(dblist, " ", namevec, NStr::eMergeDelims);
    
    if (namevec.empty()) {
        string situation;
        
        if (this_name == "-") {
            situation = "passed to CSeqDB::CSeqDB().";
        } else {
            situation = string("found in alias file [") + this_name + "].";
        }
        
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   string("No database names were ") + situation);
    }
    
    bool parens = false;
    
    for(Uint4 i = 0; i<namevec.size(); i++) {
        if (namevec[i] == SeqDB_GetBaseName(this_name)) {
            // If the base name of the alias file is also listed in
            // "dblist", it is assumed to refer to a volume instead of
            // to itself.
            
            m_VolNames.push_back(this_name);
            continue;
        }
        
        string new_db_loc( x_MkPath(m_DBPath, namevec[i], prot_nucl) );
        
        if (recurse.find(new_db_loc) != recurse.end()) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Illegal configuration: DB alias files are mutually recursive.");
        }
        
        if ( CFile(new_db_loc).Exists() ) {
            if (parens == false && seqdb_debug_class & debug_alias) {
                parens = true;
                cout << " {" << endl;
            }
            
            string newpath = SeqDB_GetDirName(new_db_loc);
            string newfile = SeqDB_GetBaseName(new_db_loc);
            
            CRef<CSeqDBAliasNode>
                subnode( new CSeqDBAliasNode(m_Atlas,
                                             newpath,
                                             newfile,
                                             prot_nucl,
                                             recurse,
                                             locked) );
            
            m_SubNodes.push_back(subnode);
        } else {
            // If the name is not found as an alias file, it is
            // considered to be a volume.
            
            m_VolNames.push_back( SeqDB_GetBasePath(new_db_loc) );
        }
    }
    
    if (seqdb_debug_class & debug_alias) {
        if (parens) {
            cout << "}" << endl;
        } else {
            cout << ";" << endl;
        }
    }
}

void CSeqDBAliasNode::GetVolumeNames(vector<string> & vols) const
{
    set<string> volset;
    x_GetVolumeNames(volset);
    
    vols.clear();
    for(set<string>::iterator i = volset.begin(); i != volset.end(); i++) {
        vols.push_back(*i);
    }
    
    // Sort to insure deterministic order.
    sort(vols.begin(), vols.end());
}

void CSeqDBAliasNode::x_GetVolumeNames(set<string> & vols) const
{
    Uint4 i = 0;
    
    for(i = 0; i < m_VolNames.size(); i++) {
        vols.insert(m_VolNames[i]);
    }
    
    for(i = 0; i < m_SubNodes.size(); i++) {
        m_SubNodes[i]->x_GetVolumeNames(vols);
    }
}

class CSeqDB_TitleWalker : public CSeqDB_AliasWalker {
public:
    virtual const char * GetFileKey() const
    {
        return "TITLE";
    }
    
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        AddString( vol.GetTitle() );
    }
    
    virtual void AddString(const string & value)
    {
        SeqDB_JoinDelim(m_Value, value, "; ");
    }
    
    string GetTitle()
    {
        return m_Value;
    }
    
private:
    string m_Value;
};


// A slightly more clever approach (might) track the contributions
// from each volume or alias file and trim the final total by the
// amount of provable overcounting detected.
// 
// Since this is probably impossible in most cases, it is not done.
// The working assumption then is that the specified databases are
// disjoint.  This design should prevent undercounting but allows
// overcounting in some cases.

class CSeqDB_MaxLengthWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_MaxLengthWalker()
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey() const
    {
        // This field is not overrideable.
        
        return "MAX_SEQ_LENGTH";
    }
    
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        Uint4 new_max = vol.GetMaxLength();
        
        if (new_max > m_Value)
            m_Value = new_max;
    }
    
    virtual void AddString(const string & value)
    {
        m_Value = NStr::StringToUInt(value);
    }
    
    Uint4 GetMaxLength()
    {
        return m_Value;
    }
    
private:
    Uint4 m_Value;
};


class CSeqDB_NSeqsWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_NSeqsWalker()
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey() const
    {
        return "NSEQ";
    }
    
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetNumOIDs();
    }
    
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt(value);
    }
    
    Uint4 GetNum() const
    {
        return m_Value;
    }
    
private:
    Uint4 m_Value;
};


class CSeqDB_NOIDsWalker : public CSeqDB_NSeqsWalker {
public:
    virtual const char * GetFileKey() const
    {
        // Override to disable the key.  (The embedded spaces would
        // break the parse, so the following non-key is safe.)
        
        return " no key ";
    }
};


class CSeqDB_TotalLengthWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_TotalLengthWalker()
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey() const
    {
        return "LENGTH";
    }
    
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetVolumeLength();
    }
    
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt8(value);
    }
    
    Uint8 GetLength() const
    {
        return m_Value;
    }
    
private:
    Uint8 m_Value;
};


class CSeqDB_VolumeLengthWalker : public CSeqDB_TotalLengthWalker {
public:
    virtual const char * GetFileKey() const
    {
        // Override to disable the key.  (The embedded spaces would
        // break the parse, so the following non-key is safe.)
        
        return " no key ";
    }
};


class CSeqDB_MembBitWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_MembBitWalker()
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey() const
    {
        return "MEMB_BIT";
    }
    
    virtual void Accumulate(const CSeqDBVol &)
    {
        // Volumes don't have this data, only alias files.
    }
    
    virtual void AddString(const string & value)
    {
        m_Value = NStr::StringToUInt(value);
    }
    
    Uint4 GetMembBit() const
    {
        return m_Value;
    }
    
private:
    Uint4 m_Value;
};


void
CSeqDBAliasNode::WalkNodes(CSeqDB_AliasWalker * walker,
                           const CSeqDBVolSet & volset) const
{
    TVarList::const_iterator iter =
        m_Values.find(walker->GetFileKey());
    
    if (iter != m_Values.end()) {
        walker->AddString( (*iter).second );
        return;
    }
    
    Uint4 i;
    
    for(i = 0; i < m_SubNodes.size(); i++) {
        m_SubNodes[i]->WalkNodes( walker, volset );
    }
    
    // For each volume name, try to find the corresponding volume and
    // call Accumulate.
    
    for(i = 0; i < m_VolNames.size(); i++) {
        if (const CSeqDBVol * vptr = volset.GetVol(m_VolNames[i])) {
            walker->Accumulate( *vptr );
        }
    }
}

void CSeqDBAliasNode::x_SetOIDMask(CSeqDBVolSet & volset, Uint4 begin, Uint4 end)
{
    vector<string> namevec;
    NStr::Tokenize(m_Values["DBLIST"], " ", namevec, NStr::eMergeDelims);
    
    if (namevec.size() != 1) {
        string msg =
            string("Alias file (") + m_DBPath +
            ") uses oid list (" + m_Values["OIDLIST"] +
            ") but has " + NStr::UIntToString(namevec.size())
            + " volumes (" + m_Values["DBLIST"] + ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    
    string vol_path(SeqDB_CombinePath(m_DBPath, namevec[0]));
    string mask_path(SeqDB_CombinePath(m_DBPath, m_Values["OIDLIST"]));
    
    volset.AddMaskedVolume(vol_path, mask_path, begin, end);
}

void CSeqDBAliasNode::x_SetGiListMask(CSeqDBVolSet & volset, Uint4 begin, Uint4 end)
{
    string resolved_gilist;
    
    vector<string> gils;
    NStr::Tokenize(m_Values["GILIST"], " ", gils, NStr::eMergeDelims);
    
    // This enforces our restriction that only one GILIST may be
    // applied to any particular volume.  We do not check if the
    // existing one is the same as what we have...
    
    if (gils.size() != 1) {
        string msg =
            string("Alias file (") + m_DBPath +
            ") has multiple GI lists (" + m_Values["GILIST"] + ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    
    ITERATE(vector<string>, iter, gils) {
        SeqDB_JoinDelim(resolved_gilist,
                        SeqDB_CombinePath(m_DBPath, *iter),
                        " ");
    }
    
    ITERATE(TVolNames, vn, m_VolNames) {
        volset.AddGiListVolume(*vn, resolved_gilist, begin, end);
    }
    
    // This "join" should not be needed - an assignment would be just
    // as good - as long as the multi-gilist exception above is
    // in force.
    
    NON_CONST_ITERATE(TSubNodeList, an, m_SubNodes) {
        SeqDB_JoinDelim((**an).m_Values["GILIST"],
                        resolved_gilist,
                        " ");
    }
    
    NON_CONST_ITERATE(TSubNodeList, an, m_SubNodes) {
        (**an).SetMasks( volset );
    }
}

void CSeqDBAliasNode::x_SetOIDRange(CSeqDBVolSet & volset, Uint4 begin, Uint4 end)
{
    vector<string> namevec;
    NStr::Tokenize(m_Values["DBLIST"], " ", namevec, NStr::eMergeDelims);
    
    if (namevec.size() != 1) {
        string msg =
            string("Alias file (") + m_DBPath + ") uses oid range (" +
            NStr::UIntToString(begin + 1) + "," + NStr::UIntToString(end) +
            ") but has " + NStr::UIntToString(namevec.size())
            + " volumes (" + m_Values["DBLIST"] + ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    
    string vol_path(SeqDB_CombinePath(m_DBPath, namevec[0]));
    
    volset.AddRangedVolume(vol_path, begin, end);
}

void CSeqDBAliasNode::SetMasks(CSeqDBVolSet & volset)
{
    TVarList::iterator gil_iter   = m_Values.find(string("GILIST"));
    TVarList::iterator oid_iter   = m_Values.find(string("OIDLIST"));
    TVarList::iterator db_iter    = m_Values.find(string("DBLIST"));
    TVarList::iterator f_oid_iter = m_Values.find(string("FIRST_OID"));
    TVarList::iterator l_oid_iter = m_Values.find(string("LAST_OID"));
    
    bool filtered = false;
    
    if (db_iter != m_Values.end()) {
        if (oid_iter   != m_Values.end() ||
            gil_iter   != m_Values.end() ||
            f_oid_iter != m_Values.end() ||
            l_oid_iter != m_Values.end()) {
            
            Uint4 first_oid = 0;
            Uint4 last_oid  = ULONG_MAX;
            
            if (f_oid_iter != m_Values.end()) {
                first_oid = NStr::StringToUInt(f_oid_iter->second);
                
                // Starts at one, adjust to zero-indexed.
                if (first_oid)
                    first_oid--;
            }
            
            if (l_oid_iter != m_Values.end()) {
                // Zero indexing and post notation adjustments cancel.
                last_oid = NStr::StringToUInt(l_oid_iter->second);
            }
            
            if (oid_iter != m_Values.end()) {
                x_SetOIDMask(volset, first_oid, last_oid);
                filtered = true;
            }
            
            if (gil_iter != m_Values.end()) {
                x_SetGiListMask(volset, first_oid, last_oid);
                filtered = true;
            }
            
            if (! filtered) {
                x_SetOIDRange(volset, first_oid, last_oid);
                filtered = true;
            }
        }
    }
    
    if (filtered) {
        return;
    }
    
    NON_CONST_ITERATE(TSubNodeList, sn, m_SubNodes) {
        (**sn).SetMasks(volset);
    }
    
    ITERATE(TVolNames, vn, m_VolNames) {
        if (CSeqDBVol * vptr = volset.GetVol(*vn)) {
            // We did NOT find an OIDLIST entry; therefore, any db
            // volumes mentioned here are included unfiltered.
            
            volset.AddFullVolume(vptr->GetVolName());
        }
    }
}

string CSeqDBAliasNode::GetTitle(const CSeqDBVolSet & volset) const
{
    CSeqDB_TitleWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetTitle();
}

Uint4 CSeqDBAliasNode::GetNumSeqs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NSeqsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Uint4 CSeqDBAliasNode::GetNumOIDs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NOIDsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Uint8 CSeqDBAliasNode::GetTotalLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_TotalLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

Uint8 CSeqDBAliasNode::GetVolumeLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_VolumeLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

Uint4 CSeqDBAliasNode::GetMembBit(const CSeqDBVolSet & volset) const
{
    CSeqDB_MembBitWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetMembBit();
}

END_NCBI_SCOPE

