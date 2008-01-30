/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/align/prosplign/prosplign_exception.hpp>
#include <algo/align/prosplign/prosplign.hpp>

#include "Info.hpp"
#include "Ali.hpp"
#include "nucprot.hpp"
#include "NSeq.hpp"
#include "PSeq.hpp"
#include "AliSeqAlign.hpp"

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

const char GAP_CHAR='-'; // used in dna and protein text
const char SPACE_CHAR=' '; // translation and protein
const char INTRON_CHAR='.'; // protein

// used in match text
const char BAD_PIECE_CHAR='X';
const char MISMATCH_CHAR=' ';
const char BAD_OR_MISMATCH[] = {BAD_PIECE_CHAR,MISMATCH_CHAR,0};
const char MATCH_CHAR='|';
const char POSIT_CHAR='+';

CProSplignOutputOptionsExt::CProSplignOutputOptionsExt(const CProSplignOutputOptions& options) : CProSplignOutputOptions(options)
{
    drop = GetTotalPositives() - GetFlankPositives();
    splice_cost = GetFlankPositives()?((100 - GetFlankPositives())*GetMinFlankingExonLen())/GetFlankPositives():0;
}

BEGIN_SCOPE(prosplign)

list<CNPiece> FindGoodParts(const string& orig_match, const string& outp, CProSplignOutputOptionsExt m_options)
{
    list<CNPiece> m_AliPiece;
    //init
    if(m_options.IsPassThrough()) {
        string::size_type n1 = outp.find_first_not_of(GAP_CHAR);
        string::size_type n2 = outp.find_last_not_of(GAP_CHAR);
        if (n1 <= n2) 
        m_AliPiece.push_back(CNPiece(n1, n2+1, 0, 0));
        return m_AliPiece;
    }

    string match = orig_match;
    for (size_t i = 1; i < match.size()-1; ++i) {
        if (isupper(outp[i])) {
            _ASSERT( outp[i-1]==SPACE_CHAR && outp[i+1]==SPACE_CHAR );
            match[i-1] = match[i+1] = match[i];
            ++i;
        }
    }

    string::size_type n = match.find_first_not_of(BAD_OR_MISMATCH);
    if(n == string::npos) return m_AliPiece; //no matches
    bool ism = true;
    bool isintr = false;
    string::size_type beg = n;
    int efflen = 0;
//    if(match[n] == '+' && pcoor[beg] == 1 && toupper(pseq[0]) == 'M') efflen += CNPiece::start_bonus;
//    if(outr[n+1] == 'M' && pcoor[beg] == 1) efflen += CNPiece::start_bonus; THE LAST VERSION
    for(;n<match.size(); ++n) {
        if(match[n] == POSIT_CHAR || match[n] == MATCH_CHAR) {
            if(!ism) {
                ism = true;
                m_AliPiece.push_back(CNPiece(beg, n, 0, efflen));
                beg = n;
                efflen = 0;
            } 
        } else {
            if(ism) {
                    ism = false;
                    m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
                    beg = n;
                    efflen = 0;
            }
        }
        if(outp[n] != INTRON_CHAR) {//inside exon
            efflen ++;  
            isintr = false;
        }
        else {//intron
            if(!isintr) {
                efflen += m_options.splice_cost;
                isintr = true;
            }
        }
    }
    if(ism) {//endpoint is a match
        m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
    }
    //join pieces
    list<CNPiece>::iterator itb, ite, itc;
    list<CNPiece>::size_type pnum = m_AliPiece.size() + 1;
    while(pnum > m_AliPiece.size()) {
        pnum = m_AliPiece.size();
        //forward
        for(itb = m_AliPiece.begin(); ; ) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            ++itc;
            while(itc != m_AliPiece.end()) {
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                ++itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.BackCheck(itb, itc)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
                ++itc;//points to  ' bad' or end
            }
            if(ite != itb) {
                m_options.Join(itb, ite);
                m_AliPiece.erase(itb, ite);
                itb = ite;
            }
            ++itb;
            if(itb == m_AliPiece.end()) break; 
            ++itb;
        }
        //backward
        itb = m_AliPiece.end();
        --itb;
        while(itb != m_AliPiece.begin()) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            while(itc != m_AliPiece.begin()) {
                --itc;//points to  ' bad'
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                --itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.ForwCheck(itc, itb)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
            }
            if(ite != itb) {
                m_options.Join(ite, itb);
                m_AliPiece.erase(ite, itb);
            }
            if(itb == m_AliPiece.begin()) break;
            --itb;
            --itb;
        }
    }
	//throw out bad pieces
    for(list<CNPiece>::iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ) {
        if(it->posit == 0) it = m_AliPiece.erase(it);
        else if (it->efflen < m_options.GetMinGoodLen()) it = m_AliPiece.erase(it);
        else ++it;
    }
    return m_AliPiece;
}


//CNPiece implementation

CNPiece::CNPiece(string::size_type obeg, string::size_type oend, int oposit, int oefflen)
    {
        beg = (int)obeg;
        end = (int)oend;
        posit = oposit;
        efflen = oefflen;
    }
END_SCOPE(prosplign)

