#ifndef NCBIREG__HPP
#define NCBIREG__HPP

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Handle info in the NCBI configuration file(s):
 *      read and parse configuration file
 *      search, edit, etc. in the retrieved configuration info
 *      dump info back to config. file
 *
 */

#include <corelib/ncbistd.hpp>
#include <list>
#include <map>


BEGIN_NCBI_SCOPE


/***  EXAMPLE of a registry file:
# Registry file comment (begin of file)
..............
[section1]
n1.n2 = value11 value12
n-2.3 = "  this value has two spaces at its very beginning and at the end  "
name3 = this is a multi\
line value
name4 = this is a single line ended by back slash\\
name5 = all backslashes and \
new lines must be \\escaped\\...
name6 = invalid backslash \in the middle
..............
; This is a section comment...
[ section2.9-bis ]
; This is an entry comment...
name1 = value1
..............
***/


///  "Section" and "name" must contain only:  [a-z], [A-Z], [0-9], [_.-];
///  they are not case-sensitive;  their leading and trailing spaces get
///  truncated.
///  Comments and empty lines are skipped during the parsing.
///  The serialization escaping rules:
///   1) [backslash] + [backslash] is converted into a single [backslash]
///   2) [backslash] + [space(s)] + [EndOfLine] is converted to an [EndOfLine]
///   3) [backslash] + ["]  is converted into a ["]
///   4) unescaped '"' get ignored in the very beginning/end of value
///   *) all other combinations with [backslash] and ["] are invalid
///  Hint:
///   !) The memory-resided values always assume the [EndOfLine] == '\n', while
///      the '\n' converts into/from the platform-specific EOL when writing or
///      reading the config stream(file).

class CNcbiRegistry {
public:
    // Flags
    enum EFlags {
        eTransient   = 0x1,
        ePersistent  = 0x100,  /* non-transient */
        eOverride    = 0x2,
        eNoOverride  = 0x200,
        eTruncate    = 0x4,
        eNoTruncate  = 0x400
    };
    typedef int TFlags;

    // 'ctors
    CNcbiRegistry(void);
    ~CNcbiRegistry(void);

    // Valid flags := { eTransient }
    // If "eTransient" flag is set then store the newly retrieved parameters as
    // transient;  otherwise, store them as persistent.
    CNcbiRegistry(CNcbiIstream& is, TFlags flags = 0);  // also, see Read()

    // Return TRUE if the registry contains no entries
    bool Empty(void) const;

    // Return TRUE if the persistent part of the registry (i.e. persistent
    // value(s) and the all-registry comment) was modified since:
    //  a) the last successful Write(), or
    //  b) the registry creation and maybe immediate read after the creation.
    bool Modified(void) const;

    // Parse the stream "is" and merge its content to current entries.
    //
    // Valid flags := { eTransient, eNoOverride }
    // If there is a conflict between the old and the new(loaded) entry value:
    //   if "eNoOverride" flag is set than just ignore the new value;
    //   otherwise, replace the old value by the new one.
    // If "eTransient" flag is set then store the newly retrieved parameters as
    // transient;  otherwise, store them as persistent.
    void Read(CNcbiIstream& is, TFlags flags = 0);

    // Dump the registry content(non-transient entries only!) to "os"
    bool Write(CNcbiOstream& os) const;

    // Reset the whole registry content
    void Clear(void);

    // First, search for the transient parameter value, and if cannot
    // find in there then continue the search in the non-transient params.
    // Return empty string if the config. parameter not found.
    //
    // Valid flags := { ePersistent }
    // If "ePersistent" flag is set then dont search in transients at all.
    const string& Get(const string& section, const string& name,
                      TFlags flags = 0) const;

