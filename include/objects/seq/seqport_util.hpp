#ifndef OBJECTS_SEQ___SEQPORT_UTIL__HPP
#define OBJECTS_SEQ___SEQPORT_UTIL__HPP

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
 * Author:  Clifford Clausen
 *          (also reviewed/fixed/groomed by Denis Vakatov and Aaron Ucko)
 *
 * File Description:
 */   

#include <objects/seq/Seq_data.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <util/random_gen.hpp>
#include <memory>
#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


// CSeqportUtil is a wrapper for a hidden object of class
// CSeqportUtil_implementation.

class CSeqportUtil_implementation;


class NCBI_SEQ_EXPORT CSeqportUtil
{
public:

    // TypeDefs
    typedef unsigned int TIndex;
    typedef pair<TIndex, TIndex> TPair;
    
    // Classes thrown as errors
    struct NCBI_SEQ_EXPORT CBadIndex : public runtime_error
    {
        CBadIndex() : runtime_error("CSeqportUtil: bad index specified") {}
    };
    struct NCBI_SEQ_EXPORT CBadSymbol : public runtime_error 
    {
        CBadSymbol() : runtime_error("CSeqportUtil: bad symbol specified") {}
    };
    struct NCBI_SEQ_EXPORT CBadType : public runtime_error 
    {
        CBadType() : runtime_error("CSeqportUtil: specified code or code "
                                   "combination not supported") {}
    };
    
    // Alphabet conversion function. Function returns the
    // number of converted codes.
    static TSeqPos Convert(const CSeq_data&       in_seq,
                           CSeq_data*             out_seq,
                           CSeq_data::E_Choice    to_code,
                           TSeqPos                uBeginIdx = 0,
                           TSeqPos                uLength   = 0,
                           bool                   bAmbig    = false,
                           CRandom::TValue        seed      = 17734276);

    // Function to provide maximum in-place packing of na
    // sequences without loss of information. Iupacna
    // can always be packed to ncbi4na without loss. Iupacna
    // can sometimes be packed to ncbi2na. Ncbi4na can
    // sometimes be packed to ncbi2na. Returns number of
    // residues packed. If in_seq cannot be packed, the
    // original in_seq is returned unchanged and the return value
    // from Pack is 0
    static TSeqPos Pack(CSeq_data*   in_seq,
                        TSeqPos      uBeginidx = 0,
                        TSeqPos      uLength   = 0);

    // Performs fast validation of CSeq_data. If all data in the
    // sequence represent valid elements of a biological sequence, then
    // FastValidate returns true. Otherwise it returns false
    static bool FastValidate(const CSeq_data&   in_seq,
                             TSeqPos            uBeginIdx = 0,
                             TSeqPos            uLength   = 0);

    // Performs validation of CSeq_data. Returns a list of indices
    // corresponding to data that does not represent a valid element
    // of a biological sequence.
    static void Validate(const CSeq_data&   in_seq,
                         vector<TSeqPos>*   badIdx,
                         TSeqPos            uBeginIdx = 0,
                         TSeqPos            uLength   = 0);

    // Get ambiguous bases. out_indices returns
    // the indices relative to in_seq of ambiguous bases.
    // out_seq returns the ambiguous bases. Note, there are
    // only ambiguous bases for iupacna->ncib2na and
    // ncib4na->ncbi2na coversions.
    static TSeqPos GetAmbigs(const CSeq_data&    in_seq,
                             CSeq_data*          out_seq,
                             vector<TSeqPos>*    out_indices,
                             CSeq_data::E_Choice to_code = CSeq_data::e_Ncbi2na,
                             TSeqPos             uBeginIdx = 0,
                             TSeqPos             uLength   = 0);

    // Get a copy of CSeq_data. No conversion is done. uBeginIdx of the
    // biological sequence in in_seq will be in position
    // 0 of out_seq. Usually, uLength bases will be copied
    // from in_seq to out_seq. If uLength goes beyond the end of
    // in_seq, it will be shortened to go to the end of in_seq.
    // For packed sequence formats (ncbi2na and ncbi4na),
    // only uLength bases are valid copies. For example,
    // in an ncbi4na encoded sequence, if uLength is odd, the last
    // sequence returned will be uLength+1 because 2 bases are encoded
    // per byte in ncbi4na. However, in this case, uLength will be returned
    // unchanged (it will remain odd unless it goes beyond the end
    // of in_seq). If uLength=0, then a copy from uBeginIdx to the end
    // of in_seq is returned.
    static TSeqPos GetCopy(const CSeq_data&   in_seq,
                           CSeq_data*         out_seq,
                           TSeqPos            uBeginIdx = 0,
                           TSeqPos            uLength   = 0);