bool CProSplignOutputOptionsExt::Bad(list<prosplign::CNPiece>::iterator it)
{
    if(it->efflen > GetMaxBadLen()) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Dropof(int efflen, int posit, list<prosplign::CNPiece>::iterator it)
{
    if((GetTotalPositives()-drop)*(efflen+it->efflen) > 100*(posit+it->posit)) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Perc(list<prosplign::CNPiece>::iterator add, int efflen, int posit, list<prosplign::CNPiece>::iterator cur)
{
    if(Dropof(efflen, posit, add)) return false;
    if(GetTotalPositives()*(efflen+cur->efflen+add->efflen) > 100*(posit+cur->posit+add->posit)) return false;
    return true;
}

void CProSplignOutputOptionsExt::Join(list<prosplign::CNPiece>::iterator it, list<prosplign::CNPiece>::iterator last)
{
    int posit = last->posit;
    int efflen = last->efflen;
    for(list<prosplign::CNPiece>::iterator it1 = it; it1 != last; ++it1) {
        posit += it1->posit;
        efflen += it1->efflen;
    }
    last->posit = posit;
    last->efflen = efflen;
    last->beg = it->beg;
}

bool CProSplignOutputOptionsExt::ForwCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it1->efflen;
    int pos = it1->posit;
    for(;it1!= it2;){
        it1++;
        if(Dropof(efflen, pos, it1)) return false;
        efflen += it1->efflen;
        pos += it1->posit;
        it1++;
        efflen += it1->efflen;
        pos += it1->posit;
    }
    return true;
}

bool CProSplignOutputOptionsExt::BackCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it2->efflen;
    int pos = it2->posit;
    for(;it1!= it2;){
        it2--;
        if(Dropof(efflen, pos, it2)) return false;
        efflen += it2->efflen;
        pos += it2->posit;
        it2--;
        efflen += it2->efflen;
        pos += it2->posit;
    }
    return true;
}

CRef<CSeq_loc> GetGenomicBounds(const objects::CSeq_align& seqalign)
{
    const CSpliced_seg& sps = seqalign.GetSegs().GetSpliced();
    const CSeq_id& nucid = sps.GetGenomic_id();
    if (seqalign.CanGetBounds()) {
        ITERATE(CSeq_align::TBounds, b,seqalign.GetBounds()) {
            if ((*b)->GetId() != NULL && (*b)->GetId()->Match(nucid))
                return *b;
        }
    }

    CRef<CSeq_loc> genomic(new CSeq_loc);

    if (sps.GetExons().empty()) {
        genomic->SetNull();
    } else {
        genomic->SetPacked_int().AddInterval(nucid,sps.GetExons().front()->GetGenomic_start(),sps.GetExons().front()->GetGenomic_end(),sps.GetGenomic_strand());
        genomic->SetPacked_int().AddInterval(nucid,sps.GetExons().back()->GetGenomic_start(),sps.GetExons().back()->GetGenomic_end(),sps.GetGenomic_strand());

        genomic = sequence::Seq_loc_Merge(*genomic, CSeq_loc::fMerge_SingleRange, NULL);
    }

    return genomic;
}

void CProSplignText::AddSpliceText(CSeqVector_CI& genomic_ci, int& nuc_prev, char match)
{
    AddDNAText(genomic_ci,nuc_prev,2);
    m_translation.append((SIZE_TYPE)2,SPACE_CHAR);
    m_match.append((SIZE_TYPE)2,match);
    m_protein.append((SIZE_TYPE)2,INTRON_CHAR);
}

void CProSplignText::AddDNAText(CSeqVector_CI& genomic_ci, int& nuc_prev, size_t len)
{
    string buf;
    genomic_ci.GetSeqData(buf,len);
    nuc_prev +=len;
    m_dna.append(buf);
}

void CProSplignText::AddProtText(CSeqVector_CI& protein_ci, int& prot_prev, size_t len)
{
    m_protein.reserve(m_protein.size()+len);

    size_t phase = (prot_prev+1)%3;

    if (phase!=0) {
        size_t prev_not_intron_pos = m_protein.find_last_not_of(INTRON_CHAR,m_protein.size()-1);
        char aa = m_protein[prev_not_intron_pos];
        SIZE_TYPE added_len = min(3-phase,len);
        if (prev_not_intron_pos == m_protein.size()-1 && phase+added_len==3) {
            m_protein.append(added_len,SPACE_CHAR);
            m_protein[m_protein.size()-3] = SPACE_CHAR;
            m_protein[m_protein.size()-2] = toupper(aa);
        } else {
            m_protein.append(added_len,aa);
        }
        len -= added_len;
        prot_prev += added_len;
    }

    if (len > 0) {
        string buf;
        protein_ci.GetSeqData(buf,(len+2)/3);
        const char* p = buf.c_str();

        while (len >= 3) {
            m_protein.push_back(SPACE_CHAR);
            m_protein.push_back(*p++);
            m_protein.push_back(SPACE_CHAR);
            len -=3;
            prot_prev += 3;
        }
        if (len > 0) {
            m_protein.append(len,tolower(*p));
        }
        prot_prev += len;
    }
}

