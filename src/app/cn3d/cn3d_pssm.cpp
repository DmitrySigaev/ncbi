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
*      new C++ PSSM construction
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>

#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_encoding.h>

#include <objects/scoremat/scoremat__.hpp>

// conflicts between algo/blast stuff and C-toolkit stuff
#undef INT1_MIN
#undef INT1_MAX
#undef INT2_MIN
#undef INT2_MAX

#include "cn3d_pssm.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);


BEGIN_SCOPE(Cn3D)

//#define DEBUG_PSSM 1 // for testing/debugging PSSM data

#define PTHROW(stream) NCBI_THROW(CException, eUnknown, stream)

class Cn3DPSSMInput : public IPssmInputData
{
private:
    const BlockMultipleAlignment *bma;
    unsigned char *masterNCBIStdaa;
    unsigned int masterLength;

    PSIMsa *data;
    PSIBlastOptions *options;
    PSIDiagnosticsRequest diag;

public:
    Cn3DPSSMInput(const BlockMultipleAlignment *b);
    ~Cn3DPSSMInput(void);

    // IPssmInputData required functions
    void Process(void) { }  // all done in c'tor
    unsigned char * GetQuery(void) { return masterNCBIStdaa; }
    unsigned int GetQueryLength(void) { return masterLength; }
    PSIMsa * GetData(void) { return data; }
    const PSIBlastOptions* GetOptions(void) { return options; }
    const char * GetMatrixName(void) { return "BLOSUM62"; }
    const PSIDiagnosticsRequest * GetDiagnosticsRequest(void) { return &diag; }
};

static const unsigned char gap = LookupNCBIStdaaNumberFromCharacter('-');

static void FillInAlignmentData(const BlockMultipleAlignment *bma, PSIMsa *data)
{
    if (data->dimensions->query_length != bma->GetMaster()->Length() || data->dimensions->num_seqs != bma->NRows() - 1)
        PTHROW("FillInAlignmentData() - data array size mismatch");

    BlockMultipleAlignment::ConstBlockList blocks;
    bma->GetBlocks(&blocks);

    unsigned int b, row, column, masterStart, masterWidth, slaveStart, slaveWidth, left, right, middle;
    const Block::Range *range;
    const Sequence *seq;

    for (b=0; b<blocks.size(); ++b) {
        const Block& block = *(blocks[b]);

        seq = bma->GetSequenceOfRow(0);
        range = block.GetRangeOfRow(0);
        if (range->from < 0 || range->from > (int)seq->Length() || range->to < -1 || range->to >= (int)seq->Length() ||
                range->to < range->from - 1 ||
                range->from != ((b == 0) ? 0 : (blocks[b - 1]->GetRangeOfRow(0)->to + 1)) ||
                (b == blocks.size() - 1 && range->to != seq->Length() - 1))
            PTHROW("FillInAlignmentData() - master range error");
        masterStart = range->from;
        masterWidth = range->to - range->from + 1;

        for (row=0; row<bma->NRows(); row++) {
            seq = bma->GetSequenceOfRow(row);
            range = block.GetRangeOfRow(row);
            if (range->from < 0 || range->from > (int)seq->Length() || range->to < -1 || range->to >= (int)seq->Length() ||
                    range->to < range->from - 1 ||
                    range->from != ((b == 0) ? 0 : (blocks[b - 1]->GetRangeOfRow(row)->to + 1)) ||
                    (b == blocks.size() - 1 && range->to != seq->Length() - 1))
                PTHROW("FillInAlignmentData() - slave range error");
            slaveStart = range->from;
            slaveWidth = range->to - range->from + 1;

            for (column=0; column<masterWidth; ++column) {
                PSIMsaCell& cell = data->data[row][masterStart + column];

                // same # residues in both master and slave
                if (slaveWidth == masterWidth) {
                    cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    cell.is_aligned = true;
                }

                // left tail
                else if (b == 0) {
                    // truncate left end of sequence
                    if (slaveWidth > masterWidth) {
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveWidth - masterWidth + column]);
                        cell.is_aligned = true;
                    } else {
                        // residues to the right
                        if (column >= masterWidth - slaveWidth) {
                            cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[column - (masterWidth - slaveWidth)]);
                            cell.is_aligned = true;
                        }
                        // pad left with unaligned gaps
                        else {
                            cell.letter = gap;
                            cell.is_aligned = false;
                        }
                    }
                }

                // right tail
                else if (b == blocks.size() - 1) {
                    // truncate right end of sequence
                    if (slaveWidth > masterWidth) {
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                        cell.is_aligned = true;
                    } else {
                        // residues to the left
                        if (column < slaveWidth) {
                            cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                            cell.is_aligned = true;
                        }
                        // pad left with unaligned gaps
                        else {
                            cell.letter = gap;
                            cell.is_aligned = false;
                        }
                    }
                }

                // more residues in master than slave: split and pad middle with (aligned) gaps
                else if (slaveWidth < masterWidth) {
                    if (column == 0) {
                        left = (slaveWidth + 1) / 2;    // +1 means left gets more in uneven split
                        middle = masterWidth - slaveWidth;
                        right = slaveWidth - left;
                    }
                    if (column < left)
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    else if (column < left + middle)
                        cell.letter = gap;
                    else
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column - middle]);
                    cell.is_aligned = true;             // even gaps are aligned in these regions
                }

                // more residues in slave than master: truncate middle of slave
                else {  // slaveWidth > masterWidth
                    if (column == 0) {
                        left = (masterWidth + 1) / 2;   // +1 means left gets more in uneven split
                        right = masterWidth - left;
                    }
                    if (column < left)
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    else
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(
                            seq->sequenceString[slaveStart + slaveWidth - masterWidth + column]);
                    cell.is_aligned = true;
                }
            }
        }
    }
}