    // Set the configuration parameter value(unset if "value" is empty)
    // Return TRUE if the new "value" is successfully set(or unset)
    //
    // Valid flags := { ePersistent, eNoOverride, eTruncate }
    // If there was already an entry with the same <section,name> key:
    //   if "eNoOverride" flag is set then do not override old value
    //   and return FALSE;  else override the old value and return TRUE.
    // If "ePersistent" flag is set then store the entry as persistent;
    // else store it as transient.
    // If "eTruncate" flag is set then truncate the leading and trailing
    // spaces -- " \r\t\v" (NOTE:  '\n' is not considered a space!).
    bool Set(const string& section, const string& name, const string& value,
             TFlags flags = 0, const string& comment = kEmptyStr);

    // Set comment "comment" for the registry entry "section:name".
    // If "name" is empty string, then set as the "section" comment.
    // If "section" is empty string, then set as the registry comment.
    // Return FALSE if "section" and/or "name" do not exist in registry.
    bool SetComment(const string& comment,  // kEmptyStr to delete the comment
                    const string& section = kEmptyStr,
                    const string& name    = kEmptyStr);

    // Get comment of the registry entry "section:name".
    // If "name" is empty string, then get "section"'s comment.
    // If "section" is empty string, then get the registry's comment.
    // Return empty string if the "section:name" entry not found.
    const string& GetComment(const string& section = kEmptyStr,
                             const string& name    = kEmptyStr);

    // These two functions:  first erase the passed list, then fill it out by:
    //    name of sections that comprise the whole registry
    void EnumerateSections(list<string>* sections) const;
    //    name of entries(all) that belong to the specified "section"
    void EnumerateEntries(const string& section, list<string>* entries) const;

private:
    struct TRegEntry {
        string persistent;  // non-transient
        string transient;   // transient(thus does not get dumped by Write())
        string comment;     // entry's comment
    };
    typedef map<string, TRegEntry,   PNocase>  TRegSection;
    typedef map<string, TRegSection, PNocase>  TRegistry;
    TRegistry    m_Registry;

    string       m_Comment;   // all-registry comment

    mutable bool m_Modified;  // persistent value(s) changed
    mutable bool m_Written;   // method Write() was called at least once


    // Valid flags := { ePersistent, eTruncate }
    void x_SetValue(TRegEntry& entry, const string& value,
                    TFlags flags, const string& comment);

    // Check if the registry contains only transient entries and comments
    bool x_IsAllTransient(void) const;

    // prohibit default initialization and assignment
    CNcbiRegistry(const CNcbiRegistry&);
    CNcbiRegistry& operator= (const CNcbiRegistry&);
};  // CNcbiRegistry


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.15  2001/09/11 00:46:56  vakatov
 * Fixes to R1.14:
 *   Renamed HasChanged() to Modified(), refined and extended its functionality
 *   Made Write() be "const" again
 *
 * Revision 1.14  2001/09/10 16:35:02  ivanov
 * Added method HasChanged()
 *
 * Revision 1.13  2001/06/22 21:50:20  ivanov
 * Added (with Denis Vakatov) ability for read/write the registry file
 * with comments. Also added functions GetComment() and SetComment().
 *
 * Revision 1.12  2001/05/17 14:54:01  lavr
 * Typos corrected
 *
 * Revision 1.11  2001/04/09 17:39:20  grichenk
 * CNcbiRegistry::Get() return type reverted to "const string&"
 *
 * Revision 1.10  2001/04/06 15:46:29  grichenk
 * Added thread-safety to CNcbiRegistry:: methods
 *
 * Revision 1.9  1999/09/02 21:53:23  vakatov
 * Allow '-' and '.' in the section/entry name
 *
 * Revision 1.8  1999/07/07 14:17:05  vakatov
 * CNcbiRegistry::  made the section and entry names be case-insensitive
 *
 * Revision 1.7  1999/07/06 15:26:31  vakatov
 * CNcbiRegistry::
 *   - allow multi-line values
 *   - allow values starting and ending with space symbols
 *   - introduced EFlags/TFlags for optional parameters in the class
 *     member functions -- rather than former numerous boolean parameters
 *
 * Revision 1.6  1998/12/28 17:56:28  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.5  1998/12/10 22:59:46  vakatov
 * CNcbiRegistry:: API is ready(and by-and-large tested)
 * ===========================================================================
 */

#endif  /* NCBIREG__HPP */