// translate last len bases in m_dna
// plus spliced codon in prev exon if at the start of exon
void CProSplignText::TranslateDNA(int phase, size_t len)
{
    _ASSERT( m_translation.size()+len ==m_dna.size() );
    _ASSERT( phase==0 || m_dna.size()>0 );

    m_translation.reserve(m_translation.size()+len);
    size_t start_pos = m_dna.size()-len;

    if (phase != 0) {
        size_t prev_exon_pos = 0;
        if ((prev_exon_pos=m_protein.find_last_not_of(INTRON_CHAR,start_pos-1))!=start_pos-1 &&
            m_match[prev_exon_pos]!=BAD_PIECE_CHAR) {
            string codon = m_dna.substr(prev_exon_pos-phase+1,phase)+m_dna.substr(start_pos,3-phase);
            char aa = prosplign::SEQUTIL::TranslateTriplet(codon);
            for( size_t i = prev_exon_pos-phase+1; i<=prev_exon_pos;++i) {
                m_translation[i] = tolower(aa);
                m_match[i] = MatchChar(i);
            }
            m_translation.append((SIZE_TYPE)(3-phase),tolower(aa));
        } else {
            m_translation.append((SIZE_TYPE)(3-phase),SPACE_CHAR);
        }
        start_pos += 3-phase;
   }

    char aa[] = "   ";
    for ( ; start_pos+3 <= m_dna.size(); start_pos += 3) {
        aa[1] = prosplign::SEQUTIL::TranslateTriplet(m_dna.substr(start_pos,3));
        m_translation += aa;
    }

    if (start_pos < m_dna.size())
        m_translation.append(m_dna.size()-start_pos,SPACE_CHAR);

    _ASSERT( m_translation.size()==m_dna.size() );
}

char CProSplignText::MatchChar(size_t i)
{
    char m = SPACE_CHAR;
    if (m_translation[i] != SPACE_CHAR && m_protein[i] != SPACE_CHAR) {
        if(m_matrix->scaled_subst_matrix[m_protein[i]][m_translation[i]] > 0) {
            if (m_translation[i] == m_protein[i])
                m = MATCH_CHAR;
            else
                m = POSIT_CHAR;
        }
    }
    return m;
}

void CProSplignText::MatchText(size_t len)
{
    _ASSERT( m_translation.size() == m_protein.size() );
    _ASSERT( m_translation.size() == m_match.size()+len );

    m_match.reserve(m_match.size()+len);

    for (size_t i = m_translation.size()-len; i < m_translation.size(); ++i) {
        m_match.push_back(MatchChar(i));
    }
}

namespace {
int GetProdPosInBases(const CProduct_pos& product_pos)
{
    if (product_pos.IsNucpos())
        return product_pos.GetNucpos();

    const CProt_pos&  prot_pos = product_pos.GetProtpos();
    return prot_pos.GetAmin()*3+ prot_pos.GetFrame()-1;
}
}

void CProSplignText::AddHoleText(
                                 bool prev_3_prime_splice, bool cur_5_prime_splice,
                                 CSeqVector_CI& genomic_ci, CSeqVector_CI& protein_ci,
                                 int& nuc_prev, int& prot_prev,
                                 int nuc_cur_start, int prot_cur_start)
{
    _ASSERT( m_dna.size() == m_translation.size() );
    _ASSERT( m_match.size() == m_protein.size() );
    _ASSERT( m_dna.size() == m_protein.size() );

    int prot_hole_len = prot_cur_start - prot_prev -1;
    int nuc_hole_len = nuc_cur_start - nuc_prev -1;

    bool can_show_splices = prot_hole_len < nuc_hole_len -4;
    if (can_show_splices && prev_3_prime_splice) {
        AddSpliceText(genomic_ci,nuc_prev, BAD_PIECE_CHAR);
        nuc_hole_len = nuc_cur_start - nuc_prev -1;
    }
    if (can_show_splices && cur_5_prime_splice) {
        nuc_cur_start -= 2;
        nuc_hole_len = nuc_cur_start - nuc_prev -1;
    }

    SIZE_TYPE hole_len = max(prot_hole_len,nuc_hole_len);
    _ASSERT( prot_hole_len>0 || nuc_hole_len>0 );
    int left_gap = 0;
    
    left_gap = (prot_hole_len-nuc_hole_len)/2;
    if (left_gap>0)
        m_dna.append((SIZE_TYPE)left_gap,GAP_CHAR);
    if (nuc_hole_len>0)
        AddDNAText(genomic_ci,nuc_prev,nuc_hole_len);
    if (prot_hole_len>nuc_hole_len)
        m_dna.append((SIZE_TYPE)(prot_hole_len-nuc_hole_len-left_gap),GAP_CHAR);
    
    m_translation.append(hole_len,SPACE_CHAR);
    m_match.append(hole_len,BAD_PIECE_CHAR);
    
    left_gap = (nuc_hole_len-prot_hole_len)/2;
    if (left_gap>0)
        m_protein.append((SIZE_TYPE)left_gap,GAP_CHAR);
    if (prot_hole_len>0)
        AddProtText(protein_ci,prot_prev,prot_hole_len);
    if (prot_hole_len<nuc_hole_len)
        m_protein.append((SIZE_TYPE)(nuc_hole_len-prot_hole_len-left_gap),
                         GAP_CHAR);
    
    if (can_show_splices && cur_5_prime_splice) {
        AddSpliceText(genomic_ci,nuc_prev, BAD_PIECE_CHAR);
    }
    _ASSERT( m_dna.size() == m_translation.size() );
    _ASSERT( m_match.size() == m_protein.size() );
    _ASSERT( m_dna.size() == m_protein.size() );
}

CProSplignText::~CProSplignText()
{
}

