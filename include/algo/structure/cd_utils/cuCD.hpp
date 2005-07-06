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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       High-level algorithmic operations on one or more CCdCore objects.
 *       (methods that only traverse the cdd ASN.1 data structure are in 
 *        placed in the CCdCore class itself)
 *
 * ===========================================================================
 */

#ifndef CU_CD_HPP
#define CU_CD_HPP

#include <corelib/ncbistd.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(cd_utils) // namespace ncbi::objects::

//class CCdCore;
//class CNcbi_mime_asn1;

const int ALIGN_ANNOTS_VALID_FAILURE = 1;

int  GetReMasterFailureCode(const CCdCore* cd);

bool Reorder(CCdCore* pCD, const vector<int> positions);

//   Structure alignments in the features are required to be listed the order in
//   which the PDB identifiers appear in the alignment.
//   'positions' contains the new order of all NON-MASTER rows, as in Reorder
bool ReorderStructureAlignments(CCdCore* pCD, const vector<int>& positions);



//   Resets a variety of fields that need to be wiped out on remastering, or
//   when removing a consensus.
void ResetFields(CCdCore* pCD);

//   If ncbiMime contains a CD, return it.  Otherwise return NULL.
CCdCore* ExtractCDFromMime(CNcbi_mime_asn1* ncbiMime);

//   Remove consensus sequence from alignment and sequence list.
//   If the master was a consensus sequence, remaster to the 2nd alignment row first.
//int  PurgeConsensusSequences(CCdCore* pCD, bool resetFields = true);

//  If copyNonASNMembers = true, all statistics, sequence strings and aligned residues 
//  data structures are copied.  The CD's read status is *always* copied.
CCdCore* CopyCD(const CCdCore* cd);
/* replaced CdAlignmentAdapter::CreateCD
CCdCore* CreateChildCD(const CCdCore* origCD, const vector<int> selectedRows, string newAccession, string shortName);
*/

//   Set creation date of CD w/ the current time.  Removes existing creation date.
bool SetCreationDate(CCdCore* cd);
bool SetUpdateDate(CCdCore* cd);
//  When only the first pointer is passed, checks for overlaps among that CD's rows.
//  Otherwise, it reports on overlaps between two distinct CDs.
int NumberOfOverlappedRows(CCdCore* cd1, CCdCore* cd2 = NULL);
int GetOverlappedRows(CCdCore* cd1, CCdCore* cd2, vector<int>& rowsOfCD1, vector<int>& rowsOfCD2);

//  For a specified row in cd1, find all potential matches in cd2.
//  If cd1 == cd2, returns row1 plus any other matches (should be no such overlaps in a valid CD)
//  If cd1AsChild == true,  mapping assumes cd2 is parent of cd1.
//  If cd1AsChild == false, mapping assumes cd1 is parent of cd2.
//  In 'overlapMode', returns the row index of cd2 for *any* overlap with row1, not just
//  those overlaps which obey the specified parent/child relationship between.
//  Return number of rows found.
int GetMappedRowIds(CCdCore* cd1, int row1, CCdCore* cd2, vector<int>& rows2, bool cd1AsChild, bool overlapMode = false);

//  return a vector containing (in order) the full sequence in NCBIeaa
//  format for every Seq_entry in the sequence list
void SetConvertedSequencesForCD(CCdCore* cd, vector<string>& convertedSequences, bool forceRecompute = false);

//  for each row, return a char* containing all residues aligned in the cd
void SetAlignedResiduesForCD(CCdCore* cd, char** & ppAlignedResidues, bool forceRecompute = false);

//  Returns '<cd->GetAccession> (<cd->GetName>)' w/o the angle brackets; 
//  format used by the validator. 
string GetVerboseNameStr(const CCdCore* cd);

// for getting the bioseq and seq-loc for rows of a CD.
CRef< CBioseq > GetMasterBioseqWithFootprintOld(CCdCore* cd);  // deprecated
CRef< CBioseq > GetMasterBioseqWithFootprint(CCdCore* cd);
CRef< CBioseq > GetBioseqWithFootprintForNthRow(CCdCore* cd, int N, string& errstr);
bool GetBioseqWithFootprintForNRows(CCdCore* cd, int N, vector< CRef< CBioseq > >& bioseqs, string& errstr);

CRef< COrg_ref > GetCommonTax(CCdCore* cd);
bool obeysParentTypeConstraints(const CCdCore* pCD);
void calcDiversityRanking(CCdCore* pCD, list<int>& rankList);

struct SeqTreeNode
{
	string name; //gi or PDB for a leaf node; empty for internal node
	int x;
	int y;
	bool isLeaf;
	string childAcc;
};
typedef pair<SeqTreeNode, SeqTreeNode> SeqTreeEdge;

string layoutSeqTree(CCdCore* pCD, int maxX, int maxY, int yInt, vector<SeqTreeEdge>& edgs);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // ALGCD_HPP

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2005/07/06 20:58:19  cliu
 * return tree parameter string
 *
 * Revision 1.2  2005/06/21 13:10:20  cliu
 * add tree layout.
 *
 * Revision 1.1  2005/04/19 14:28:00  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