double GetStandardProbability(char ch)
{
    typedef map < char, double > CharDoubleMap;
    static CharDoubleMap standardProbabilities;

    if (standardProbabilities.size() == 0) {  // initialize static stuff
        if (BLASTAA_SIZE != 26) {
            ERRORMSG("GetStandardProbability() - confused by BLASTAA_SIZE != 26");
            return 0.0;
        }
        double *probs = BLAST_GetStandardAaProbabilities();
        for (unsigned int i=0; i<26; ++i) {
            standardProbabilities[LookupCharacterFromNCBIStdaaNumber(i)] = probs[i];
#ifdef DEBUG_PSSM
            TRACEMSG("standard probability " << LookupCharacterFromNCBIStdaaNumber(i) << " : " << probs[i]);
#endif
        }
        sfree(probs);
    }

    CharDoubleMap::const_iterator f = standardProbabilities.find(toupper((unsigned char) ch));
    if (f != standardProbabilities.end())
        return f->second;
    WARNINGMSG("GetStandardProbability() - unknown residue character " << ch);
    return 0.0;
}

static double CalculateInformationContent(const PSIMsa *data, bool ignoreMaster)
{
    double infoContent = 0.0;

    typedef map < unsigned char, unsigned int > ColumnProfile;
    ColumnProfile profile;
    ColumnProfile::iterator p, pe;
    unsigned int nRes;

#ifdef DEBUG_PSSM
    CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out | IOS_BASE::app);
#endif

    for (unsigned int c=0; c<data->dimensions->query_length; ++c) {

        profile.clear();
        nRes = 0;

        // build profile
        for (unsigned int r=(ignoreMaster ? 1 : 0); r<=data->dimensions->num_seqs; ++r) {
            if (data->data[r][c].letter != gap) {                       // don't include gaps
                p = profile.find(data->data[r][c].letter);
                if (p == profile.end())
                    profile[data->data[r][c].letter] = 1;
                else
                    ++(p->second);
                ++nRes;
            }
        }

#ifdef DEBUG_PSSM
        if (ofs)
            ofs << "column " << (c+1) << ", total " << nRes;
        double columnContent = 0.0;
#endif

        // do info content
        static const double ln2 = log(2.0), threshhold = 0.0001;
        for (p=profile.begin(), pe=profile.end(); p!=pe; ++p) {
            double expFreq = GetStandardProbability(LookupCharacterFromNCBIStdaaNumber(p->first));
            if (expFreq > threshhold) {
                double obsFreq = 1.0 * p->second / nRes,
                       freqRatio = obsFreq / expFreq;
                if (freqRatio > threshhold) {
                    infoContent += obsFreq * log(freqRatio) / ln2;

#ifdef DEBUG_PSSM
                    columnContent += obsFreq * log(freqRatio) / ln2;
                    if (ofs)
                        ofs << ", " << LookupCharacterFromNCBIStdaaNumber(p->first) << '(' << p->second << ") "
                            << setprecision(3) << (obsFreq * log(freqRatio) / ln2);
#endif
                }
            }
        }
#ifdef DEBUG_PSSM
        if (ofs)
            ofs << ", sum info: " << setprecision(6) << columnContent << '\n';
#endif
    }

    TRACEMSG("information content: " << infoContent);
    return infoContent;
}

