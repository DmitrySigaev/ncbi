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

#include <objtools/data_loaders/table/sage_dload.hpp>
#include <sqlite/sqlite.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objtools/manip/sage_manip.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/handle_range_map.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CSageDataLoader::SIdHandleByContent::operator()(const CSeq_id_Handle& h1,
                                                     const CSeq_id_Handle& h2) const
{
    CConstRef<CSeq_id> id1 = h1.GetSeqId();
    CConstRef<CSeq_id> id2 = h2.GetSeqId();
    return (*id1 < *id2);
}


CSageDataLoader::CSageDataLoader(const string& input_file,
                                 const string& temp_file,
                                 bool delete_file)
    : CDataLoader(input_file)
{
    //
    // create our SQLite DB
    //
    m_Table.Reset(new CSQLiteTable(input_file, temp_file, delete_file));

    //
    // now, store some precalculated info about our table
    //
    {{
        // extract the column names
        list<string> cols;
        m_Table->GetColumnTitles(cols);
        m_Cols.reserve(cols.size());
        std::copy(cols.begin(), cols.end(), back_inserter(m_Cols));
    }}

    // determine our column mapping
    int i = 0;
    m_ColAssign.resize(m_Cols.size(), eUnknown);
    fill(m_ColIdx, m_ColIdx + eMaxKnownCol, -1);

    ITERATE(vector<string>, iter, m_Cols) {
        string str(*iter);
        NStr::ToLower(str);
        if (str == "contig"  ||
            str == "contig_accession"  ||
            str == "accession") {
            m_ColAssign[i] = eAccession;
            m_ColIdx[eAccession] = i;
        } else if (str == "tag_position_on_contig"  ||
                   str == "pos"  ||
                   str == "contig_pos") {
            m_ColAssign[i] = eFrom;
            m_ColIdx[eFrom] = i;
        } else if (str == "tag"  ||
                   str == "catgtag") {
            m_ColAssign[i] = eTag;
            m_ColIdx[eTag] = i;
        } else if (str == "method") {
            m_ColAssign[i] = eMethod;
            m_ColIdx[eMethod] = i;
        } else if (str == "tag_orientation"  ||
                   str == "strand"  ||
                   str == "orientation") {
            m_ColAssign[i] = eStrand;
            m_ColIdx[eStrand] = i;
        }

        ++i;
    }

    string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);

    CSQLite& sqlite = m_Table->SetDB();
    // create an index on accession
    try {
        sqlite.Execute("create index IDX_accession "
                       "on TableData (" + acc_col + ")");
    }
    catch (...) {
        // index already exists - ignored
    }

    // extract a list of the accessions we have
    CRef<CSQLiteQuery> q
        (sqlite.Compile("select distinct " + acc_col +
                        " from TableData order by " + acc_col));

    int count;
    const char** data = NULL;
    const char** cols = NULL;
    while (q->NextRow(count, data, cols)) {
        CRef<CSeq_id> id(new CSeq_id(data[0]));
        if (id->Which() == CSeq_id::e_not_set) {
            LOG_POST(Error << "failed to index id = " << data[0]);
            continue;
        }

        CSeq_id_Handle handle = CSeq_id_Handle::GetHandle(*id);
        m_Ids.insert(TIdMap::value_type(handle, data[0]));
        _TRACE("  id = " << data[0]);
    }

    LOG_POST(Info << "CSageDataLoader: " << m_Ids.size() << " distinct ids");

}


// Request from a datasource using handles and ranges instead of seq-loc
// The TSEs loaded in this call will be added to the tse_set.
void CSageDataLoader::GetRecords(const CSeq_id_Handle& idh,
                                 EChoice choice)
{
    string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);

    //
    // find out if we've already loaded annotations for this seq-id
    //
    TEntries::iterator iter = m_Entries.find(idh);
    if (iter != m_Entries.end()) {
        return;
    }

    //
    // now, find out if this ID is in our list of ids
    //
    pair<TIdMap::iterator, TIdMap::iterator> id_iter = m_Ids.equal_range(idh);
    if (id_iter.first == id_iter.second) {
        return;
    }

    //
    // okay, this ID is correct, so load all features with this id
    //

    string sql("select * from TableData where " + acc_col +
               " in (");
    string tmp;
    for ( ;  id_iter.first != id_iter.second;  ++id_iter.first) {
        TIdMap::iterator iter = id_iter.first;

        if ( !tmp.empty() ) {
            tmp += ", ";
        }
        tmp += "'" + iter->second + "'";
    }
    sql += tmp + ")";

    CSQLiteTable::TIterator row_iter = m_Table->Begin(sql);

    CRef<CSeq_annot> annot(new CSeq_annot());
    vector<string> data;
    for ( ;  *row_iter;  ++(*row_iter)) {
        list<string> temp;
        (*row_iter).GetRow(temp);
        data.resize(temp.size());
        std::copy(temp.begin(), temp.end(), data.begin());


        // create a new feature
        CRef<CSeq_feat> feat(new CSeq_feat());
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId().Assign(*idh.GetSeqId());

        CUser_object& user = feat->SetData().SetUser()
            .SetCategory(CUser_object::eCategory_Experiment);
        CUser_object& experiment =
            user.SetExperiment(CUser_object::eExperiment_Sage);
        CSageData manip(experiment);

        // fill in our columns
        bool neg_strand = false;
        TSeqPos from = 0;
        TSeqPos len  = 0;
        for (int i = 0;  i < data.size();  ++i) {
            switch (m_ColAssign[i]) {
            case eAccession:
                // already handled as ID...
                break;

            case eTag:
                manip.SetTag(data[i]);
                len = data[i].length();
                break;

            case eStrand:
                if (NStr::Compare(data[i], "-") == 0) {
                    neg_strand = true;
                }
                break;

            case eFrom:
                from = NStr::StringToInt(data[i]);
                break;

            case eMethod:
                manip.SetMethod(data[i]);
                break;

            case eUnknown:
            default:
                manip.SetField(m_Cols[i], data[i]);
                break;
            }
        }

        size_t to = from + len;
        if (neg_strand) {
            loc.SetInt().SetStrand(eNa_strand_minus);

            size_t offs = to - from;
            from -= offs;
            to   -= offs;
        } else {
            loc.SetInt().SetStrand(eNa_strand_plus);
        }

        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo  (to);

        annot->SetData().SetFtable().push_back(feat);
    }

    CRef<CSeq_entry> entry;
    if (annot) {
        // we then add the object to the data loader
        // we need to create a dummy TSE for it first
        entry.Reset(new CSeq_entry());
        entry->SetSet().SetSeq_set();
        entry->SetSet().SetAnnot().push_back(annot);
        GetDataSource()->AddTSE(*entry);

        _TRACE("CSageDataLoader(): loaded "
            << annot->GetData().GetFtable().size()
            << " features for " << idh.AsString());
    }

    // we always save an entry here.  If the entry is empty,
    // we have no information about this sequence, but we at
    // least don't need to repeat an expensive search
    m_Entries[idh] = entry;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/11/28 13:41:10  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.3  2003/11/13 21:01:03  dicuccio
 * Altered to support finding multiple accession strings matching a single handle
 *
 * Revision 1.2  2003/10/30 21:43:08  jcherry
 * Fixed typo
 *
 * Revision 1.1  2003/10/02 17:40:17  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