    // Method to keep only a contiguous piece of a sequence beginning
    // at uBeginIdx and uLength residues long. Does bit shifting as
    // needed to put uBeginIdx of original sequence at position zero on output.
    // Similar to GetCopy(), but done in place.  Returns length of
    // kept sequence.
    static TSeqPos Keep(CSeq_data*   in_seq,
                        TSeqPos      uBeginIdx = 0,
                        TSeqPos      uLength   = 0);

    // Append in_seq2 to to end of in_seq1. Both in seqs must be
    // in the same alphabet or this method will throw a runtime_error.
    // The result of the append will be put into out_seq.
    // For packed sequences ncbi2na and ncbi4na, Append will shift and
    // append so as to remove any jaggedness at the append point.
    static TSeqPos Append(CSeq_data*         out_seq,
                          const CSeq_data&   in_seq1,
                          TSeqPos            uBeginIdx1,
                          TSeqPos            uLength1,
                          const CSeq_data&   in_seq2,
                          TSeqPos            uBeginIdx2,
                          TSeqPos            uLength2);

    // Create a biological complement of an na sequence.
    // Attempts to complement an aa sequence will throw
    // a runtime_error. Returns length of complemented sequence.

    // Complement the input sequence in place
    static TSeqPos Complement(CSeq_data*   in_seq,
                              TSeqPos      uBeginIdx = 0,
                              TSeqPos      uLength   = 0);

    // Complement the input sequence and put the result in
    // the output sequence
    static TSeqPos Complement(const CSeq_data&   in_seq,
                              CSeq_data*         out_seq,
                              TSeqPos            uBeginIdx = 0,
                              TSeqPos            uLength   = 0);

    // Create a biological sequence that is the reversse
    // of an input sequence. Attempts to reverse an aa
    // sequence will throw a runtime_error. Returns length of
    // reversed sequence.

    // Reverse a sequence in place
    static TSeqPos Reverse(CSeq_data*   in_seq,
                           TSeqPos      uBeginIdx = 0,
                           TSeqPos      uLength   = 0);

    // Reverse an input sequence and put result in output sequence.
    // Reverses packed bytes as necessary.
    static TSeqPos Reverse(const CSeq_data&   in_seq,
                           CSeq_data*         out_seq,
                           TSeqPos            uBeginIdx = 0,
                           TSeqPos            uLength   = 0);


    // Create the reverse complement of an input sequence. Attempts
    // to reverse-complement an aa sequence will throw a
    // runtime_error.

    // Reverse complement a sequence in place
    static TSeqPos ReverseComplement(CSeq_data*   in_seq,
                                     TSeqPos      uBeginIdx = 0,
                                     TSeqPos      uLength   = 0);

    // Reverse complmenet a sequence and put result in output sequence
    static TSeqPos ReverseComplement(const CSeq_data&   in_seq,
                                     CSeq_data*         out_seq,
                                     TSeqPos            uBeginIdx = 0,
                                     TSeqPos            uLength   = 0);
                                          
    // Given an Ncbistdaa input code index, returns the 3 letter Iupacaa3 code
    // Implementation is efficient
    static const string& GetIupacaa3(TIndex ncbistdaa);
    
    // Given a code type expressible as enum CSeq_data::E_Choice, returns
    // true if the code type is available. See Seq_data_.hpp for definition 
    // of enum CSeq_data::E_Choice.
    static bool IsCodeAvailable(CSeq_data::E_Choice code_type);
    
    // Same as above, but for code types expressible as enum ESeq_code_type.
    // See Seq_code_type_.hpp for definition of enum ESeq_code_type.
    static bool IsCodeAvailable(ESeq_code_type code_type);
    
    // Gets the first and last indices of a code type (e.g., for iupacna,
    // the first index is 65 and the last index is 89). Throws CBadType
    // if code_type is not available
    static TPair GetCodeIndexFromTo(CSeq_data::E_Choice code_type);
    
    // Same as above but takes code type expressed as an ESeq_code_type
    static TPair GetCodeIndexFromTo(ESeq_code_type code_type);
        
    // Given an index for any code type expressible as a
    // CSeq_data::E_Choice, returns the corresponding symbol (code)
    // (e.g., for iupacna, idx 65 causes "A" to be returned)
    // if the code type is available (e.g., ncbi8aa is not currently
    // available). Use IsCodeAvailable() to find out which code types
    // are available. Throws CBadType if code_type not available. Throws
    // CBadIndex if idx out of range for code_type.                    
    static const string& GetCode(CSeq_data::E_Choice code_type, 
                                 TIndex              idx); 