Cn3DPSSMInput::Cn3DPSSMInput(const BlockMultipleAlignment *b) : bma(b)
{
    TRACEMSG("Creating Cn3DPSSMInput structure");

    // encode master
    masterLength = bma->GetMaster()->Length();
    masterNCBIStdaa = new unsigned char[masterLength];
    for (unsigned int i=0; i<masterLength; ++i)
        masterNCBIStdaa[i] = LookupNCBIStdaaNumberFromCharacter(bma->GetMaster()->sequenceString[i]);

    // create PSIMsa
    PSIMsaDimensions dim;
    dim.query_length = bma->GetMaster()->Length();
    dim.num_seqs = bma->NRows() - 1;    // not including master
    data = PSIMsaNew(&dim);
    FillInAlignmentData(bma, data);

    // set up PSIDiagnosticsRequest
    diag.information_content = false;
    diag.residue_frequencies = false;
    diag.weighted_residue_frequencies = false;
    diag.frequency_ratios = true;      // true to match cdtree
    diag.gapless_column_weights = false;

    // create PSIBlastOptions
    PSIBlastOptionsNew(&options);
    options->nsg_compatibility_mode = false;    // false for now, since we're not using a consensus
    double infoContent = CalculateInformationContent(data, false);
    if      (infoContent > 84  ) options->pseudo_count = 10;
    else if (infoContent > 55  ) options->pseudo_count =  7;
    else if (infoContent > 43  ) options->pseudo_count =  5;
    else if (infoContent > 41.5) options->pseudo_count =  4;
    else if (infoContent > 40  ) options->pseudo_count =  3;
    else if (infoContent > 39  ) options->pseudo_count =  2;
    else                         options->pseudo_count =  1;

#ifdef DEBUG_PSSM
    CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out | IOS_BASE::app);
    if (ofs) {
//        diag.residue_frequencies = true;
        ofs << "information content: " << setprecision(6) << infoContent << '\n'
            << "pseudocount: " << options->pseudo_count << '\n'
            << "query length: " << GetQueryLength() << '\n'
            << "query: ";
        for (unsigned int i=0; i<GetQueryLength(); ++i)
            ofs << LookupCharacterFromNCBIStdaaNumber(GetQuery()[i]);
        ofs << "\nmatrix name: " << GetMatrixName() << '\n'
            << "options->pseudo_count: " << options->pseudo_count << '\n'
            << "options->inclusion_ethresh: " << options->inclusion_ethresh << '\n'
            << "options->use_best_alignment: " << (int) options->use_best_alignment << '\n'
            << "options->nsg_compatibility_mode: " << (int) options->nsg_compatibility_mode << '\n'
            << "options->impala_scaling_factor: " << options->impala_scaling_factor << '\n'
            << "diag->information_content: " << (int) GetDiagnosticsRequest()->information_content << '\n'
            << "diag->residue_frequencies: " << (int) GetDiagnosticsRequest()->residue_frequencies << '\n'
            << "diag->weighted_residue_frequencies: " << (int) GetDiagnosticsRequest()->weighted_residue_frequencies << '\n'
            << "diag->frequency_ratios: " << (int) GetDiagnosticsRequest()->frequency_ratios << '\n'
            << "diag->gapless_column_weights: " << (int) GetDiagnosticsRequest()->gapless_column_weights << '\n'
            << "num_seqs: " << data->dimensions->num_seqs << ", query_length: " << data->dimensions->query_length << '\n';
        for (unsigned int row=0; row<=data->dimensions->num_seqs; ++row) {
            for (unsigned int column=0; column<data->dimensions->query_length; ++column)
                ofs << LookupCharacterFromNCBIStdaaNumber(data->data[row][column].letter);
            ofs << '\n';
        }
        for (unsigned int row=0; row<=data->dimensions->num_seqs; ++row) {
            for (unsigned int column=0; column<data->dimensions->query_length; ++column)
                ofs << (data->data[row][column].is_aligned ? 'A' : 'U');
            ofs << '\n';
        }
    }
