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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to color alignment blocks by sequence conservation
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>  // must come before C-toolkit stuff
#include <corelib/ncbi_limits.h>

#include <blastkar.h>           // for BLAST standard probability routines
#include <objseq.h>
#include <math.h>

#include "cn3d/conservation_colorer.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/cn3d_blast.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

#define BLOSUMSIZE 24
static const char Blosum62Fields[BLOSUMSIZE] =
   { 'A', 'R', 'N', 'D', 'C', 'Q', 'E', 'G', 'H', 'I', 'L', 'K', 'M',
     'F', 'P', 'S', 'T', 'W', 'Y', 'V', 'B', 'Z', 'X', '*' };
static const char Blosum62Matrix[BLOSUMSIZE][BLOSUMSIZE] = {
/*       A,  R,  N,  D,  C,  Q,  E,  G,  H,  I,  L,  K,  M,  F,  P,  S,  T,  W,  Y,  V,  B,  Z,  X,  * */
/*A*/ {  4, -1, -2, -2,  0, -1, -1,  0, -2, -1, -1, -1, -1, -2, -1,  1,  0, -3, -2,  0, -2, -1,  0, -4 },
/*R*/ { -1,  5,  0, -2, -3,  1,  0, -2,  0, -3, -2,  2, -1, -3, -2, -1, -1, -3, -2, -3, -1,  0, -1, -4 },
/*N*/ { -2,  0,  6,  1, -3,  0,  0,  0,  1, -3, -3,  0, -2, -3, -2,  1,  0, -4, -2, -3,  3,  0, -1, -4 },
/*D*/ { -2, -2,  1,  6, -3,  0,  2, -1, -1, -3, -4, -1, -3, -3, -1,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
/*C*/ {  0, -3, -3, -3,  9, -3, -4, -3, -3, -1, -1, -3, -1, -2, -3, -1, -1, -2, -2, -1, -3, -3, -2, -4 },
/*Q*/ { -1,  1,  0,  0, -3,  5,  2, -2,  0, -3, -2,  1,  0, -3, -1,  0, -1, -2, -1, -2,  0,  3, -1, -4 },
/*E*/ { -1,  0,  0,  2, -4,  2,  5, -2,  0, -3, -3,  1, -2, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
/*G*/ {  0, -2,  0, -1, -3, -2, -2,  6, -2, -4, -4, -2, -3, -3, -2,  0, -2, -2, -3, -3, -1, -2, -1, -4 },
/*H*/ { -2,  0,  1, -1, -3,  0,  0, -2,  8, -3, -3, -1, -2, -1, -2, -1, -2, -2,  2, -3,  0,  0, -1, -4 },
/*I*/ { -1, -3, -3, -3, -1, -3, -3, -4, -3,  4,  2, -3,  1,  0, -3, -2, -1, -3, -1,  3, -3, -3, -1, -4 },
/*L*/ { -1, -2, -3, -4, -1, -2, -3, -4, -3,  2,  4, -2,  2,  0, -3, -2, -1, -2, -1,  1, -4, -3, -1, -4 },
/*K*/ { -1,  2,  0, -1, -3,  1,  1, -2, -1, -3, -2,  5, -1, -3, -1,  0, -1, -3, -2, -2,  0,  1, -1, -4 },
/*M*/ { -1, -1, -2, -3, -1,  0, -2, -3, -2,  1,  2, -1,  5,  0, -2, -1, -1, -1, -1,  1, -3, -1, -1, -4 },
/*F*/ { -2, -3, -3, -3, -2, -3, -3, -3, -1,  0,  0, -3,  0,  6, -4, -2, -2,  1,  3, -1, -3, -3, -1, -4 },
/*P*/ { -1, -2, -2, -1, -3, -1, -1, -2, -2, -3, -3, -1, -2, -4,  7, -1, -1, -4, -3, -2, -2, -1, -2, -4 },
/*S*/ {  1, -1,  1,  0, -1,  0,  0,  0, -1, -2, -2,  0, -1, -2, -1,  4,  1, -3, -2, -2,  0,  0,  0, -4 },
/*T*/ {  0, -1,  0, -1, -1, -1, -1, -2, -2, -1, -1, -1, -1, -2, -1,  1,  5, -2, -2,  0, -1, -1,  0, -4 },
/*W*/ { -3, -3, -4, -4, -2, -2, -3, -2, -2, -3, -2, -3, -1,  1, -4, -3, -2, 11,  2, -3, -4, -3, -2, -4 },
/*Y*/ { -2, -2, -2, -3, -2, -1, -2, -3,  2, -1, -1, -2, -1,  3, -3, -2, -2,  2,  7, -1, -3, -2, -1, -4 },
/*V*/ {  0, -3, -3, -3, -1, -2, -2, -3, -3,  3,  1, -2,  1, -1, -2, -2,  0, -3, -1,  4, -3, -2, -1, -4 },
/*B*/ { -2, -1,  3,  4, -3,  0,  1, -1,  0, -3, -4,  0, -3, -3, -2,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
/*Z*/ { -1,  0,  0,  1, -3,  3,  4, -2,  0, -3, -3,  1, -1, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
/*X*/ {  0, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2,  0,  0, -2, -1, -1, -1, -1, -1, -4 },
/***/ { -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,  1 }
};

map < char, map < char, int > > Blosum62Map;

inline char ScreenResidueCharacter(char original)
{
    char ch = toupper(original);
    switch (ch) {
        case 'A': case 'R': case 'N': case 'D': case 'C':
        case 'Q': case 'E': case 'G': case 'H': case 'I':
        case 'L': case 'K': case 'M': case 'F': case 'P':
        case 'S': case 'T': case 'W': case 'Y': case 'V':
        case 'B': case 'Z':
            break;
        default:
            ch = 'X'; // make all but natural aa's just 'X'
    }
    return ch;
}

int GetBLOSUM62Score(char a, char b)
{
    if (Blosum62Map.size() == 0) {
        // initialize BLOSUM map for easy access
        for (int row=0; row<BLOSUMSIZE; row++) {
             for (int column=0; column<BLOSUMSIZE; column++)
                Blosum62Map[Blosum62Fields[row]][Blosum62Fields[column]] =
                    Blosum62Matrix[row][column];
        }
    }

    return Blosum62Map[ScreenResidueCharacter(a)][ScreenResidueCharacter(b)];
}

static int GetPSSMScore(const BLAST_Matrix *pssm, char ch, int masterIndex)
{
    if (!pssm || masterIndex < 0 || masterIndex >= pssm->rows) {
        ERRORMSG("GetPSSMScore() - invalid parameters");
        return 0;
    }
    return pssm->matrix[masterIndex][LookupBLASTResidueNumberFromCharacter(ch)];
}

static map < char, float > StandardProbabilities;

ConservationColorer::ConservationColorer(const BlockMultipleAlignment *parent) :
    nColumns(0), colorsCurrent(false), alignment(parent)
{
    if (StandardProbabilities.size() == 0) {  // initialize static stuff

        static const char ncbistdaa2char[26] = {
            'X', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'K', 'L', 'M',
            'N', 'P', 'Q', 'R', 'S', 'T', 'V', 'W', 'X', 'Y', 'Z', 'X', 'X'
        };

        // calculate expected residue frequencies (standard probabilities)
        // (code borrowed from SeqAlignInform() in cddutil.c)
        BLAST_ScoreBlkPtr sbp = BLAST_ScoreBlkNew(Seq_code_ncbistdaa, 1);
        BLAST_ResFreqPtr stdrfp = NULL;
        int a;
        if (!sbp) goto on_error;
        sbp->read_in_matrix = TRUE;
        sbp->protein_alphabet = TRUE;
        sbp->posMatrix = NULL;
        sbp->number_of_contexts = 1;
        Nlm_Char BLOSUM62[20];
        Nlm_StrCpy(BLOSUM62, "BLOSUM62");

        // test C-toolkit data directory registry - which BlastScoreBlkMatFill() uses
        Nlm_Char dataPath[255];
        if (Nlm_FindPath("ncbi", "ncbi", "data", dataPath, 255))
            TRACEMSG("FindPath() returned '" << dataPath << "'");
        else
            WARNINGMSG("FindPath() failed!");

        if (BlastScoreBlkMatFill(sbp, BLOSUM62) != 0) goto on_error;
        stdrfp = BlastResFreqNew(sbp);
        if (!stdrfp) goto on_error;
        if (BlastResFreqStdComp(sbp, stdrfp) != 0) goto on_error;
        for (a=1; a<23; a++) // from 'A' to 'Y'
            StandardProbabilities[ncbistdaa2char[a]] = (float) stdrfp->prob[a];
//        for (a=1; a<23; a++)
//            TESTMSG("std prob '" << ncbistdaa2char[a] << "' = "
//                << setprecision(10) << StandardProbabilities[ncbistdaa2char[a]]);
        goto cleanup;

on_error:
        ERRORMSG("ConservationColorer::ConservationColorer() - "
            "error initializing standard residue probabilities");

cleanup:
        if (stdrfp) BlastResFreqDestruct(stdrfp);
        if (sbp) BLAST_ScoreBlkDestruct(sbp);
    }
}

void ConservationColorer::AddBlock(const UngappedAlignedBlock *block)
{
    // sanity check
    if (!block->IsFrom(alignment)) {
        ERRORMSG("ConservationColorer::AddBlock : block is not from the associated alignment");
        return;
    }

    blocks[block].resize(block->width);

    // map block column position to profile position
    for (int i=0; i<block->width; i++) blocks[block][i] = nColumns + i;
    nColumns += block->width;

    colorsCurrent = false;
}

typedef map < char, int > ColumnProfile;

void ConservationColorer::CalculateConservationColors(void)
{
    if (colorsCurrent) return;

    TRACEMSG("calculating conservation colors");

    if (blocks.size() == 0) return;

    ColumnProfile profile;
    ColumnProfile::iterator p, pe, p2;
    int row, profileColumn;

    typedef vector < int > IntVector;
    IntVector varieties(nColumns, 0), weightedVarieties(nColumns, 0);
    identities.resize(nColumns);
    int minVariety, maxVariety, minWeightedVariety, maxWeightedVariety;

    typedef vector < float > FloatVector;
    FloatVector informationContents(nColumns, 0.0f);
    float totalInformationContent = 0.0f;

    int nRows = alignment->NRows();
    typedef map < char, int > CharIntMap;
    vector < CharIntMap > fitScores(nColumns);
    int minFit, maxFit;

    typedef map < const UngappedAlignedBlock * , FloatVector > BlockRowScores;
    BlockRowScores blockFitScores, blockZFitScores, blockRowFitScores;
    float minBlockFit, maxBlockFit, minBlockZFit, maxBlockZFit, minBlockRowFit, maxBlockRowFit;

    BlockMap::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        blockFitScores[b->first].resize(nRows, 0.0f);

        for (int blockColumn=0; blockColumn<b->first->width; blockColumn++) {
            profileColumn = b->second[blockColumn];

            // create profile for this column
            profile.clear();
            for (row=0; row<nRows; row++) {
                char ch = ScreenResidueCharacter(b->first->GetCharacterAt(blockColumn, row));
                if ((p=profile.find(ch)) != profile.end())
                    p->second++;
                else
                    profile[ch] = 1;
            }

            // identity for this column?
            if (profile.size() == 1 && profile.begin()->first != 'X')
                identities[profileColumn] = true;
            else
                identities[profileColumn] = false;

            // variety for this column
            int& variety = varieties[profileColumn];
            variety = profile.size();
            if (profile.find('X') != profile.end())
                variety += profile['X'] - 1; // each 'X' counts as one variety
            if (blockColumn == 0 && b == blocks.begin()) {
                minVariety = maxVariety = variety;
            } else {
                if (variety < minVariety) minVariety = variety;
                else if (variety > maxVariety) maxVariety = variety;
            }

            // weighted variety for this column
            pe = profile.end();
            int& weightedVariety = weightedVarieties[profileColumn];
            for (p=profile.begin(); p!=pe; p++) {
                weightedVariety +=
                    (p->second * (p->second - 1) / 2) * GetBLOSUM62Score(p->first, p->first);
                p2 = p;
                for (p2++; p2!=pe; p2++)
                    weightedVariety +=
                        p->second * p2->second * GetBLOSUM62Score(p->first, p2->first);
            }
            if (blockColumn == 0 && b == blocks.begin()) {
                minWeightedVariety = maxWeightedVariety = weightedVariety;
            } else {
                if (weightedVariety < minWeightedVariety) minWeightedVariety = weightedVariety;
                else if (weightedVariety > maxWeightedVariety) maxWeightedVariety = weightedVariety;
            }

            // information content (calculated in bits -> logs of base 2) and fit scores for this column
            pe = profile.end();
            float &columnInfo = informationContents[profileColumn];
            for (p=profile.begin(); p!=pe; p++) {
                static const float ln2 = (float) log(2.0), threshhold = 0.0001f;
                float residueScore = 0.0f, expFreq = StandardProbabilities[p->first];
                if (expFreq > threshhold) {
                    float obsFreq = 1.0f * p->second / nRows,
                          freqRatio = obsFreq / expFreq;
                    if (freqRatio > threshhold) {
                        residueScore = obsFreq * ((float) log(freqRatio)) / ln2;
                        columnInfo += residueScore; // information content
                    }
                }
                // fit score
                int& fit = fitScores[profileColumn][p->first];
                fit = GetPSSMScore(alignment->GetPSSM(),
                    p->first, b->first->GetRangeOfRow(0)->from + blockColumn);
                if (blockColumn == 0 && b == blocks.begin() && p == profile.begin()) {
                    minFit = maxFit = fit;
                } else {
                    if (fit < minFit) minFit = fit;
                    else if (fit > maxFit) maxFit = fit;
                }
            }
            totalInformationContent += columnInfo;

            // add up residue fit scores to get block fit scores
            for (row=0; row<nRows; row++) {
                char ch = ScreenResidueCharacter(b->first->GetCharacterAt(blockColumn, row));
                blockFitScores[b->first][row] += GetPSSMScore(alignment->GetPSSM(),
                    ch, b->first->GetRangeOfRow(0)->from + blockColumn);
            }
        }

        // find average/min/max block fit
        float average = 0.0f;
        for (row=0; row<nRows; row++) {
            float& score = blockFitScores[b->first][row];
            score /= b->first->width;   // average fit score across the block for this row
            average += score;
            if (row == 0 && b == blocks.begin()) {
                minBlockFit = maxBlockFit = score;
            } else {
                if (score < minBlockFit) minBlockFit = score;
                else if (score > maxBlockFit) maxBlockFit = score;
            }
        }
        average /= nRows;

        // calculate block Z scores from block fit scores
        if (nRows >= 2) {
            // calculate standard deviation of block fit score over all rows of this block
            float stdDev = 0.0f;
            for (row=0; row<nRows; row++)
                stdDev += (blockFitScores[b->first][row] - average) *
                          (blockFitScores[b->first][row] - average);
            stdDev /= nRows - 1;
            stdDev = sqrt(stdDev);
            if (stdDev > 1e-10) {
                // calculate Z scores for each row
                blockZFitScores[b->first].resize(nRows);
                for (row=0; row<nRows; row++)
                    blockZFitScores[b->first][row] = (blockFitScores[b->first][row] - average) / stdDev;
            }
        }
    }

    // calculate row fit scores based on Z-scores for each block across a given row
    if (blocks.size() >= 2) {
        for (b=blocks.begin(); b!=be; b++)
            blockRowFitScores[b->first].resize(nRows, kMin_Float);

        // calculate row average, standard deviation, and Z-scores
        for (row=0; row<nRows; row++) {
            float average = 0.0f;
            for (b=blocks.begin(); b!=be; b++)
                average += blockFitScores[b->first][row];
            average /= blocks.size();
            float stdDev = 0.0f;
            for (b=blocks.begin(); b!=be; b++)
                stdDev += (blockFitScores[b->first][row] - average) *
                          (blockFitScores[b->first][row] - average);
            stdDev /= blocks.size() - 1;
            stdDev = sqrt(stdDev);
            if (stdDev > 1e-10) {
                for (b=blocks.begin(); b!=be; b++)
                    blockRowFitScores[b->first][row] = (blockFitScores[b->first][row] - average) / stdDev;
            }
        }
    }

    INFOMSG("Total information content of aligned blocks: " << totalInformationContent << " bits");

    // now assign colors
    varietyColors.resize(nColumns);
    weightedVarietyColors.resize(nColumns);
    informationContentColors.resize(nColumns);
    fitColors.resize(nRows * nColumns);

    double scale;
    for (profileColumn=0; profileColumn<nColumns; profileColumn++) {

        // variety
        if (maxVariety == minVariety)
            scale = 1.0;
        else
            scale = 1.0 - 1.0 * (varieties[profileColumn] - minVariety) / (maxVariety - minVariety);
        varietyColors[profileColumn] = GlobalColors()->Get(Colors::eConservationMap, scale);

        // weighted variety
        if (maxWeightedVariety == minWeightedVariety)
            scale = 1.0;
        else
            scale = 1.0 * (weightedVarieties[profileColumn] - minWeightedVariety) /
                (maxWeightedVariety - minWeightedVariety);
        weightedVarietyColors[profileColumn] = GlobalColors()->Get(Colors::eConservationMap, scale);

        // information content, based on absolute scale
        static const float minInform = 0.10f, maxInform = 6.24f;
        scale = (informationContents[profileColumn] - minInform) / (maxInform - minInform);
        if (scale < 0.0) scale = 0.0;
        else if (scale > 1.0) scale = 1.0;
        scale = sqrt(scale);    // apply non-linearity so that lower values are better distinguished
        informationContentColors[profileColumn] = GlobalColors()->Get(Colors::eConservationMap, scale);

        // fit
        CharIntMap::const_iterator c, ce = fitScores[profileColumn].end();
        for (c=fitScores[profileColumn].begin(); c!=ce; c++) {
            if (maxFit == minFit)
                scale = 1.0;
            else
                scale = 1.0 * (c->second - minFit) / (maxFit - minFit);
            fitColors[profileColumn][c->first] = GlobalColors()->Get(Colors::eConservationMap, scale);
        }
    }

    // block fit
    blockFitColors.clear();
    for (b=blocks.begin(); b!=be; b++) {
        blockFitColors[b->first].resize(nRows);
        for (row=0; row<nRows; row++) {
            if (maxBlockFit == minBlockFit)
                scale = 1.0;
            else
                scale = 1.0 * (blockFitScores[b->first][row] - minBlockFit) / (maxBlockFit - minBlockFit);
            blockFitColors[b->first][row] = GlobalColors()->Get(Colors::eConservationMap, scale);
        }
    }

    // block Z fit
    blockZFitColors.clear();
    for (b=blocks.begin(); b!=be; b++) {
        blockZFitColors[b->first].resize(nRows, Vector(0,0,0));
        if (blockZFitScores.find(b->first) != blockZFitScores.end()) {  // if this column has scores
            for (row=0; row<nRows; row++) {                             // normalize colors per column
                float zScore = blockZFitScores[b->first][row];
                if (row == 0) {
                    minBlockZFit = maxBlockZFit = zScore;
                } else {
                    if (zScore < minBlockZFit) minBlockZFit = zScore;
                    else if (zScore > maxBlockZFit) maxBlockZFit = zScore;
                }
            }
            for (row=0; row<nRows; row++) {
                if (maxBlockZFit == minBlockZFit)
                    scale = 1.0;
                else
                    scale = 1.0 * (blockZFitScores[b->first][row] - minBlockZFit) /
                                        (maxBlockZFit - minBlockZFit);
                blockZFitColors[b->first][row] = GlobalColors()->Get(Colors::eConservationMap, scale);
            }
        }
    }

    // block row fit
    blockRowFitColors.clear();
    for (b=blocks.begin(); b!=be; b++)
        blockRowFitColors[b->first].resize(nRows, Vector(0,0,0));
    if (blocks.size() >= 2) {
        for (row=0; row<nRows; row++) {
            if (blockRowFitScores.begin()->second[row] != kMin_Float) { // if this row has fit scores
                for (b=blocks.begin(); b!=be; b++) {                    // normalize colors per row
                    float zScore = blockRowFitScores[b->first][row];
                    if (b == blocks.begin()) {
                        minBlockRowFit = maxBlockRowFit = zScore;
                    } else {
                        if (zScore < minBlockRowFit) minBlockRowFit = zScore;
                        else if (zScore > maxBlockRowFit) maxBlockRowFit = zScore;
                    }
                }
                for (b=blocks.begin(); b!=be; b++) {
                    if (maxBlockRowFit == minBlockRowFit)
                        scale = 1.0;
                    else
                        scale = 1.0 * (blockRowFitScores[b->first][row] - minBlockRowFit) /
                                            (maxBlockRowFit - minBlockRowFit);
                    blockRowFitColors[b->first][row] = GlobalColors()->Get(Colors::eConservationMap, scale);
                }
            }
        }
    }

    colorsCurrent = true;
}

void ConservationColorer::GetProfileIndexAndResidue(
    const UngappedAlignedBlock *block, int blockColumn, int row,
    int *profileIndex, char *residue) const
{
    BlockMap::const_iterator b = blocks.find(block);
    *profileIndex = b->second[blockColumn];
    *residue = ScreenResidueCharacter(b->first->GetCharacterAt(blockColumn, row));
}

void ConservationColorer::Clear(void)
{
    nColumns = 0;
    blocks.clear();
    FreeColors();
    colorsCurrent = false;
}

void ConservationColorer::FreeColors(void)
{
    identities.clear();
    varietyColors.clear();
    weightedVarietyColors.clear();
    informationContentColors.clear();
    fitColors.clear();
    colorsCurrent = false;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2003/02/12 15:36:14  thiessen
* clean up fit scoring code
*
* Revision 1.28  2003/02/06 16:39:53  thiessen
* add block row fit coloring
*
* Revision 1.27  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.26  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.25  2003/01/30 14:00:23  thiessen
* add Block Z Fit coloring
*
* Revision 1.24  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.23  2002/07/23 16:40:39  thiessen
* remove static decl
*
* Revision 1.22  2002/07/23 15:46:50  thiessen
* print out more BLAST info; tweak save file name
*
* Revision 1.21  2002/04/27 16:32:12  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.20  2002/03/01 15:47:10  thiessen
* fix bug in fit coloring for non-standard residues
*
* Revision 1.19  2002/02/13 19:45:29  thiessen
* Fit coloring by info content contribution
*
* Revision 1.18  2001/09/27 15:37:58  thiessen
* decouple sequence import and BLAST
*
* Revision 1.17  2001/08/24 00:41:35  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.16  2001/08/13 22:30:59  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.15  2001/06/04 14:58:00  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.14  2001/06/02 17:22:45  thiessen
* fixes for GCC
*
* Revision 1.13  2001/05/31 18:47:08  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.12  2001/05/25 01:38:16  thiessen
* minor fixes for compiling on SGI
*
* Revision 1.11  2001/05/22 22:37:13  thiessen
* check data registry path
*
* Revision 1.10  2001/05/22 19:09:30  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.9  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.8  2001/02/22 00:30:06  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.7  2001/02/15 00:03:54  thiessen
* print out total information content
*
* Revision 1.6  2001/02/13 20:33:49  thiessen
* add information content coloring
*
* Revision 1.5  2000/11/30 15:49:38  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.4  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.3  2000/10/16 14:25:48  thiessen
* working alignment fit coloring
*
* Revision 1.2  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.1  2000/09/20 22:24:48  thiessen
* working conservation coloring; split and center unaligned justification
*
*/
