#ifndef BLASTFMTUTIL_HPP
#define BLASTFMTUTIL_HPP

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
 * Author:  Jian Ye
 */

/** @file blastfmtutil.hpp
 * BLAST formatter utilities.
 */

#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include  <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/alnmgr/alnvec.hpp>

/**setting up scope*/
BEGIN_NCBI_SCOPE
USING_SCOPE (objects);


///blast related url
///entrez
const string kEntrezUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entre\
z/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s\" %s>";

///trace db
const string kTraceUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/Traces\
/trace.cgi?cmd=retrieve&dopt=fasta&val=%s\">";

///unigene
const string kUnigeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entr\
ez/query.fcgi?db=unigene&cmd=search&term=%d[Nucleotide+UID]\"><img border=0 h\
eight=16 width=16 src=\"/blast/images/U.gif\" alt=\"UniGene info\"></a>";

///structure
const string kStructureUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/St\
ructure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s\
&blast_view=%s&hsp=0&taxname=%s&client=blast\"><img border=0 height=16 width=\
16 src=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Re\
lated structures\"></a>";

///structure overview
const string kStructure_Overview = "<a href=\"http://www.ncbi.nlm.nih.\
gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_\
RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>";

///Geo
const string kGeoUrl =  "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=geo&term=%d[gi]\"><img border=0 height=16 width=16 src=\"/blast\
/images/E.gif\" alt=\"Geo\"></a>";

///Gene
const string kGeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=gene&cmd=search&term=%d[%s]\"><img border=0 height=16 width=16 \
src=\"/blast/images/G.gif\" alt=\"Gene info\"></a>";

///Sub-sequence
const string kEntrezSubseqUrl = "<a href=\"http://www.ncbi.nlm.nih.\
gov/entrez/viewer.fcgi?val=%d&db=%s&from=%d&to=%d&view=gbwithparts\">";

///Bl2seq 
const string kBl2seqUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/\
blast/bl2seq/wblast2.cgi?PROGRAM=tblastx&WORD=3&RID=%s&ONE=%s&TWO=%s\">Get \
TBLASTX alignments</a>";


/** This class contains misc functions for displaying BLAST results. */

class CBlastFormatUtil 
{
   
public:
   
    ///Error info structure
    struct SBlastError {
        EDiagSev level;   
        string message;  
    };

    ///Blast database info
    struct SDbInfo {
        bool   is_protein;
        string   name;
        string   definition;
        string   date;
        long int total_length;
        int   number_seqs;
        bool subset;	
    };
    ///Output blast errors
    ///@param error_return: list of errors to report
    ///@param error_post: post to stderr or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintError(list<SBlastError>& error_return, 
                                bool error_post, CNcbiOstream& out);
    
    ///Print out blast engine version
    ///@param program: name of blast program such as blastp, blastn
    ///@param html: in html format or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintVersionInfo(string program, bool html, 
                                      CNcbiOstream& out);

    ///Print out blast reference
    ///@param html: in html format or not
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///
    static void BlastPrintReference(bool html, size_t line_len, 
                                    CNcbiOstream& out);

    ///Print out misc information separated by "~"
    ///@param str:  input information
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///
    static void PrintTildeSepLines(string str, size_t line_len, 
                                   CNcbiOstream& out);

    ///Print out blast database information
    ///@param dbinfo_list: database info list
    ///@param line_length: length of each line desired
    ///@param out: stream to ouput
    ///
    static void PrintDbReport(list<SDbInfo>& dbinfo_list, size_t line_length, 
                              CNcbiOstream& out);
    
    ///Print out kappa, lamda blast parameters
    ///@param lambda
    ///@param k
    ///@param h
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///@param gapped: gapped alignment?
    ///@param c
    ///
    static void PrintKAParameters(float lambda, float k, float h,
                                  size_t line_len, CNcbiOstream& out, 
                                  bool gapped, float c = 0.0);
    
    ///Print out blast query info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not
    /// @param tabular Is this done for tabular formatting? 
    ///
    static void AcknowledgeBlastQuery(const CBioseq& cbs, size_t line_len,
                                      CNcbiOstream& out, bool believe_query,
                                      bool html, bool tabular=false);
    
    ///Get blast defline
    ///@param handle: bioseq handle to extract blast defline from
    ///
    static CRef<CBlast_def_line_set> 
    GetBlastDefline (const CBioseq_Handle& handle);

    ///Extract score info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///
    static void GetAlnScores(const CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<int>& use_this_gi);
    
    ///Add the specified white space
    ///@param out: ostream to add white space
    ///@param number: the number of white spaces desired
    ///
    static void AddSpace(CNcbiOstream& out, size_t number);
    
    ///format evalue and bit_score 
    ///@param evalue: e value
    ///@param bit_score: bit score
    ///@param evalue_str: variable to store the formatted evalue
    ///@param bit_score_str: variable to store the formatted bit score
    ///
    static void GetScoreString(double evalue, double bit_score, 
                               string& evalue_str, 
                               string& bit_score_str);
    
    ///Fill new alignset containing the specified number of alignments with
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param number: the specified number
    ///
    static void PruneSeqalign(CSeq_align_set& source_aln, 
                              CSeq_align_set& new_aln,
                              unsigned int number);

    /// Count alignment length, number of gap openings and total number of gaps
    /// in a single alignment.
    /// @param salv Object representing one alignment (HSP) [in]
    /// @param align_length Total length of this alignment [out]
    /// @param num_gaps Total number of insertions and deletions in this 
    ///                 alignment [out]
    /// @param num_gap_opens Number of gap segments in the alignment [out]
    static void GetAlignLengths(CAlnVec& salv, int& align_length, 
                                int& num_gaps, int& num_gap_opens);

    /// If a Seq-align-set contains Seq-aligns with discontinuous type segments, 
    /// extract the underlying Seq-aligns and put them all in a flat 
    /// Seq-align-set.
    /// @param source Original Seq-align-set
    /// @param target Resulting Seq-align-set
    static void ExtractSeqalignSetFromDiscSegs(CSeq_align_set& target,
                                               const CSeq_align_set& source);

    ///Create denseseg representation for densediag seqalign
    ///@param aln: the input densediag seqalign
    ///@return: the new denseseg seqalign
    static CRef<CSeq_align> CreateDensegFromDendiag(const CSeq_align& aln);
};

END_NCBI_SCOPE


/*===========================================
$Log$
Revision 1.12  2005/05/02 17:32:30  dondosha
Added static method CreateDensegFromDendiag, previously static function in showalign.cpp

Revision 1.11  2005/03/14 20:09:41  dondosha
Added 2 static methods, needed for tabular output; added boolean argument to AcknowledgeBlastQuery for tabular version

Revision 1.10  2005/03/02 18:21:17  jianye
some style fix

Revision 1.9  2005/02/23 16:28:55  jianye
num_ident addition to getalnscore

Revision 1.8  2005/02/22 15:52:49  jianye
add pruneseqalign

Revision 1.7  2005/02/14 17:42:36  jianye
Added more url

Revision 1.6  2005/02/09 17:35:30  jianye
add common url

Revision 1.5  2005/02/02 16:31:57  jianye
int to size_t to get rid of compiler warning

Revision 1.4  2005/02/01 21:28:42  camacho
Doxygen fixes

Revision 1.3  2005/01/31 17:43:02  jianye
change unsigned int to size_t

Revision 1.2  2005/01/25 17:34:13  jianye
add NCBI_XBLASTFORMAT_EXPORT label

Revision 1.1  2005/01/24 16:41:43  jianye
Initial check in



*/
#endif