CProSplignText::CProSplignText(objects::CScope& scope, const objects::CSeq_align& seqalign, const string& matrix_name) :
    m_matrix(new prosplign::CSubstMatrix(matrix_name, 1))
{
    const CSpliced_seg& sps = seqalign.GetSegs().GetSpliced();

    ENa_strand strand = sps.GetGenomic_strand();

    const CSeq_id& protid = sps.GetProduct_id();
    int prot_len = sps.GetProduct_length()*3;
    CSeqVector protein_seqvec(scope.GetBioseqHandle(protid), CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI protein_ci(protein_seqvec);

    CRef<CSeq_loc> genomic_seqloc = GetGenomicBounds(seqalign);
    CSeqVector genomic_seqvec(*genomic_seqloc, scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    int nuc_from = genomic_seqloc->GetTotalRange().GetFrom();
    int nuc_to = genomic_seqloc->GetTotalRange().GetTo();
    int nuc_prev = -1;
    int prot_prev = -1;
    bool prev_3_prime_splice = false;
    ITERATE(CSpliced_seg::TExons, e_it, sps.GetExons()) {
        const CSpliced_exon& exon = **e_it;
        int prot_cur_start = GetProdPosInBases(exon.GetProduct_start());
        int prot_cur_end = GetProdPosInBases(exon.GetProduct_end());
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();
        if (strand==eNa_strand_plus) {
            nuc_cur_start -= nuc_from;
            nuc_cur_end -= nuc_from;
        } else {
            swap(nuc_cur_start,nuc_cur_end);
            nuc_cur_start = nuc_to - nuc_cur_start;
            nuc_cur_end = nuc_to - nuc_cur_end;
        }
        bool cur_5_prime_splice = exon.CanGetSplice_5_prime() && exon.GetSplice_5_prime().CanGetBases() && exon.GetSplice_5_prime().GetBases().size()==2;
        bool hole_before =
            prot_prev+1 != prot_cur_start || !( prev_3_prime_splice && cur_5_prime_splice || prot_cur_start==0 && nuc_cur_start==0);

        if (hole_before) {
            AddHoleText(prev_3_prime_splice, cur_5_prime_splice,
                        genomic_ci, protein_ci,
                        nuc_prev, prot_prev,
                        nuc_cur_start, prot_cur_start);
        } else { //intron
            SIZE_TYPE intron_len = nuc_cur_start - nuc_prev -1;
            AddDNAText(genomic_ci, nuc_prev, intron_len);
            m_translation.append(intron_len,SPACE_CHAR);
            m_match.append(intron_len,MISMATCH_CHAR);
            m_protein.append(intron_len,INTRON_CHAR);
        }

        _ASSERT( m_dna.size() == m_translation.size() );
        _ASSERT( m_match.size() == m_protein.size() );
        _ASSERT( m_dna.size() == m_protein.size() );
        
        prev_3_prime_splice = exon.CanGetSplice_3_prime() && exon.GetSplice_3_prime().CanGetBases() && exon.GetSplice_3_prime().GetBases().size()==2;

        ITERATE(CSpliced_exon::TParts, p_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **p_it;
            if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
                int len = 0;
                if (chunk.IsDiag()) {
                    len = chunk.GetDiag();
                } else if (chunk.IsMatch()) {
                    len = chunk.GetMatch();
                } else if (chunk.IsMismatch()) {
                    len = chunk.GetMismatch();
                }
                AddDNAText(genomic_ci,nuc_prev,len);
                TranslateDNA((prot_prev+1)%3,len);
                AddProtText(protein_ci,prot_prev,len);
                MatchText(len);
            } else if (chunk.IsProduct_ins()) {
                _ASSERT((prot_prev+1)%3==0);

                SIZE_TYPE len = chunk.GetProduct_ins();
                m_dna.append(len,GAP_CHAR);
                m_translation.append(len,SPACE_CHAR);
                m_match.append(len,MISMATCH_CHAR);
                AddProtText(protein_ci,prot_prev,len);
            } else if (chunk.IsGenomic_ins()) {
                SIZE_TYPE len = chunk.GetGenomic_ins();
                AddDNAText(genomic_ci,nuc_prev,len);
                if (0<=prot_prev && prot_prev<prot_len-1)
                    TranslateDNA((prot_prev+1-len%3)%3,len);
                else
                    m_translation.append(len,SPACE_CHAR);
                m_match.append(len,MISMATCH_CHAR);
                m_protein.append(len,GAP_CHAR);
            }
            _ASSERT(prot_prev <= prot_cur_end);
        }
        _ASSERT(prot_prev == prot_cur_end);
        _ASSERT(nuc_prev == nuc_cur_end);

        _ASSERT( m_dna.size() == m_translation.size() );
        _ASSERT( m_match.size() == m_protein.size() );
        _ASSERT( m_dna.size() == m_protein.size() );
    }

    int nuc_cur_start = nuc_to - nuc_from +1;
    int prot_cur_start = prot_len;
    if (prot_prev+1 != prot_cur_start || nuc_prev+1 != nuc_cur_start) {
        bool cur_5_prime_splice = false;
        AddHoleText(prev_3_prime_splice, cur_5_prime_splice,
                    genomic_ci, protein_ci,
                    nuc_prev, prot_prev,
                    nuc_cur_start, prot_cur_start);
    }
}

namespace {
int GetCompNum(const CSeq_align& sa)
{
    if (sa.CanGetExt()) {
        ITERATE(CSeq_align::TExt, e, sa.GetExt()) {
            const CUser_object& uo = **e;
            if (uo.CanGetData()) {
                ITERATE(CUser_object::TData, field, uo.GetData()) {
                    if ((*field)->CanGetLabel() && (*field)->GetLabel().IsStr() && (*field)->GetLabel().GetStr()=="CompartmentId") {
                        return (*field)->GetData().GetInt();
                    }
                }
            }
        }
    }
    return -1;
}
}

void CProSplignText::Output(const CSeq_align& seqalign, CScope& scope, ostream& out, int width)
{
    int compartment_id = GetCompNum(seqalign);
    string contig_name = seqalign.GetSegs().GetSpliced().GetGenomic_id().GetSeqIdString(true);
    string prot_id = seqalign.GetSegs().GetSpliced().GetProduct_id().GetSeqIdString(true);
    TSeqRange bounds = GetGenomicBounds(seqalign)->GetTotalRange();
    int nuc_from = bounds.GetFrom();
    int nuc_to = bounds.GetTo();
    bool is_plus_strand = seqalign.GetSegs().GetSpliced().GetGenomic_strand()==eNa_strand_plus;

    out<<endl<<"************************************************************************"<<endl;
    out<<"************************************************************************"<<endl;
    out<<"************************************************************************"<<endl;
    out<<compartment_id<<"\t"<<contig_name<<"\t"<<prot_id<<"\t"<<nuc_from<<"\t"<<nuc_to<<"\t";
    out<<(is_plus_strand?'+':'-')<<endl;

    CProSplignText align_text(scope, seqalign);
    const string& dna = align_text.GetDNA();
    const string& translation = align_text.GetTranslation();
    string match = align_text.GetMatch();
    const string& protein = align_text.GetProtein();
    string good_parts;
    good_parts.append(match.size(),'*');
    for (size_t i = 0; i < match.size(); ++i) {
        if (match[i] == BAD_PIECE_CHAR)
            match[i] = good_parts[i] = ' ';
    }

    int npos1 = is_plus_strand?nuc_from:nuc_to;

    int prot_beg_pos = protein.find_first_not_of(GAP_CHAR);
    int prot_end_pos = protein.find_last_not_of(GAP_CHAR);

    for(int i=0;i<prot_end_pos; i+=width) {
        int apos = i+width-1;
        if (apos >= (int)dna.length())
            apos = (int)dna.length() - 1;

#ifdef NCBI_COMPILER_WORKSHOP
        int gaps = 0;
        count(dna.begin()+i, dna.begin()+(i+width), GAP_CHAR, gaps);
        int real_bases = width-gaps;
#else
        int real_bases = width-count(dna.begin()+i, dna.begin()+(i+width), GAP_CHAR);
#endif

        int npos2 = is_plus_strand?npos1+real_bases-1:npos1-(real_bases-1);

        //throw out head and tail lines with a complete row gap
        if (apos > prot_beg_pos) {
            out.setf(IOS_BASE::left, IOS_BASE::adjustfield);
            if (real_bases>0) {
                out<<setw(12)<<npos1<<dna.substr(i, width)<<"   "<<npos2<<endl;
            } else { //complete row is a nucleotide gap
                out<<setw(12)<<"-"<<dna.substr(i, width)<<"   "<<"-"<<endl;
            }
            out<<setw(12)<<" "<<translation.substr(i, width)<<endl;
            out<<setw(12)<<" "<<match.substr(i, width)<<endl;
            out<<setw(12)<<" "<<protein.substr(i, width)<<endl;
            out<<setw(12)<<" "<<good_parts.substr(i, width)<<endl;
        }

        npos1 = is_plus_strand?npos2+1:npos2-1;
    }
}


namespace {
bool HasStartOnNuc(const CSpliced_seg& sps, CScope& scope)
{
    const CSpliced_exon& exon = *sps.GetExons().front();
    if (exon.GetProduct_start().GetProtpos().GetFrame()!=1)
        return false;
    const CSpliced_exon_chunk& chunk = *exon.GetParts().front();
    if (chunk.IsProduct_ins() || chunk.IsGenomic_ins())
        return false;
    int len = 0;
    if (chunk.IsDiag()) {
        len = chunk.GetDiag();
    } else if (chunk.IsMatch()) {
        len = chunk.GetMatch();
    } else if (chunk.IsMismatch()) {
        len = chunk.GetMismatch();
    }
    if (len < 3)
        return false;

    CSeq_id nucid;
    nucid.Assign(sps.GetGenomic_id());
    CSeq_loc genomic_seqloc(nucid,exon.GetGenomic_start(), exon.GetGenomic_end(),sps.GetGenomic_strand());

    CSeqVector genomic_seqvec(genomic_seqloc, scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    string buf;
    genomic_ci.GetSeqData(buf, 3);
    
    return buf=="ATG";
}

bool HasStopOnNuc(const CSpliced_seg& sps, CScope& scope)
{
    const CSpliced_exon& exon = *sps.GetExons().back();
    if (exon.GetProduct_end().GetProtpos().GetFrame()!=3)
        return false;

    if (sps.GetGenomic_strand()==eNa_strand_minus &&
        exon.GetGenomic_start()<3)
        return false;

    TSeqPos stop_codon_start = sps.GetGenomic_strand()==eNa_strand_plus?exon.GetGenomic_end()+1:exon.GetGenomic_start()-3;
    TSeqPos stop_codon_end = sps.GetGenomic_strand()==eNa_strand_plus?exon.GetGenomic_end()+3:exon.GetGenomic_start()-1;

    CSeq_id nucid;
    nucid.Assign(sps.GetGenomic_id());

    TSeqPos seq_end = sequence::GetLength(nucid, &scope)-1;
    if (sps.GetGenomic_strand()==eNa_strand_plus?seq_end<stop_codon_end:stop_codon_start<0)
        return false;

    CSeq_loc genomic_seqloc(nucid,stop_codon_start, stop_codon_end,sps.GetGenomic_strand());

    CSeqVector genomic_seqvec(genomic_seqloc, scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    string buf;
    genomic_ci.GetSeqData(buf, 3);
    
    return buf.size()==3 && (buf=="TAA" || buf=="TGA" || buf=="TAG");
}
}

BEGIN_SCOPE(prosplign)
void SeekStartStop(CSeq_align& seq_align, CScope& scope)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();

    if (sps.IsSetModifiers()) {
        for (CSpliced_seg::TModifiers::iterator m = sps.SetModifiers().begin(); m != sps.SetModifiers().end(); ) {
            if ((*m)->IsStart_codon_found() || (*m)->IsStop_codon_found())
                m = sps.SetModifiers().erase(m);
            else
                ++m;
        }
        if (sps.GetModifiers().empty())
            sps.ResetModifiers();
    }

    if (!sps.SetExons().empty()) {
        //start, stop
        if(HasStartOnNuc(sps, scope)) {
            CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
            modi->SetStart_codon_found(true);
            sps.SetModifiers().push_back(modi);
        }
        if(HasStopOnNuc(sps, scope)) {
            CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
            modi->SetStop_codon_found(true);
            sps.SetModifiers().push_back(modi);
        }
    }
}
END_SCOPE(prosplign)

USING_SCOPE(prosplign);

namespace {

struct CAliChunk {
    CAliChunk(TSeqPos ali_pos, TSeqPos nuc_pos, TSeqPos prot_pos, CSpliced_seg::TExons::iterator exon_iter, CSpliced_exon::TParts::iterator chunk_iter) :
        m_nuc_pos(nuc_pos), m_prot_pos(prot_pos), m_exon_iter(exon_iter), m_chunk_iter(chunk_iter), m_bad(false)
    {
        const CSpliced_exon_chunk& chunk = **chunk_iter;

        m_nuc_len = 0;
        m_prot_len = 0;
        if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
            int len = 0;
            if (chunk.IsDiag()) {
                len = chunk.GetDiag();
            } else if (chunk.IsMatch()) {
                len = chunk.GetMatch();
            } else if (chunk.IsMismatch()) {
                len = chunk.GetMismatch();
            }

            m_nuc_len = len;
            m_prot_len = len;
        } else if (chunk.IsProduct_ins()) {
            m_prot_len = chunk.GetProduct_ins();
        } else if (chunk.IsGenomic_ins()) {
            m_nuc_len = chunk.GetGenomic_ins();
        }

        m_ali_range = TSeqRange(ali_pos, ali_pos + max(m_nuc_len,m_prot_len)-1);
    }

    TSeqRange m_ali_range;
    TSeqPos m_nuc_pos;
    TSeqPos m_prot_pos;
    int m_nuc_len;
    int m_prot_len;
    CSpliced_seg::TExons::iterator m_exon_iter;
    CSpliced_exon::TParts::iterator m_chunk_iter;
    bool m_bad;
};

typedef list<CAliChunk> TAliChunkCollection;
typedef TAliChunkCollection::iterator TAliChunkIterator;


TAliChunkCollection ExtractChunks(CSeq_align& seq_align)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();
    ENa_strand strand = sps.GetGenomic_strand();
    TSeqRange bounds = GetGenomicBounds(seq_align)->GetTotalRange();
    int nuc_from = bounds.GetFrom();
    int nuc_to = bounds.GetTo();
    int prot_from = 0;

    int alignment_pos = 0;

    TAliChunkCollection chunks;

    NON_CONST_ITERATE (CSpliced_seg::TExons, e_it, sps.SetExons()) {
        CSpliced_exon& exon = **e_it;
        int prot_cur_start = GetProdPosInBases(exon.GetProduct_start());
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();

        if (strand == eNa_strand_plus) {
            alignment_pos += max(prot_cur_start-prot_from, nuc_cur_start-nuc_from);
            nuc_from = nuc_cur_start;
        } else {
            alignment_pos += max(prot_cur_start-prot_from, nuc_to-nuc_cur_end);
            nuc_to = nuc_cur_end;
        }
        prot_from = prot_cur_start;

        NON_CONST_ITERATE (CSpliced_exon::TParts, p_it, exon.SetParts()) {
            CAliChunk chunk(alignment_pos, strand == eNa_strand_plus?nuc_from:nuc_to, prot_from, e_it, p_it);
            alignment_pos = chunk.m_ali_range.GetTo()+1;
            prot_from += chunk.m_prot_len;
            if (strand == eNa_strand_plus) {
                nuc_from += chunk.m_nuc_len;
            } else {
                nuc_to -= chunk.m_nuc_len;
            }

            chunks.push_back(chunk);
        }

        _ASSERT( GetProdPosInBases(exon.GetProduct_end())+1==prot_from );
        _ASSERT( strand==eNa_strand_plus && nuc_cur_end+1==nuc_from || strand!=eNa_strand_plus && nuc_cur_start-1==nuc_to );
    }
    return chunks;
}

list<TSeqRange> InvertPartList(const list<CNPiece>& good_parts, TSeqRange total_range)
{
    list<TSeqRange> bad_parts;
    
    int tail_beg = total_range.GetFrom();
    int tail_end = total_range.GetTo();
    ITERATE(list<CNPiece>, i, good_parts) {
        if (tail_beg < i->beg)
            bad_parts.push_back(TSeqRange(tail_beg,i->beg-1));
        tail_beg = i->end;
    }
    if (tail_beg <= tail_end)
        bad_parts.push_back(TSeqRange(tail_beg,tail_end));

    return bad_parts;
}
    
void TestExonLength(const CSpliced_exon& exon)
{
        int prot_cur_start = GetProdPosInBases(exon.GetProduct_start());
        int prot_cur_end = GetProdPosInBases(exon.GetProduct_end());
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();
        int nuc_len = 0;
        int prot_len = 0;
        ITERATE (CSpliced_exon::TParts, p_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **p_it;

            if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
                int len = 0;
                if (chunk.IsDiag()) {
                    len = chunk.GetDiag();
                } else if (chunk.IsMatch()) {
                    len = chunk.GetMatch();
                } else if (chunk.IsMismatch()) {
                    len = chunk.GetMismatch();
                }
                
                nuc_len += len;
                prot_len += len;
            } else if (chunk.IsProduct_ins()) {
                prot_len += chunk.GetProduct_ins();
            } else if (chunk.IsGenomic_ins()) {
                nuc_len += chunk.GetGenomic_ins();
            }
        }
        _ASSERT( nuc_cur_end-nuc_cur_start+1 == nuc_len );
        _ASSERT( prot_cur_end-prot_cur_start+1 == prot_len );
}

