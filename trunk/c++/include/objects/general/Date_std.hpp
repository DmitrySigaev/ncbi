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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   Useful member functions for standard dates: comparison and formatting
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'general.asn'.
 */

#ifndef OBJECTS_GENERAL_DATE_STD_HPP
#define OBJECTS_GENERAL_DATE_STD_HPP


// generated includes
#include <objects/general/Date_std_.hpp>

#include <objects/general/Date.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CDate_std : public CDate_std_Base
{
    typedef CDate_std_Base Tparent;
public:
    // constructors
    CDate_std(void);
    CDate_std(const CTime& time,
              CDate::EPrecision prec = CDate::ePrecision_second);
    CDate_std(time_t time, CDate::EPrecision prec = CDate::ePrecision_second);
    
    // destructor
    ~CDate_std(void);

    // conversion to/from CTime
    void  Assign (const CTime& time,
                  CDate::EPrecision prec = CDate::ePrecision_second);
    CTime AsCTime(CTime::ETimeZone tz = CTime::eLocal) const;

    CDate::ECompare Compare(const CDate_std& date) const;

    // See explanation of format in Date.hpp
    void GetDate(string* label, const string& format) const;

private:
    // Prohibit copy constructor and assignment operator
    CDate_std(const CDate_std& value);
    CDate_std& operator=(const CDate_std& value);

};



/////////////////// CDate_std inline methods

// constructor
inline
CDate_std::CDate_std(void)
{
}

inline
CDate_std::CDate_std(const CTime& time, CDate::EPrecision prec)
{
    Assign(time, prec);
}

inline
CDate_std::CDate_std(time_t time, CDate::EPrecision prec)
{
    Assign(time, prec);
}


/////////////////// end of CDate_std inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2002/12/06 20:03:15  ucko
 * Support conversion to/from CTime and from time_t
 *
 * Revision 1.2  2002/10/04 14:45:08  ucko
 * Add a generic date formatter with flexible support for missing fields.
 *
 * Revision 1.1  2001/11/16 20:54:50  ucko
 * Added comparison for dates.
 *
 * ===========================================================================
 */

#endif // OBJECTS_GENERAL_DATE_STD_HPP
/* Original file checksum: lines: 90, chars: 2388, CRC32: 3888bc05 */
