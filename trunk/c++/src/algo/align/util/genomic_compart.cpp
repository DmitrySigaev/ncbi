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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <algo/align/util/genomic_compart.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool IsIntersecting(const pair<TSeqRange, TSeqRange>& r1,
                    const pair<TSeqRange, TSeqRange>& r2)
{
    bool is_intersecting =
        (r1.first.IntersectingWith(r2.first)  ||
         r1.second.IntersectingWith(r2.second));

    /**
    cerr << "("
        << r1.first << ", "
        << r1.second
        << ")"
        << " x ("
        << r2.first << ", "
        << r2.second
        << ")"
        << ": is_intersecting = "
        << (is_intersecting ? "true" : "false")
        << endl;
        **/

    return is_intersecting;
}

bool IsConsistent(const pair<TSeqRange, TSeqRange>& r1,
                  const pair<TSeqRange, TSeqRange>& r2,
                  ENa_strand s1, ENa_strand s2)
{
    bool is_consistent = false;
    if (s1 == s2) {
        is_consistent = (r1.first < r2.first  &&  r1.second < r2.second)  ||
                        (r2.first < r1.first  &&  r2.second < r1.second);
    }
    else {
        is_consistent = (r1.first < r2.first  &&  r2.second < r1.second)  ||
                        (r2.first < r1.first  &&  r1.second < r2.second);
    }

    /**
    cerr << "("
        << r1.first << ", "
        << r1.second
        << ", " << s1 << ")"
        << " x ("
        << r2.first << ", "
        << r2.second
        << ", " << s2 << ")"
        << ": is_consistent = "
        << (is_consistent ? "true" : "false")
        << endl;
        **/

    return is_consistent;
}


TSeqPos Difference(const pair<TSeqRange, TSeqRange>& r1,
                   const pair<TSeqRange, TSeqRange>& r2,
                   ENa_strand s1, ENa_strand s2)
{
    TSeqPos diff = 0;

    if (r1.first.GetTo() < r2.first.GetFrom()) {
        diff += r2.first.GetFrom() - r1.first.GetTo();
    }
    else if (r2.first.GetTo() < r1.first.GetFrom()) {
        diff += r1.first.GetFrom() - r2.first.GetTo();
    }

    if (r1.second.GetTo() < r2.second.GetFrom()) {
        diff += r2.second.GetFrom() - r1.second.GetTo();
    }
    else if (r2.second.GetTo() < r1.second.GetFrom()) {
        diff += r1.second.GetFrom() - r2.second.GetTo();
    }

    /**
    if (s1 == eNa_strand_minus) {
        if (r2.first.GetTo() < r1.first.GetFrom()) {
            diff += r1.first.GetFrom() - r2.first.GetTo();
        }
    } else {
        if (r1.first.GetTo() < r2.first.GetFrom()) {
            diff += r2.first.GetFrom() - r1.first.GetTo();
        }
    }
    if (s2 == eNa_strand_minus) {
        if (r2.second.GetTo() < r1.second.GetFrom()) {
            diff += r1.second.GetFrom() - r2.second.GetTo();
        }
    } else {
        if (r1.second.GetTo() < r2.second.GetFrom()) {
            diff += r2.second.GetFrom() - r1.second.GetTo();
        }
    }
    **/

    /**
    cerr << "("
        << r1.first << ", "
        << r1.second << ")"
        << " x ("
        << r2.first << ", "
        << r2.second << ")"
        << ": diff = " << diff
        << endl;
        **/

    return diff;
}

typedef pair<TSeqRange, TSeqRange> TRange;
typedef pair<TRange, CRef<CSeq_align> > TAlignRange;

struct SRangesBySize
{
    bool operator() (const TAlignRange& r1,
                     const TAlignRange& r2) const
    {
        TSeqPos len1 = max(r1.first.first.GetLength(),
                           r1.first.second.GetLength());
        TSeqPos len2 = max(r2.first.first.GetLength(),
                           r2.first.second.GetLength());
        return len1 > len2;
    }
};


