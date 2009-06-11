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
 * Author:  Sergey Satskiy
 *
 */

#include <ncbi_pch.hpp>
#include <util/util_misc.hpp>

#if defined(NCBI_OS_UNIX)
#  include <termios.h>
#  include <unistd.h>
#  include <corelib/ncbistre.hpp>
#elif defined(NCBI_OS_MSWIN)
#  include <conio.h>
#else
#  error  "Unsuported platform"
#endif


BEGIN_NCBI_SCOPE


const char* CGetPasswordFromConsoleException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eGetTerminalAttrError: return "eGetTerminalAttrError";
    case eSetTerminalAttrError: return "eSetTerminalAttrError";
    case eGetLineError:         return "eGetLineError";
    case eKeyboardInterrupt:    return "eKeyboardInterrupt";
    default:                    return CException::GetErrCodeString();
    }
}


string g_GetPasswordFromConsole(const string& prompt)
{
    string password;

#if defined(NCBI_OS_UNIX)
    // UNIX implementation

    struct termios old_attributes;

    if (tcgetattr(0, &old_attributes) != 0) {
        NCBI_THROW
            (CGetPasswordFromConsoleException, eGetTerminalAttrError,
             "g_GetPasswordFromConsole(): cannot get terminal attributes");
    }


    struct termios new_attributes = old_attributes;
    new_attributes.c_lflag &= ~ECHO;

    if (tcsetattr(0, TCSADRAIN, &new_attributes) != 0) {
        NCBI_THROW
            (CGetPasswordFromConsoleException, eSetTerminalAttrError,
             "g_GetPasswordFromConsole(): cannot set terminal attributes");
    }

    if ( !prompt.empty() )
        NcbiCout << prompt << NcbiFlush;

    if ( !NcbiGetline(NcbiCin, password, '\n') ) {
        tcsetattr(0, TCSADRAIN, &old_attributes);
        NcbiCin.clear();
        NCBI_THROW
            (CGetPasswordFromConsoleException, eGetLineError,
             "g_GetPasswordFromConsole(): cannot get line from terminal");
    }

    // Restore terminal attributes
    tcsetattr(0, TCSADRAIN, &old_attributes);

#elif defined(NCBI_OS_MSWIN)
    // Windows implementation

    for (size_t index = 0;  index < prompt.size();  ++index) {
        _putch(prompt[index]);
    }

    for (;;) {
        char ch;
        ch = _getch();
        if (ch == '\r'  ||  ch == '\n')
            break;
        if (ch == '\003')
            NCBI_THROW(CGetPasswordFromConsoleException, eKeyboardInterrupt,
                       "g_GetPasswordFromConsole(): keyboard interrupt");
        if (ch == '\b') {
            if ( !password.empty() ) {
                password.resize(password.size() - 1);
            }
        }
        else
            password.append(1, ch);
    }

    _putch('\r');
    _putch('\n');
#endif

    return password;
}


END_NCBI_SCOPE