void SplitChunk(TAliChunkCollection& chunks, TAliChunkIterator iter, TSeqPos start_of_second_chunk, bool genomic_plus)
{
    _DEBUG_CODE( TestExonLength(**iter->m_exon_iter); );
    _ASSERT( iter->m_ali_range.GetFrom() < start_of_second_chunk );
    _ASSERT( start_of_second_chunk <= iter->m_ali_range.GetTo());
    _ASSERT( iter->m_nuc_len == iter->m_prot_len ); // not an insertion

    CRef< CSpliced_exon_chunk > new_chunk( new CSpliced_exon_chunk);
    new_chunk->Assign(**iter->m_chunk_iter);
    int first_len = start_of_second_chunk - iter->m_ali_range.GetFrom();
    int second_len = iter->m_ali_range.GetTo() - start_of_second_chunk+1;

    TAliChunkIterator first_iter = chunks.insert(iter, *iter);

    if (genomic_plus) {
        iter->m_nuc_pos += first_len;
    } else {
        iter->m_nuc_pos -= first_len;
    }
    iter->m_prot_pos += first_len;

    if (new_chunk->IsDiag()) {
        new_chunk->SetDiag(first_len);
        (*iter->m_chunk_iter)->SetDiag(second_len);
    } else if (new_chunk->IsMatch()) {
        new_chunk->SetMatch(first_len);
        (*iter->m_chunk_iter)->SetMatch(second_len);
    } else if (new_chunk->IsMismatch()) {
        new_chunk->SetMismatch(first_len);
        (*iter->m_chunk_iter)->SetMismatch(second_len);
    }

    first_iter->m_ali_range.SetTo(start_of_second_chunk-1);
    iter->m_ali_range.SetFrom(start_of_second_chunk);

    first_iter->m_nuc_len = first_iter->m_prot_len = first_len;
    iter->m_nuc_len = iter->m_prot_len = second_len;

    first_iter->m_chunk_iter = (*iter->m_exon_iter)->SetParts().insert(iter->m_chunk_iter, new_chunk);

    _DEBUG_CODE( TestExonLength(**iter->m_exon_iter); );
}