void FindCompartments(const list< CRef<CSeq_align> >& aligns,
                      list< CRef<CSeq_align_set> >& align_sets,
                      TCompartOptions options)
{
    //
    // sort by sequence pair + strand
    //
    typedef pair<CSeq_id_Handle, ENa_strand> TIdStrand;
    typedef pair<TIdStrand, TIdStrand> TIdPair;
    typedef map<TIdPair, list< CRef<CSeq_align> > > TAlignments;
    TAlignments alignments;
    ITERATE (list< CRef<CSeq_align> >, it, aligns) {
        CSeq_id_Handle qid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(0));
        ENa_strand q_strand = (*it)->GetSeqStrand(0);
        CSeq_id_Handle sid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(1));
        ENa_strand s_strand = (*it)->GetSeqStrand(1);
        TIdPair p(TIdStrand(qid, q_strand), TIdStrand(sid, s_strand));
        alignments[p].push_back(*it);
    }


    typedef pair<TSeqPos, CRef<CSeq_align_set> > TCompartScore;
    vector<TCompartScore> scored_compartments;


    // we only compartmentalize within each sequence id / strand pair
    ITERATE (TAlignments, align_it, alignments) {
        const TIdPair& id_pair = align_it->first;
        ENa_strand q_strand = id_pair.first.second;
        ENa_strand s_strand = id_pair.second.second;

        const list< CRef<CSeq_align> >& aligns = align_it->second;

        //
        // reduce the list to a set of overall ranges
        //
        typedef pair<TSeqRange, TSeqRange> TRange;
        typedef pair<TRange, CRef<CSeq_align> > TAlignRange;

        vector<TAlignRange> align_ranges;

        ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
            TSeqRange q_range = (*iter)->GetSeqRange(0);
            TSeqRange s_range = (*iter)->GetSeqRange(1);
            TRange r(q_range, s_range);
            align_ranges.push_back(TAlignRange(r, *iter));
        }

        /**
        cerr << "ranges: "
            << id_pair.first.first << "/" << q_strand
            << " x "
            << id_pair.second.first << "/" << s_strand
            << ":"<< endl;
        vector<TAlignRange>::const_iterator prev_it = align_ranges.end();
        ITERATE (vector<TAlignRange>, it, align_ranges) {
            cerr << "  ("
                << it->first.first << ", "
                << it->first.second << ")"
                << " [" << it->first.first.GetLength()
                << ", " << it->first.second.GetLength() << "]";
            if (prev_it != align_ranges.end()) {
                cerr << "  consistent="
                    << (IsConsistent(prev_it->first, it->first,
                                     q_strand, s_strand) ? "true" : "false");
                cerr << "  diff="
                    << Difference(prev_it->first, it->first,
                                  q_strand, s_strand);
            }

            cerr << endl;
            prev_it = it;
        }
        **/

        //
        // sort by descending hit size
        // fit each new hit into its best compartment compartment
        //
        std::sort(align_ranges.begin(), align_ranges.end(), SRangesBySize());

        list< multiset<TAlignRange> > compartments;

        // iteration through this list now gives us a natural assortment by
        // largest alignment.  we iterate through this list and inspect the
        // nascent compartments.  we must evaluate for possible fit within each
        // compartment
        NON_CONST_ITERATE (vector<TAlignRange>, it, align_ranges) {

            bool found = false;
            list< multiset<TAlignRange> >::iterator best_compart =
                compartments.end();
            TSeqPos best_diff = kMax_Int;
            NON_CONST_ITERATE (list< multiset<TAlignRange> >,
                               compart_it, compartments) {
                multiset<TAlignRange>::iterator place =
                    compart_it->lower_bound(*it);

                TSeqPos diff = 0;
                bool is_consistent = false;
                bool is_intersecting = false;
                if (place == compart_it->end()) {
                    // best place is the end; we therefore evaluate whether we
                    // can be appended to this compartment
                    --place;
                    is_intersecting = IsIntersecting(place->first,
                                                     it->first);
                    is_consistent = IsConsistent(place->first,
                                                 it->first,
                                                 q_strand, s_strand);
                    diff = Difference(place->first,
                                      it->first,
                                      q_strand, s_strand);
                }
                else {
                    if (place == compart_it->begin()) {
                        // best place is the beginning; we therefore evaluate
                        // whether we can be prepended to this compartment
                        is_intersecting = IsIntersecting(it->first,
                                                         place->first);
                        is_consistent = IsConsistent(it->first,
                                                     place->first,
                                                     q_strand, s_strand);
                        diff = Difference(it->first,
                                          place->first,
                                          q_strand, s_strand);
                    }
                    else {
                        // best place is in the middle; we must evaluate two
                        // positions
                        is_intersecting = IsIntersecting(it->first,
                                                         place->first);
                        is_consistent =
                            IsConsistent(it->first,
                                         place->first,
                                         q_strand, s_strand);
                        diff = Difference(it->first,
                                          place->first,
                                          q_strand, s_strand);

                        --place;
                        is_intersecting |= IsIntersecting(place->first,
                                                          it->first);
                        is_consistent &=
                            IsConsistent(place->first,
                                         it->first,
                                         q_strand, s_strand);
                        diff = min(diff,
                                   Difference(place->first,
                                              it->first,
                                              q_strand, s_strand));
                    }
                }

                if ((is_consistent  ||
                    ( (options & fCompart_AllowIntersections)  &&
                      is_intersecting ))  &&
                    diff < best_diff) {
                    best_compart = compart_it;
                    best_diff = diff;
                    found = true;
                }
            }

            if ( !found  ||  best_compart == compartments.end() ) {
                compartments.push_back(multiset<TAlignRange>());
                compartments.back().insert(*it);
            }
            else {
                best_compart->insert(*it);
            }

            /**
            cerr << "compartments: found " << compartments.size() << endl;
            size_t count = 0;
            ITERATE (list< multiset<TAlignRange> >, it, compartments) {
                ++count;
                cerr << "  compartment " << count << endl;
                ITERATE (multiset<TAlignRange>, i, *it) {
                    cerr << "    ("
                        << i->first.first << ", "
                        << i->first.second << ")"
                        << " [" << i->first.first.GetLength()
                        << ", " << i->first.second.GetLength() << "]"
                        << endl;
                }
            }
            **/
        }

        /**
        {{
             cerr << "found " << compartments.size() << endl;
             size_t count = 0;
             ITERATE (list< multiset<TAlignRange> >, it, compartments) {
                 ++count;
                 cerr << "  compartment " << count << endl;
                 ITERATE (multiset<TAlignRange>, i, *it) {
                     cerr << "    ("
                         << i->first.first << ", "
                         << i->first.second << ")"
                         << " [" << i->first.first.GetLength()
                         << ", " << i->first.second.GetLength() << "]"
                         << endl;
                 }
             }
         }}
         **/

        // pack into seq-align-sets
        ITERATE (list< multiset<TAlignRange> >, it, compartments) {
            CRef<CSeq_align_set> sas(new CSeq_align_set);
            ITERATE (multiset<TAlignRange>, i, *it) {
                sas->Set().push_back(i->second);
            }

            TSeqPos sum = 0;
            ITERATE (CSeq_align_set::Tdata, it, sas->Get()) {
                sum += (*it)->GetAlignLength();
            }

            TCompartScore sc(sum, sas);
            scored_compartments.push_back(sc);
        }
    }

    //
    // sort our compartments by size descending
    //
    std::sort(scored_compartments.begin(), scored_compartments.end());
    NON_CONST_REVERSE_ITERATE (vector<TCompartScore>, it, scored_compartments) {
        align_sets.push_back(it->second);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