#endif
}

Cn3DPSSMInput::~Cn3DPSSMInput(void)
{
    PSIMsaFree(data);
    PSIBlastOptionsFree(options);
    delete[] masterNCBIStdaa;
}

static BLAST_Matrix * ConvertPSSMToBLASTMatrix(const CPssmWithParameters& pssm)
{
    TRACEMSG("converting CPssmWithParameters to BLAST_Matrix");

    if (!pssm.GetPssm().IsSetFinalData())
        PTHROW("ConvertPSSMToBLASTMatrix() - pssm must have finalData");
    unsigned int nScores = pssm.GetPssm().GetNumRows() * pssm.GetPssm().GetNumColumns();
    if (pssm.GetPssm().GetNumRows() != 26 || pssm.GetPssm().GetFinalData().GetScores().size() != nScores)
        PTHROW("ConvertPSSMToBLASTMatrix() - bad matrix size");

    BLAST_Matrix *matrix = (BLAST_Matrix *) MemNew(sizeof(BLAST_Matrix));

    // set BLAST_Matrix values
    matrix->is_prot = pssm.GetPssm().GetIsProtein();
    matrix->name = NULL;
    matrix->rows = pssm.GetPssm().GetNumColumns() + 1;  // rows and columns are reversed in pssm vs BLAST_Matrix
    matrix->columns = pssm.GetPssm().GetNumRows();
    matrix->posFreqs = NULL;
    if (pssm.GetPssm().GetFinalData().IsSetKappa())
        matrix->karlinK = pssm.GetPssm().GetFinalData().GetKappa();
    else
        ERRORMSG("ConvertPSSMToBLASTMatrix() - missing Kappa");
    matrix->original_matrix = NULL;

    // allocate matrix
    matrix->matrix = (Int4Ptr *) MemNew(matrix->rows * sizeof(Int4Ptr));
    unsigned int i;
    for (i=0; (int)i<matrix->rows; ++i)
        matrix->matrix[i] = (Int4Ptr) MemNew(matrix->columns * sizeof(Int4));

    // convert matrix
    unsigned int r = 0, c = 0;
    CPssmFinalData::TScores::const_iterator s = pssm.GetPssm().GetFinalData().GetScores().begin();
    for (i=0; i<nScores; ++i, ++s) {

        matrix->matrix[r][c] = *s;

        // adjust for matrix layout in pssm
        if (pssm.GetPssm().GetByRow()) {
            ++r;
            if (r == pssm.GetPssm().GetNumColumns()) {
                ++c;
                r = 0;
            }
        } else {
            ++c;
            if (c == pssm.GetPssm().GetNumRows()) {
                ++r;
                c = 0;
            }
        }
    }

    // Set the last row to BLAST_SCORE_MIN
    for (i=0; (int)i<matrix->columns; i++)
        matrix->matrix[matrix->rows - 1][i] = BLAST_SCORE_MIN;

#ifdef DEBUG_PSSM
    CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out | IOS_BASE::app);
    if (ofs) {
        ofs << "matrix->is_prot: " << (int) matrix->is_prot << '\n'
            << "matrix->name: " << (matrix->name ? matrix->name : "(none)") << '\n'
            << "matrix->rows: " << matrix->rows << '\n'
            << "matrix->columns: " << matrix->columns << '\n';
        for (r=0; (int)r<matrix->rows; ++r) {
            for (c=0; (int)c<matrix->columns; ++c)
                ofs << matrix->matrix[r][c] << ' ';
            ofs << '\n';
        }
    }
#endif

    return matrix;
}

BLAST_Matrix * CreateBlastMatrix(const BlockMultipleAlignment *bma)
{
#ifdef DEBUG_PSSM
    {{
        CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out);
    }}