void DropExon(CSpliced_seg::TExons& exons, CSpliced_seg::TExons::iterator& exon_iter)
{
    exons.erase(exon_iter);
}

void DropExonHead(TAliChunkIterator chunk_iter, bool genomic_plus)
{
    CSpliced_exon* cur_exon = *chunk_iter->m_exon_iter;

    _DEBUG_CODE( TestExonLength(*cur_exon); );
#ifdef _DEBUG
    size_t chunks_count = cur_exon->GetParts().size();
#endif

    cur_exon->SetParts().erase(cur_exon->SetParts().begin(), chunk_iter->m_chunk_iter);

    if (genomic_plus) {
        cur_exon->SetGenomic_start(chunk_iter->m_nuc_pos);
    } else {
        cur_exon->SetGenomic_end(chunk_iter->m_nuc_pos);
    }
    cur_exon->SetProduct_start(*NultriposToProduct_pos(chunk_iter->m_prot_pos));
    cur_exon->SetPartial(true);
    if (cur_exon->IsSetSplice_5_prime())
        cur_exon->ResetSplice_5_prime();

    _ASSERT( 0 < cur_exon->GetParts().size() );
    _ASSERT( cur_exon->GetParts().size() <  chunks_count );

    _DEBUG_CODE( TestExonLength(*cur_exon); );
}

