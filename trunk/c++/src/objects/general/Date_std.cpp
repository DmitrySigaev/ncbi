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
 *
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/general_exception.hpp>

#include <vector>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CDate_std::~CDate_std(void)
{
}


void CDate_std::SetToTime(const CTime& time, CDate::EPrecision prec)
{
    switch (prec) {
    case CDate::ePrecision_second:
        SetSecond(time.Second());
        SetMinute(time.Minute());
        SetHour  (time.Hour());
        // fall through
    case CDate::ePrecision_day:
        SetDay   (time.Day());
        SetMonth (time.Month());
        SetYear  (time.Year());
        break;
    default:
        break;
    }
}


CTime CDate_std::AsCTime(CTime::ETimeZone tz) const
{
    return CTime(GetYear(), GetMonth(), GetDay(),
                 GetHour(), GetMinute(), GetSecond(), 0, tz);
}


CDate::ECompare CDate_std::Compare(const CDate_std& date) const
{
    if (GetYear() < date.GetYear()) {
        return CDate::eCompare_before;
    } else if (GetYear() > date.GetYear()) {
        return CDate::eCompare_after;
    }

    if ((IsSetSeason()  ||  date.IsSetSeason())
        && ( !IsSetSeason()  ||  !date.IsSetSeason()
            || GetSeason()  !=  date.GetSeason())) {
        return CDate::eCompare_unknown;
    }

    if (IsSetMonth()  ||  date.IsSetMonth()) {
        if ( !IsSetMonth()  ||  !date.IsSetMonth()) {
            return CDate::eCompare_unknown;
        } else if (GetMonth() < date.GetMonth()) {
            return CDate::eCompare_before;
        } else if (GetMonth() > date.GetMonth()) {
            return CDate::eCompare_after;
        }
    }

    if (IsSetDay()  ||  date.IsSetDay()) {
        if ( !IsSetDay()  ||  !date.IsSetDay()) {
            return CDate::eCompare_unknown;
        } else if (GetDay() < date.GetDay()) {
            return CDate::eCompare_before;
        } else if (GetDay() > date.GetDay()) {
            return CDate::eCompare_after;
        }
    }

    if (IsSetHour()  ||  date.IsSetHour()) {
        if ( !IsSetHour()  ||  !date.IsSetHour()) {
            return CDate::eCompare_unknown;
        } else if (GetHour() < date.GetHour()) {
            return CDate::eCompare_before;
        } else if (GetHour() > date.GetHour()) {
            return CDate::eCompare_after;
        }
    }

    if (IsSetMinute()  ||  date.IsSetMinute()) {
        if ( !IsSetMinute()  ||  !date.IsSetMinute()) {
            return CDate::eCompare_unknown;
        } else if (GetMinute() < date.GetMinute()) {
            return CDate::eCompare_before;
        } else if (GetMinute() > date.GetMinute()) {
            return CDate::eCompare_after;
        }
    }

    if (IsSetSecond()  ||  date.IsSetSecond()) {
        if ( !IsSetSecond()  ||  !date.IsSetSecond()) {
            return CDate::eCompare_unknown;
        } else if (GetSecond() < date.GetSecond()) {
            return CDate::eCompare_before;
        } else if (GetSecond() > date.GetSecond()) {
            return CDate::eCompare_after;
        }
    }

    return CDate::eCompare_same;
}


