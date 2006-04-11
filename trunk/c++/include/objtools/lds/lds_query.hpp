#ifndef LDS_QUERY_HPP__
#define LDS_QUERY_HPP__
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
 * File Description: Different query functions to LDS database.
 *
 */

#include <bdb/bdb_cursor.hpp>

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_object.hpp>
#include <objmgr/util/obj_sniff.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SLDS_SeqIdBase;
class CLDS_Database;

//////////////////////////////////////////////////////////////////
///
/// CLDS_Query different queries to the LDS database.
///

class NCBI_LDS_EXPORT CLDS_Query
{
public:
    CLDS_Query(CLDS_Database& db);

    /// Scan the database, find the file, return TRUE if file exists.
    /// Linear scan, no idx optimization.
    bool FindFile(const string& path);


    /// Utility class to search sequences
    class NCBI_LDS_EXPORT CSequenceFinder
    {
    public:
        CSequenceFinder(CLDS_Query& query);
        ~CSequenceFinder();

        /// Find sequence
        void Find(const string&   seqid, 
                  CLDS_Set*       obj_ids);

        /// Do sequence screening, new candidates are added(sic!)
        /// to the internal candidate set
        ///
        /// @sa GetCandidates()
        ///
        void Screen(const string&   seqid);

        void Screen(const SLDS_SeqIdBase& sbase);

        /// Find sequences using pre-screened candidates
        void FindInCandidates(const vector<string>& seqids, 
                              CLDS_Set*             obj_ids);

        void FindInCandidates(const string& seqid, 
                              CLDS_Set*     obj_ids);

        const CLDS_Set& GetCandidates() const { return m_CandidateSet; }
        CLDS_Set& GetCandidates() { return m_CandidateSet; }

    private:
        CSequenceFinder(const CSequenceFinder& finder);
        CSequenceFinder& operator=(const CSequenceFinder& finder);
    private:
        CLDS_Query&     m_Query;
        CBDB_FileCursor m_CurInt_idx;
        CBDB_FileCursor m_CurTxt_idx;
        CRef<CSeq_id>   m_SeqId;
        CLDS_Set        m_CandidateSet;
        SLDS_SeqIdBase  m_SBase;
    };


    /// Scan objects database, find sequence, add found ids to
    /// the result set
    ///
    /// @param seqid
    ///    Sequence id to search for
    /// @param obj_ids
    ///    Result set
    /// @param cand_ids
    ///    Candidate result set, based on screening
    ///    (can be NULL)
    /// @param tmp_seqid
    ///    Temporary sequence id used for seqid parsing
    ///    (optional)
    ///
    void FindSequence(const string&   seqid, 
                      CLDS_Set*       obj_ids,
                      CLDS_Set*       cand_ids = 0,
                      CSeq_id*        tmp_seqid = 0,
                      SLDS_SeqIdBase* sbase = 0);

    /// Scan the objects database, search for sequences.
    /// All found ids are added to the obj_ids set.
    void FindSequences(const vector<string>& seqids, 
                       CLDS_Set*             obj_ids);

    /// Non-exact search using sequence id index.
    /// All found ids are added to the result set
    void ScreenSequence(const SLDS_SeqIdBase& sbase, CLDS_Set* obj_ids);

    void ScreenSequence(const SLDS_SeqIdBase&    sbase, 
                        CLDS_Set*                obj_ids,
                        CBDB_FileCursor&         cur_int_idx,
                        CBDB_FileCursor&         cur_txt_idx);

    /// Scan seq_id_list, search for referred sequence ids .
    void FindSeqIdList(const vector<string>& seqids, CLDS_Set* obj_ids);

    /// Scan the objects database, search for sequences
    /// All found ids are added to the obj_ids set.
    void FindSequences(const string& query, CLDS_Set* obj_ids);

    /// Structure describes the indexed object
    struct SObjectDescr
    {
        SObjectDescr() 
		: id(0), is_object(false), format(CFormatGuess::eUnknown), pos(0)  
		{}
        int                     id;
        bool                    is_object;
        string                  type_str;
        CFormatGuess::EFormat   format;
        string                  file_name;
        CNcbiStreampos          pos;
        string                  title;
    };

    /// Return object's description.
    /// SObjectDescr.id == 0 if requested object cannot be found.
    SObjectDescr GetObjectDescr(const map<string, int>& type_map, 
                                int id,
                                bool trace_to_top = false);

    SObjectDescr GetObjectDescr(int id,
                                bool trace_to_top = false);


    /// For a given object scans all parents to find the topmost SeqEntry
    /// (top level bioseq-sets are not taken into account)
    SObjectDescr GetTopSeqEntry(const map<string, int>& type_map, 
                                int id);


    void ReportDuplicateObjectSeqId(const string& seqid, 
                                    int           old_rec_id,
                                    int           new_rec_id);

private:
    /// Fills descr based on current fetched m_db.object_db
    void x_FillDescrObj(SObjectDescr* descr, const map<string, int>& type_map);
    /// Fills descr based on current fetched m_db.annot_db
    void x_FillDescrAnnot(SObjectDescr* descr, const map<string, int>& type_map);

private:
    friend class CSequenceFinder;

private:
    CLDS_Database&         m_DataBase;
    SLDS_TablesCollection& m_db;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2006/04/11 19:14:12  kuznets
 * More careful initialization of SObjectDescr
 *
 * Revision 1.13  2005/11/21 18:51:10  jcherry
 * Added export specifier for CLDS_Query::CSequenceFinder, plus explicit
 * destructor (required for export)
 *
 * Revision 1.12  2005/10/21 14:37:57  ucko
 * Fix friend declaration to refer to CSequenceFinder (which needs it
 * when built with some compilers) rather than CSequenceQuery (which
 * appears not to exist).
 *
 * Revision 1.11  2005/10/20 15:33:46  kuznets
 * Implemented duplicate id check
 *
 * Revision 1.10  2005/10/17 12:19:35  rsmith
 * initialize streampos members.
 *
 * Revision 1.9  2005/10/12 12:18:04  kuznets
 * Use 64-bit file sizes and offsets
 *
 * Revision 1.8  2005/10/06 16:17:15  kuznets
 * Implemented SeqId index
 *
 * Revision 1.7  2005/01/13 17:37:21  kuznets
 * +GetTopSeqEntry
 *
 * Revision 1.6  2004/03/11 18:43:10  kuznets
 * + FindSequences (by a query string)
 *
 * Revision 1.5  2003/08/06 20:47:58  kuznets
 * SObjectDescr receives title field to facilitate structure reuse in gbench UI component)
 *
 * Revision 1.4  2003/07/10 20:09:26  kuznets
 * Implemented GetObjectDescr query. Searches both objects and annotations.
 *
 * Revision 1.3  2003/07/09 19:31:56  kuznets
 * Added query scanning sequence id list.
 *
 * Revision 1.2  2003/06/20 19:55:50  kuznets
 * Implemented new function "FindSequences"
 *
 * Revision 1.1  2003/06/16 14:54:08  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * ===========================================================================
 */

#endif