#endif

    BLAST_Matrix *matrix = NULL;
    try {
        Cn3DPSSMInput input(bma);
        CPssmEngine engine(&input);
        CRef < CPssmWithParameters > pssm = engine.Run();

#ifdef DEBUG_PSSM
        CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out | IOS_BASE::app);
        if (ofs) {
            CObjectOStreamAsn oosa(ofs, false);
            oosa << *pssm;

/*
            if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetResFreqsPerPos()) {
                vector < int >
                    freqs(pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().size()),
                    nNonGap(pssm->GetPssm().GetNumColumns(), 0);
                unsigned int i;
                CPssmIntermediateData::TResFreqsPerPos::const_iterator
                    l = pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().begin();
                for (i=0; i<pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().size(); ++i, ++l)
                    freqs[i] = *l;
                int freq, n;
                ofs << "observed frequencies:\n";
                for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                    ofs << "column " << (c+1) << ": ";
                    n = 0;
                    for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                        if (pssm->GetPssm().GetByRow())
                            freq = freqs[r * pssm->GetPssm().GetNumColumns() + c];
                        else
                            freq = freqs[c * pssm->GetPssm().GetNumRows() + r];
                        if (freq > 0) {
                            ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << freq << ") ";
                            n += freq;
                            if (r != 0)
                                nNonGap[c] += freq;
                        }
                    }
                    ofs << "total: " << n << " non-gap: " << nNonGap[c] << '\n';
                }

                if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetWeightedResFreqsPerPos()) {
                    vector < double > wfreqs(pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().size());
                    CPssmIntermediateData::TWeightedResFreqsPerPos::const_iterator
                        m = pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().begin();
                    for (i=0; i<pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().size(); ++i, ++m)
                        wfreqs[i] = *m;
                    double wfreq, s;
                    ofs << "weighted frequencies:\n";
                    for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                        ofs << "column " << (c+1) << ": ";
                        s = 0.0;
                        for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                            if (pssm->GetPssm().GetByRow())
                                wfreq = wfreqs[r * pssm->GetPssm().GetNumColumns() + c];
                            else
                                wfreq = wfreqs[c * pssm->GetPssm().GetNumRows() + r];
                            if (wfreq != 0.0) {
                                ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << wfreq << ") ";
                                s += wfreq;
                            }
                        }
                        ofs << "sum: " << s << '\n';
                    }
                }

                if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetFreqRatios()) {
                    vector < double > ratios(pssm->GetPssm().GetIntermediateData().GetFreqRatios().size());
                    CPssmIntermediateData::TFreqRatios::const_iterator
                        n = pssm->GetPssm().GetIntermediateData().GetFreqRatios().begin();
                    for (i=0; i<pssm->GetPssm().GetIntermediateData().GetFreqRatios().size(); ++i, ++n)
                        ratios[i] = *n;
                    double ratio, s;
                    ofs << "frequency ratios:\n";
                    for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                        ofs << "column " << (c+1) << ": ";
                        s = 0.0;
                        for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                            if (pssm->GetPssm().GetByRow())
                                ratio = ratios[r * pssm->GetPssm().GetNumColumns() + c];
                            else
                                ratio = ratios[c * pssm->GetPssm().GetNumRows() + r];
                            if (ratio != 0.0) {
                                ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << ratio << ") ";
                                s += ratio;
                            }
                        }
                        ofs << "sum: " << s << '\n';
                    }
                }
            }
*/
        }
#endif

        matrix = ConvertPSSMToBLASTMatrix(*pssm);
    } catch (exception& e) {
        ERRORMSG("CreateBlastMatrix() failed with exception: " << e.what());
    }
    return matrix;
}

void OutputPSSM(const BlockMultipleAlignment *bma, CNcbiOstream& os)
{
    try {
        Cn3DPSSMInput input(bma);
        CPssmEngine engine(&input);
        CRef < CPssmWithParameters > pssm = engine.Run();
        pssm->SetPssm().SetQuery().SetSeq().Assign(bma->GetMaster()->bioseqASN.GetObject());
        CObjectOStreamAsn osa(os, false);
        osa << *pssm;
    } catch (exception& e) {
        ERRORMSG("OutputPSSM() failed with exception: " << e.what());
    }
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2005/10/25 17:41:35  thiessen
* fix flicker in alignment display; add progress meter and misc fixes to refiner
*
* Revision 1.8  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.7  2005/09/26 14:42:24  camacho
* Renamed blast_psi.hpp -> pssm_engine.hpp
*
* Revision 1.6  2005/06/07 12:18:52  thiessen
* add PSSM export
*
* Revision 1.5  2005/06/03 16:25:24  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.4  2005/04/21 14:31:19  thiessen
* add MonitorAlignments()
*
* Revision 1.3  2005/03/25 15:10:45  thiessen
* matched self-hit E-values with CDTree2
*
* Revision 1.2  2005/03/08 22:09:51  thiessen
* use proper default options
*
* Revision 1.1  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
*/