void SplitExon(CSpliced_seg::TExons& exons, TAliChunkIterator chunk_iter, bool genomic_plus)
{
    CSpliced_exon* cur_exon = *chunk_iter->m_exon_iter;
    
    _DEBUG_CODE( TestExonLength(*cur_exon); );
#ifdef _DEBUG
    size_t chunks_count = cur_exon->GetParts().size();
#endif

    CRef< CSpliced_exon > new_exon( new CSpliced_exon );
    new_exon->Assign(*cur_exon);

    CSpliced_exon::TParts::iterator new_exon_chunk = new_exon->SetParts().begin();
    ITERATE(CSpliced_exon::TParts, old_exon_chunk, cur_exon->GetParts()) {
        if (old_exon_chunk==chunk_iter->m_chunk_iter)
            break;
        ++new_exon_chunk;
    }

    new_exon->SetParts().erase(new_exon_chunk, new_exon->SetParts().end());

    if (genomic_plus) {
        new_exon->SetGenomic_end(chunk_iter->m_nuc_pos-1);
    } else {
        new_exon->SetGenomic_start(chunk_iter->m_nuc_pos+1);
    }
    new_exon->SetProduct_end(*NultriposToProduct_pos(chunk_iter->m_prot_pos-1));
    new_exon->SetPartial(true);
    if (new_exon->IsSetSplice_3_prime())
        new_exon->ResetSplice_3_prime();

    _ASSERT( new_exon->GetGenomic_start() <= new_exon->GetGenomic_end() );
    _ASSERT( GetProdPosInBases(new_exon->GetProduct_start()) <= GetProdPosInBases(new_exon->GetProduct_end()) );

    exons.insert(chunk_iter->m_exon_iter, new_exon);

    DropExonHead(chunk_iter, genomic_plus);

    _ASSERT( 0 < new_exon->GetParts().size() && 0 < cur_exon->GetParts().size() );
    _ASSERT( new_exon->GetParts().size()+cur_exon->GetParts().size() == chunks_count );

    _DEBUG_CODE( TestExonLength(*new_exon); );
    _DEBUG_CODE( TestExonLength(*cur_exon); );
}

}

