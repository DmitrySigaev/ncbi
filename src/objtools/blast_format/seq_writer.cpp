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
 * Author: Christiam Camacho
 *
 */

/** @file seq_writer.cpp
 *  Implementation of the CSeqFormatter class
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast_format/seq_writer.hpp>
#include <objtools/blast_format/blastdb_dataextract.hpp>
#include <algo/blast/blastinput/blast_input.hpp>    // for CInputException
#include "masking_fmt_spec.hpp"
#include <numeric>      // for std::accumulate

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
USING_SCOPE(objects);

const char CMaskingFmtSpecHelper::kDelim(',');
const string CMaskingFmtSpecHelper::kDelimStr(1, CMaskingFmtSpecHelper::kDelim);

CSeqFormatter::CSeqFormatter(const string& format_spec, CSeqDB& blastdb,
                 CNcbiOstream& out, 
                 CSeqFormatterConfig config /* = CSeqFormatterConfig() */)
    : m_Out(out), m_FmtSpec(format_spec), m_BlastDb(blastdb),
      m_MaskingAlgoHelper(0), m_FastaRequestedAtEOL(false)
{
    auto_ptr<CMaskingFmtSpecHelper> masking_algo_helper
		(new CMaskingFmtSpecHelper(m_FmtSpec, m_BlastDb));
    vector<char> repl_types;    // replacement types

    // Record where the offsets where the replacements must occur
    for (SIZE_TYPE i = 0; i < m_FmtSpec.size(); i++) {
        if (m_FmtSpec[i] == '%' && m_FmtSpec[i+1] == '%') {
            // remove the escape character for '%'
            m_FmtSpec.erase(i++, 1);
            continue;
        }

        if (m_FmtSpec[i] == '%') {
            m_ReplOffsets.push_back(i);
            repl_types.push_back(m_FmtSpec[i+1]);
        }
    }

    if (m_ReplOffsets.empty() || repl_types.size() != m_ReplOffsets.size()) {
        NCBI_THROW(CInputException, eInvalidInput,
                   "Invalid format specification");
    }

    // Validate filtering algorithms provided, if any
    if ( !config.m_FiltAlgoIds.empty() ) {
        CMaskingFmtSpecHelper::ValidateFilteringAlgorithms
            (m_BlastDb, config.m_FiltAlgoIds);
    }

    ITERATE(vector<char>, fmt, repl_types) {
        switch (*fmt) {
        case 'f':
            m_DataExtractors.push_back
                (new CFastaExtractor(config.m_LineWidth, 
                                     config.m_SeqRange,
                                     config.m_Strand, 
                                     config.m_TargetOnly, 
                                     config.m_UseCtrlA,
                                     config.m_FiltAlgoIds));
            break;

        case 's':
            m_DataExtractors.push_back
                (new CSeqDataExtractor(config.m_SeqRange, 
                                       config.m_Strand, 
                                       config.m_FiltAlgoIds));
            break;

        case 'a':
            m_DataExtractors.push_back
                (new CAccessionExtractor(config.m_TargetOnly));
            break;

        case 'g':
            m_DataExtractors.push_back(new CGiExtractor());
            break;

        case 'o':
            m_DataExtractors.push_back(new COidExtractor());
            break;

        case 't':
            m_DataExtractors.push_back(new CTitleExtractor(config.m_TargetOnly));
            break;

        case 'l':
            m_DataExtractors.push_back(new CSeqLenExtractor());
            break;

        case 'T':
            m_DataExtractors.push_back(new CTaxIdExtractor());
            break;

        case 'P':
            m_DataExtractors.push_back(new CPigExtractor());
            break;

        case 'L':
            m_DataExtractors.push_back(new CCommonTaxonomicNameExtractor());
            break;

        case 'S':
            m_DataExtractors.push_back(new CScientificNameExtractor());
            break;

        case 'm':
            {
                _ASSERT(masking_algo_helper.get());
                vector<int> filt_algo_ids = 
                    masking_algo_helper->GetFilteringAlgorithms();
                m_DataExtractors.push_back
                    (new CMaskingDataExtractor(filt_algo_ids));
                break;
            }

        default:
            CNcbiOstrstream os;
            os << "Unrecognized format specification: '%" << *fmt << "'";
            NCBI_THROW(CInputException, eInvalidInput, 
                       CNcbiOstrstreamToString(os));
        }
    }

    // This is needed for pretty formatting purposes
    if (dynamic_cast<CFastaExtractor*>(m_DataExtractors.back())) {
        m_FastaRequestedAtEOL = true;
    }
	m_MaskingAlgoHelper = masking_algo_helper.release();
}

CSeqFormatter::~CSeqFormatter()
{
    delete m_MaskingAlgoHelper;
    NON_CONST_ITERATE(vector<IBlastDBExtract*>, itr, m_DataExtractors) {
        delete *itr;
    };
}

void
CSeqFormatter::Write(CBlastDBSeqId& id)
{
    vector<string> data2write;
    x_Transform(id, data2write);
    m_Out << x_Replacer(data2write);
    if ( !m_FastaRequestedAtEOL ) {
        m_Out << "\n";
    }
}

void
CSeqFormatter::x_Transform(CBlastDBSeqId& id, vector<string>& retval)
{
    retval.clear();
    retval.reserve(m_DataExtractors.size());

    // for every type of specification, call the extractor
    NON_CONST_ITERATE(vector<IBlastDBExtract*>, extractor, m_DataExtractors) {
        retval.push_back((*extractor)->Extract(id, m_BlastDb));
    }
}

/// Auxiliary functor to compute the length of a string
struct StrLenAdd : public binary_function<SIZE_TYPE, const string&, SIZE_TYPE>
{
    SIZE_TYPE operator() (SIZE_TYPE a, const string& b) const {
        return a + b.size();
    }
};

string
CSeqFormatter::x_Replacer(const vector<string>& data2write) const
{
    const string& kFiltAlgoSpec =
        m_MaskingAlgoHelper->GetFiltAlgorithmSpecifiers();
    SIZE_TYPE data2write_size = accumulate(data2write.begin(), data2write.end(),
                                           0, StrLenAdd());

    string retval;
    retval.reserve(m_FmtSpec.size() + data2write_size -
                   (m_DataExtractors.size() * 2));

    SIZE_TYPE fmt_idx = 0;
    for (SIZE_TYPE i = 0, kSize = m_ReplOffsets.size(); i < kSize; i++) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_ReplOffsets[i]]);
        retval.append(data2write[i]);
        fmt_idx = m_ReplOffsets[i] + 2;
        if (m_FmtSpec[m_ReplOffsets[i]+1] == 'm') {
            fmt_idx += kFiltAlgoSpec.size();
        }
    }
    if (fmt_idx <= m_FmtSpec.size()) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_FmtSpec.size()]);
    }

    return retval;
}

END_NCBI_SCOPE
