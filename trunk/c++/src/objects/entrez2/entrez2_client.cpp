/* $Id$
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
 * Author:  Josh Cherry
 *
 * File Description:
 *   Entrez2 network client
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'entrez2.asn'.
 */

// standard includes

// generated includes

#include <objects/entrez2/entrez2_client.hpp>

#include <objects/entrez2/Entrez2_db_id.hpp>
#include <objects/entrez2/Entrez2_link_id.hpp>
#include <objects/entrez2/Entrez2_db_id.hpp>
#include <objects/entrez2/Entrez2_id.hpp>
#include <objects/entrez2/Entrez2_get_links.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CEntrez2Client::~CEntrez2Client(void)
{
}


/// A simplified interface for getting neighbors (links)

/// This form just yields a vector of UIDs
void CEntrez2Client::GetNeighbors(int query_uid, const string& db,
                                  const string& link_type,
                                  vector<int>& neighbor_uids)
{
    // first retrieve the link_set
    CRef<CEntrez2_link_set> link_set;
    link_set = GetNeighbors(query_uid, db, link_type);
    
    // then extract the UIDs
    for (CEntrez2_id_list::TConstUidIterator it
             = link_set->GetIds().GetConstUidIterator();  
         !it.AtEnd();  ++it) {
        neighbor_uids.push_back(*it);
    }
}


/// This form returns the entire CEntrez2_link_set object,
/// which includes scores.
CRef<CEntrez2_link_set> CEntrez2Client::GetNeighbors(int query_uid,
                                                     const string& db,
                                                     const string& link_type)
{
    CEntrez2_id_list uids;
    uids.SetDb() = db;
    uids.SetNum(1);
    uids.SetUids().resize(uids.sm_UidSize);
    CEntrez2_id_list::TUidIterator it = uids.GetUidIterator();
    *it = query_uid;
    
    CEntrez2_get_links gl;
    gl.SetUids(uids);
    gl.SetLinktype().Set(link_type);

    CRef<CEntrez2_link_set> link_set = AskGet_links(gl);
    return link_set;
}


/// Retrieve counts of the various types of neighbors available
CRef<CEntrez2_link_count_list>
CEntrez2Client::GetNeighborCounts(int query_uid,
                                  const string& db)
{
    CEntrez2_id uid;
    uid.SetDb() = db;
    uid.SetUid(query_uid);
    return AskGet_link_counts(uid);
}


/// Query a db with a string, returning uids as integers
void CEntrez2Client::Query(const string& query, const string& db,
                           vector<int>& result_uids)
{
    CRef<CEntrez2_boolean_element> bel(new CEntrez2_boolean_element);
    bel->SetStr(query);

    CEntrez2_boolean_exp bexp;
    bexp.SetDb().Set(db);
    bexp.SetExp().push_back(bel);

    CEntrez2_eval_boolean req;
    req.SetReturn_UIDs(true);
    req.SetQuery(bexp);

    CRef<CEntrez2_boolean_reply> reply = AskEval_boolean(req);
    
    // now extract the UIDs
    for (CEntrez2_id_list::TConstUidIterator it
             = reply->GetUids().GetConstUidIterator();  
         !it.AtEnd();  ++it) {
        result_uids.push_back(*it);
    }
}


/// Given some uids, a database, and an entrez query string,
/// determine which of these uids match the query string
void CEntrez2Client::FilterIds(const vector<int> query_uids, const string& db,
                               const string& query_string,
                               vector<int>& result_uids)
{

    if (query_uids.empty()) {
        return;
    }

    string whole_query = '(' + query_string + ')' + " AND " + '(';
    ITERATE (vector<int>, uid, query_uids) {
        if (uid != query_uids.begin()) {
            whole_query += " OR ";
        }
        whole_query += NStr::IntToString(*uid) + "[UID]";
    }
    Query(whole_query, db, result_uids);
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2003/10/16 20:10:23  jcherry
* Added some simplified interfaces for querying
*
* Revision 1.3  2003/10/08 19:56:44  jcherry
* OK, we're back
*
* Revision 1.1  2003/10/08 19:18:01  jcherry
* Added a simplified interface for getting neighbors
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1896, CRC32: cd6a8df4 */