void CDate_std::GetDate(string* label, const string& format) const
{
    static const char* const kMonths[] = {
        "0", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    if (!label) {
        return;
    }
    unsigned int                        depth = 0;
    vector<pair<SIZE_TYPE, SIZE_TYPE> > starts;
    starts.push_back(make_pair(label->size(), (SIZE_TYPE)0));
    ITERATE (string, it, format) {
        if (*it != '%') {
            *label += *it;
            continue;
        }
        if (++it == format.end()) {
            NCBI_THROW2(CGeneralParseException, eFormat,
                        "CDate_std::GetDate(): incomplete % expression",
                        it - format.begin());
        }
        // Check for things that can only immediately follow %
        if (*it == '%') {
            *label += '%';
            continue;
        }
        else if (*it == '{') {
            depth++;
            starts.push_back(make_pair(label->size(),
                                       SIZE_TYPE(it - format.begin())));
            continue;
        } else if (*it == '}') {
            if (depth == 0) {
                NCBI_THROW2(CGeneralParseException, eFormat,
                            "CDate_std::GetDate(): unbalanced %}",
                            it - format.begin());
            }
            depth--;
            starts.pop_back();
            continue;
        } else if (*it == '|') {
            // We survived, so just look for the appropriate %}.
            if (depth == 0) {
                return; // Can ignore rest of format
            }
            unsigned int depth2 = 0;
            for (;;) {
                while (++it != format.end()  &&  *it != '%')
                    ;
                if (it == format.end()  ||  ++it == format.end()) {
                    NCBI_THROW2(CGeneralParseException, eFormat,
                                "CDate_std::GetDate(): unbalanced %{",
                                starts.back().second);
                }
                if (*it == '}') {
                    if (depth2 == 0) {
                        depth--;
                        starts.pop_back();
                        break;
                    } else {
                        depth2--;
                    }
                } else if (*it == '{') {
                    depth2++;
                }
            }
            continue;
        }

        unsigned int length = 0;
        int          value  = -1;
        while (isdigit((unsigned char)(*it))) {
            length = length * 10 + *it - '0';
            if (++it == format.end()) {
                NCBI_THROW2(CGeneralParseException, eFormat,
                            "CDate_std::GetDate(): incomplete % expression",
                            it - format.begin());
            }
        }
        switch (*it) {
        case 'Y': value = GetYear(); break;
        case 'M':
        case 'N': value = IsSetMonth()  ? GetMonth()  : -1; break;
        case 'D': value = IsSetDay()    ? GetDay()    : -1; break;
        case 'S': value = IsSetSeason() ? 1           : -1; break;
        case 'h': value = IsSetHour()   ? GetHour()   : -1; break;
        case 'm': value = IsSetMinute() ? GetMinute() : -1; break;
        case 's': value = IsSetSecond() ? GetSecond() : -1; break;
        default:
            NCBI_THROW2(CGeneralParseException, eFormat,
                        "CDate_std::GetDate(): unrecognized format specifier",
                        it - format.begin());
        }

        if (value >= 0) {
            if (*it == 'N') { // special cases
                const char* name = kMonths[value];
                if (length > 0) {
                    label->append(name, length);
                } else {
                    *label += name;
                }
            } else if (*it == 'S') {
                if (length > 0) {
                    label->append(GetSeason(), 0, length);
                } else {
                    *label += GetSeason();
                }
            } else { // just a number
                if (length > 0) {
                    // We want exactly <length> digits.
                    CNcbiOstrstream oss;
                    oss << setfill('0') << setw(length) << value;
                    string s = CNcbiOstrstreamToString(oss);
                    label->append(s, s.size() > length ? s.size() - length : 0,
                                  length);
                } else {
                    *label += NStr::IntToString(value);
                }
            }
        } else {
            // missing...roll back label and look for alternatives, or
            // throw if at top level and none found
            label->erase(starts.back().first);
            char         request = *it;
            unsigned int depth2  = 0;
            for (;;) {
                while (++it != format.end()  &&  *it != '%')
                    ;
                if (it == format.end()  ||  ++it == format.end()) {
                    if (depth > 0  ||  depth2 > 0) {
                        NCBI_THROW2(CGeneralParseException, eFormat,
                                    "CDate_std::GetDate(): unbalanced %{",
                                    starts.back().second);
                    } else {
                        NCBI_THROW2(CGeneralParseException, eFormat,
                                   string("CDate_std::GetDate():"
                                          " missing required field %")
                                   + request, it - format.begin() - 1);
                    }
                }
                if (*it == '|'  &&  depth2 == 0) {
                    break;
                } else if (*it == '}') {
                    if (depth2 == 0) {
                        if (depth == 0) {
                            NCBI_THROW2(CGeneralParseException, eFormat,
                                        "CDate_std::GetDate(): unbalanced %}",
                                        it - format.begin());
                        }
                        depth--;
                        starts.pop_back();
                        break;
                    } else {
                        depth2--;
                    }
                } else if (*it == '{') {
                    depth2++;
                }
            }
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.8  2005/06/03 16:51:39  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 6.7  2004/05/19 17:21:39  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 6.6  2003/03/11 15:38:42  kuznets
 * iterate -> ITERATE
 *
 * Revision 6.5  2003/02/24 20:02:36  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 6.4  2002/12/09 17:30:11  ucko
 * Rename Assign to SetToTime to avoid shadowing CSerialObject; propagate tz
 *
 * Revision 6.3  2002/12/06 20:03:16  ucko
 * Support conversion to/from CTime and from time_t
 *
 * Revision 6.2  2002/10/04 14:45:09  ucko
 * Add a generic date formatter with flexible support for missing fields.
 *
 * Revision 6.1  2001/11/16 20:54:52  ucko
 * Added comparison for dates.
 *
 * ===========================================================================
 */

/* Original file checksum: lines: 61, chars: 1885, CRC32: 4ef42d28 */