void prosplign::RefineAlignment(CSeq_align& seq_align, const list<CNPiece>& good_parts)
{
    CSpliced_seg& sps = seq_align.SetSegs().SetSpliced();
    TAliChunkCollection chunks = ExtractChunks(seq_align);

    if (chunks.empty())
        return;

    bool genomic_plus = sps.GetGenomic_strand()==eNa_strand_plus;

    list<TSeqRange> bad_parts = InvertPartList(good_parts, TSeqRange(chunks.front().m_ali_range.GetFrom(),chunks.back().m_ali_range.GetTo()));

    TAliChunkIterator chunk_iter = chunks.begin();

    ITERATE(list<TSeqRange>, bad_part, bad_parts) {
        while (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetTo() < bad_part->GetFrom()) {
            ++chunk_iter;
        }

        if (chunk_iter == chunks.end())
            break;
        if (bad_part->GetTo() < chunk_iter->m_ali_range.GetFrom())
            continue;

        if (chunk_iter->m_ali_range.GetFrom() < bad_part->GetFrom())
            SplitChunk(chunks, chunk_iter, bad_part->GetFrom(), genomic_plus);

        while (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetTo() <= bad_part->GetTo())
            chunk_iter++->m_bad = true;

        if (chunk_iter != chunks.end() && chunk_iter->m_ali_range.GetFrom() <= bad_part->GetTo()) {
            chunk_iter->m_bad = true;
            SplitChunk(chunks, chunk_iter, bad_part->GetTo()+1, genomic_plus);
            chunk_iter->m_bad = false;
        }
    }
    
    CSpliced_seg::TExons::iterator prev_exon_iter = sps.SetExons().end();

    NON_CONST_ITERATE(TAliChunkCollection, chunk_iter, chunks) {
        while(chunk_iter != chunks.end() && !chunk_iter->m_bad) {
            prev_exon_iter = chunk_iter->m_exon_iter;
            ++chunk_iter;
        }
        if (chunk_iter == chunks.end())
            break;
        if (prev_exon_iter != chunk_iter->m_exon_iter) {
            if ((*chunk_iter->m_exon_iter)->IsSetSplice_5_prime())
                (*chunk_iter->m_exon_iter)->ResetSplice_5_prime();
        } else {
            SplitExon(sps.SetExons(),chunk_iter, genomic_plus);
        }

        prev_exon_iter = chunk_iter->m_exon_iter;
        TAliChunkIterator next_chunk_iter = chunk_iter;
        ++next_chunk_iter;
        while (next_chunk_iter != chunks.end() && next_chunk_iter->m_bad && next_chunk_iter->m_exon_iter==prev_exon_iter) {
            chunk_iter = next_chunk_iter++;
        }

        if (next_chunk_iter == chunks.end() || next_chunk_iter->m_exon_iter!=prev_exon_iter) {
            DropExon(sps.SetExons(), prev_exon_iter);
        } else {
            DropExonHead(next_chunk_iter, genomic_plus);
        }
    }

#ifdef _DEBUG
    ITERATE (CSpliced_seg::TExons, e_it, sps.GetExons()) {
        TestExonLength(**e_it);
    }
    
#endif
}

END_NCBI_SCOPE

