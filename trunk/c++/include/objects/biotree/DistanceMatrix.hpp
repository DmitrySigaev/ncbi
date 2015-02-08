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
 */

/// @file DistanceMatrix.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'biotree.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: DistanceMatrix_.hpp


#ifndef OBJECTS_BIOTREE_DISTANCEMATRIX_HPP
#define OBJECTS_BIOTREE_DISTANCEMATRIX_HPP


// generated includes
#include <objects/biotree/DistanceMatrix_.hpp>

#include <util/math/matrix.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_BIOTREE_EXPORT CDistanceMatrix : public CDistanceMatrix_Base
{
    typedef CDistanceMatrix_Base Tparent;
public:
    // Type of Phylip-style format
    enum EFormat {
        eSquare,  // full square matrix
        eUpper,   // upper-triangular
        eLower,   // lower-triangular
        eGuess    // guess which of the above it is
    };

    // constructor
    CDistanceMatrix(void);
    // destructor
    ~CDistanceMatrix(void);
    
    // Create an ordinary matrix representation
    void AsMatrix(CNcbiMatrix<double>& mat) const;
    // Set to values of an ordinary matrix.
    // Must be square and symmetric, with zeros on the main diagonal.
    void FromMatrix(const CNcbiMatrix<double>& mat);
    // Read from a stream in the format used by Phylip
    void Read(istream &istr, EFormat format = eGuess);
private:
    // Prohibit copy constructor and assignment operator
    CDistanceMatrix(const CDistanceMatrix& value);
    CDistanceMatrix& operator=(const CDistanceMatrix& value);

};

/////////////////// CDistanceMatrix inline methods

// constructor
inline
CDistanceMatrix::CDistanceMatrix(void)
{
}


/////////////////// end of CDistanceMatrix inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_BIOTREE_DISTANCEMATRIX_HPP
/* Original file checksum: lines: 94, chars: 2678, CRC32: a1f7b9a8 */