    // Similar to above, but works for all code types expressible as
    // a ESeq_code_type (i.e., iupacaa3 is expressible as an
    // ESeq_code_type but not as a CSeq_data::E_Choice)
    static const string& GetCode(ESeq_code_type code_type, 
                                 TIndex         idx); 
                        
    // Given an index for any code type expressible as a
    // CSeq_data::E_Choice, returns the corresponding name
    // (e.g., for iupacna, idx 65 causes "Adenine" to be returned)
    // if the code type is available (e.g., ncbi8aa is not currently
    // available). Use IsCodeAvailable() to find out which code types
    // are available. Throws CBadType if code_type not available. Throws
    // CBadIndex if idx out of range for code_type.                  
    static const string& GetName(CSeq_data::E_Choice code_type, 
                                 TIndex              idx); 

    // Similar to above, but works for all code types expressible as
    // a ESeq_code_type (i.e., iupacaa3 is expressible as an
    // ESeq_code_type but not as a CSeq_data::E_Choice)
    static const string& GetName(ESeq_code_type code_type, 
                                 TIndex         idx); 

    // Given a code (symbol) for any code type expressible as a
    // CSeq_data::E_Choice, returns the corresponding index
    // (e.g., for iupacna, symbol "A" causes 65 to be returned)
    // if the code type is available (e.g., ncbi8aa is not currently
    // available). Use IsCodeAvailable() to find out which code types
    // are available.Throws CBadType if code_type not available. Throws
    // CBadSymbol if code is not a valid symbol for code_type.                     
    static TIndex GetIndex(CSeq_data::E_Choice code_type, const string& code);
    
    // Similar to above, but works for all code types expressible as
    // a ESeq_code_type (i.e., iupacaa3 is expressible as an
    // ESeq_code_type but not as a CSeq_data::E_Choice)
    static TIndex GetIndex(ESeq_code_type code_type, const string& code);
    
    // Get the index that is the complement of the index for the
    // input CSeq_data::E_Choice code type (e.g., for iupacna, the
    // complement of index 66 is 86). Throws CBadType if complements for 
    // code_type not available. Throws CBadIndex if idx out of range for 
    // code_type    
    static TIndex GetIndexComplement(CSeq_data::E_Choice code_type,
                                     TIndex              idx);

    // Same as above, but for code type expressible as ESeq_code_type.                                  
    static TIndex GetIndexComplement(ESeq_code_type code_type,
                                     TIndex         idx);
    
    // Takes an index for a from_type code and returns the index for to_type
    // (e.g., GetMapToIndex(CSeq_data::e_Iupacna, CSeq_data::e_Ncbi2na,
    // 68) returns 2). Returns 255 if no map value exists for a legal
    // index (e.g., GetMapToIndex(CSeq_data::e_Iupacna, CSeq_data::e_Ncbi2na,
    // 69) returns 255). Throws CBadType if map for from_type to to_type not
    // available. Throws CBadIndex if from_idx out of range for from_type.
    static TIndex GetMapToIndex(CSeq_data::E_Choice from_type,
                             CSeq_data::E_Choice    to_type,
                             TIndex                 from_idx);
    
    // Same as above, but uses ESeq_code_type.                        
    static TIndex GetMapToIndex(ESeq_code_type from_type,
                                ESeq_code_type to_type,
                                TIndex         from_idx);

private:
    
    // we maintain a singleton internally
    // these variables and functions control access to the singleton

    static auto_ptr<CSeqportUtil_implementation> sm_Implementation;

    static void                         x_InitImplementation(void);
    static CSeqportUtil_implementation& x_GetImplementation (void);
};


inline
CSeqportUtil_implementation& CSeqportUtil::x_GetImplementation(void)
{
    if ( !sm_Implementation.get() ) {
        x_InitImplementation();
    }
    return *sm_Implementation;
}


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___SEQPORT_UTIL__HPP */

 /*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.8  2002/12/26 12:42:59  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.7  2002/09/13 20:43:01  ucko
 * +#include <memory> (for auto_ptr<>)
 *
 * Revision 1.6  2002/09/13 18:35:33  dicuccio
 * Fixed problem with static object initialization and type information.
 * First pass at reformatting code.
 *
 * Revision 1.5  2002/05/14 15:12:06  clausen
 * Added IsCodeAvailable, GetCodeIndexFromTo, GetName, GetIndexComplement, GetMapToIndex
 *
 * Revision 1.4  2002/05/03 21:28:03  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.3  2002/01/10 19:20:45  clausen
 * Added GetIupacaa3, GetCode, and GetIndex
 *
 * Revision 1.2  2001/09/07 14:16:49  ucko
 * Cleaned up external interface.
 *
 * Revision 1.1  2001/08/24 00:34:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
