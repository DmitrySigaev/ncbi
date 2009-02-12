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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test CStreamUtils::Pushback() interface.
 *
 */

#include <ncbi_pch.hpp>
#include "pbacktest.hpp"
#include <corelib/ncbidbg.hpp>
#include <corelib/request_control.hpp>
#include <corelib/stream_utils.hpp>
#include <vector>
#include <stdlib.h>
#include <time.h>

#include <common/test_assert.h>  /* This header must go last */

#define _STR(a) #a
#define  STR(a) _STR(a)

#ifdef   max
#  undef max
#endif
#define  max(a, b) ((a) > (b) ? (a) : (b))


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 512*1024;


/* NOTE about MSVC compiler and its C++ std. library:
 *
 * The C++ standard is very confusing on the stream's ability to do
 * pushback() and unget(); it says nothing about the case whether
 * they are guaranteed, and if so, under what circumstances.
 * The only clear message is seen in "The C++ Programming Language",
 * 3rd ed. by B.Stroustrup, p.644, which reads "what is guaranteed
 * is that you can back up one character after a successful read."
 *
 * Most implementation obey this; but there are some that do not.
 *
 * A bug or not a bug, we had to put putback and unget tests to only
 * follow a first pushback operation, which (internally) changes the
 * stream buffer and always keeps "usual backup condition", described in
 * the standard, thus allowing at least one-char backup after a read.
 *
 * So, if you plan to do a portable backup, which can possibly follow
 * an unformatted read, do so by using CStreamUtils::Pushback() instead of
 * standard means, as the latter may be broken.
 */

extern int TEST_StreamPushback(iostream&    ios,
                               unsigned int seed_in,
                               bool         rewind)
{
    size_t i, j, k;
    vector<CT_CHAR_TYPE*> v;

    // Warnings, errors, etc
    GetDiagContext().SetLogRate_Limit(CDiagContext::eLogRate_Err,
                                      CRequestRateControl::kNoLimit);
    // Infos and traces only
    GetDiagContext().SetLogRate_Limit(CDiagContext::eLogRate_Trace,
                                      CRequestRateControl::kNoLimit);

    unsigned int seed = seed_in ? seed_in : (unsigned int) time(0);
    ERR_POST(Info << "Seed = " << seed);
    srand(seed);

    ERR_POST(Info << "Generating array of random data");
    char* orig = new char[kBufferSize + 1];
    char* data = new char[kBufferSize + 2];
    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            orig[j*1024 + i] = "0123456789"[rand() % 10];
        orig[j*1024 + 1024 - 1] = '\n';
    }
    orig[kBufferSize    ] = '\0';
    memset(data, '\xFF', kBufferSize + 1);
    data[kBufferSize + 1] = '\0';

    ERR_POST(Info << "Sending data down the stream");
    if (!(ios << orig)) {
        ERR_POST("Cannot send data");
        return 1;
    }
    if (rewind)
        ios.seekg(0);

    ERR_POST(Info << "Doing random reads and {push|step}backs of the reply");

    int d = 0;
    size_t nbusy = 0;
    size_t nread = 0;
    size_t npback = 0;
    char   pbackch = '\0';
#ifdef NCBI_COMPILER_MSVC
    bool   first_pushback_done = false;
#endif
    for (k = 0;  nread < kBufferSize;  k++) {
        size_t navail = kBufferSize + 1 - nread;
        if (navail >= 10)
            navail /= 10;
        i = rand() % navail + 1;

        if (nread + i > kBufferSize + 1)
            i = kBufferSize + 1 - nread;
        else if (!((j = npback ? rand() & 7 : 3) & ~1)  &&  (++d % 10) < 5) {
            i %= npback;
            i++;
        }
        _ASSERT(i > 0);

        bool   putback = false;
        string eol(1, (char)(rand() & 0x1F) + ' ');
        char*  end = (char*)(j & ~1 ? 0 : memchr(orig + nread, eol[0], i));
        ERR_POST(Info << "Reading " << i << " byte" << (i == 1 ? "" : "s") <<
                 (j & ~1 ? "" : string(" with ") +
                  (j ? "get('" +NStr::PrintableString(eol)+ "')" +
                   (end ? ", expecting " + NStr::UInt8ToString
                    ((Uint8)(end - (orig + nread))) : "")
                   : "read()")));

        // Force at least minimal blocking, since Readsome might not
        // block at all, and accepting 0-sized reads could lead to spinning.
        ios.peek();
        switch (j) {
        case 0:
            ios.read(data + nread, i);
            j = ios.gcount();
            break;
        case 1:
            ios.get(data + nread, i + 1/*EOL*/, eol[0]);
            j = ios.gcount();
            _ASSERT(!data[nread + j]);
            data[nread + j] = orig[nread + j]; // undo '\0' side effect mess
            if (!j) {
                _ASSERT(ios.rdstate() & NcbiFailbit);
                ios.clear();
                _ASSERT(ios.get() == eol[0]);
                j++;
            } else {
                if (!ios.good()) {
                    _ASSERT(ios.rdstate() == NcbiEofbit);
                    _ASSERT(nread + j == kBufferSize);
                    ios.clear();
                } else if (j < i)
                    _ASSERT(orig[nread + j] == eol[0]);
                putback = true;
            }
            break;
        default:
            j = CStreamUtils::Readsome(ios, data + nread, i);
            break;
        }

        if (!ios.good()  ||  !j) {
            if (ios.good())
                ERR_POST("Nothing received");
            else
                ERR_POST("Cannot receive data");
            return 2;
        }

        if (j < i)
            ERR_POST(Info << "Got " << j << " byte" << &"s"[j == 1]);
        else
            _ASSERT(j == i);
        if (npback <= j)
            npback  = 0;
        else
            npback -= j;

        if (pbackch  &&  pbackch != data[nread]) {
            ERR_POST("Mismatch: putback '"                         <<
                     NStr::PrintableString(string(1, pbackch))     <<
                     "' vs '"                                      <<
                     NStr::PrintableString(string(1, data[nread])) <<
                     "' read");
            return 2;
        } else
            pbackch = '\0';

        for (i = 0;  i < j;  i++) {
            if (orig[nread + i] != data[nread + i]) {
                ERR_POST("Mismatch: sent '"                                <<
                         NStr::PrintableString(string(1, orig[nread + i])) <<
                         "' vs '"                                          <<
                         NStr::PrintableString(string(1, data[nread + i])) <<
                         "' received @ " << i);
                return 2;
            }
        }
        nread += j;
 
        ERR_POST(Info << "Obtained so far " <<
                 nread << " out of " << kBufferSize << ", " <<
                 npback << " pending");
        bool update = false;

        if (rewind  &&  rand() % 7 == 0  &&  nread < kBufferSize) {
            if (rand() & 1) {
                ERR_POST(Info << "Testing pre-seekg(" << nread <<
                         ", " << STR(IOS_BASE) "::beg)");
                ios.seekg(nread, IOS_BASE::beg);
            } else {
                ERR_POST(Info << "Testing pre-seekg(" << nread << ')');
                ios.seekg(nread);
            }
            if (!ios.good()) {
                ERR_POST("Cannot position stream");
                return 2;
            }
        } else if (ios.good()  &&  rand() % 5 == 0  &&  j > 1  &&  !putback) {
#ifdef NCBI_COMPILER_MSVC
            if (!rewind  ||  first_pushback_done) {
#endif
                j--;
                npback++;
                putback = true;
                pbackch = data[--nread];
                string action;
                if (rand() & 1) {
                    action = "Putback";
                    ios.putback(pbackch);
                } else {
                    action = "Unget";
                    ios.unget();
                }
                ERR_POST(Info << action << "('"                    <<
                         NStr::PrintableString(string(1, pbackch)) << "')");
                if (!ios.good()) {
                    ERR_POST("Cannot put a byte back");
                    return 2;
                }
                update = true;
#ifdef NCBI_COMPILER_MSVC
            }
#endif
        }

        if (update) {
            ERR_POST(Info << "Obtained so far " <<
                     nread << " out of " << kBufferSize << ", " <<
                     npback << " pending");
            update = false;
        }

        if (!(rand() % 101)  &&  (++d % 10) < 5)
            i = rand() % nread + 1;
        else
            i = rand() % j     + 1;

        if ((i != nread  &&  i != j)  ||  --i) {
            int how = rand() & 0x0F;
            int pushback = how & 0x01;
            int longform = how & 0x02;
            int original = how & 0x04;
            int passthru = 0;
            size_t slack = 0;
            CT_CHAR_TYPE* p;
            nread -= i;
            if (!original) {
                slack = rand() & 0x7FF;  // slack space filled with garbage
                p = new CT_CHAR_TYPE[i + (slack << 1)];
                for (size_t ii = 0;  ii < slack;  ii++) {
                    p[            ii] = rand() % 26 + 'A';
                    p[slack + i + ii] = rand() % 26 + 'a';
                }
                memcpy(p + slack, data + nread, i);
                passthru = how & 0x08;
            } else
                p = data + nread;
            npback += i;
            ERR_POST(Info << (pushback ? "Pushing" : "Stepping") <<
                     " back " << i << " byte" << &"s"[i == 1]    <<
                     (!longform
                      ? (original
                         ? ""
                         : " [copy]")
                      : (original
                         ? " [original]"
                         : (passthru
                            ? " [passthru copy]"
                            : " [retained copy]"))));
            switch (how &= 3) {
            case 0:
                CStreamUtils::Stepback(ios, p + slack, i);
                break;
            case 1:
                CStreamUtils::Pushback(ios, p + slack, i);
                break;
            case 2:
                CStreamUtils::Stepback(ios, p + slack, i, passthru ? p : 0);
                break;
            case 3:
                CStreamUtils::Pushback(ios, p + slack, i, passthru ? p : 0);
                if (!original  &&  !passthru)
                    v.push_back(p);
                break;
            }
            pbackch = data[nread];
            putback = true;
            if (how ^ 3) {
                // clobber unused data
                if (!original  &&  (!longform  ||  !passthru)) {
                    for (size_t ii = slack;  ii < slack + i;  ii++)
                        p[ii] = "%*-+="[rand() % 5];
                    delete[] p;
                }
                for (size_t ii = max(nbusy, nread);  ii < nread + i;  ii++)
                    data[ii] = "!@#$&"[rand() % 5];
            } else if (original  &&  nbusy < nread + i)
                nbusy = nread + i;
#ifdef NCBI_COMPILER_MSVC
            first_pushback_done = true;
#endif
            ERR_POST(Info << "Obtained so far " <<
                     nread << " out of " << kBufferSize << ", " <<
                     npback << " pending");
        }

        if (rewind  &&  rand() % 9 == 0  &&  nread < kBufferSize) {
            if (putback  &&  pbackch) {
                data[nread++] = pbackch;
                pbackch = '\0';
                update = true;
                npback--;
            }
            if (rand() & 1) {
                ERR_POST(Info << "Testing post-seekg(" << nread << ')');
                ios.seekg(nread);
            } else {
                ERR_POST(Info << "Testing post-seekg(" << nread <<
                         ", " << STR(IOS_BASE) << "::beg)");
                ios.seekg(nread, IOS_BASE::beg);
            }
            if (!ios.good()) {
                ERR_POST("Cannot re-position stream");
                return 2;
            }
        }

        if (update) {
            ERR_POST(Info << "Obtained so far " <<
                     nread << " out of " << kBufferSize << ", " <<
                     npback << " pending");
        }
    }
    
    ERR_POST(Info << nread << " byte" << &"s"[nread==1]           <<
             " obtained in " << k << " iteration" << &"s"[k == 1] <<
             (ios.eof() ? " (EOF)" : ""));
    data[nread] = '\0';
    _ASSERT(!npback);

    for (i = 0;  i < kBufferSize;  i++) {
        if (!data[i]) {
            ERR_POST("Zero byte encountered @ " << i);
            return 1;
        }
        if (orig[i] != data[i]) {
            ERR_POST("Mismatch: in '"                          <<
                     NStr::PrintableString(string(1, orig[i])) <<
                     "' vs '"                                  <<
                     NStr::PrintableString(string(1, data[i])) <<
                     "' out @ " << i);
            return 1;
        }
    }
    if (nread > kBufferSize) {
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << nread);
        return 1;
    }

    CT_POS_TYPE posp = ios.tellp();
    CT_OFF_TYPE offp = (CT_OFF_TYPE)(posp
                                     - (CT_POS_TYPE)((CT_OFF_TYPE)(0)));
    _ASSERT(Int8(offp) == NcbiStreamposToInt8(posp));
    _ASSERT(NcbiInt8ToStreampos(Int8(offp)) == posp);

    CT_POS_TYPE posg = ios.tellg();
    CT_OFF_TYPE offg = (CT_OFF_TYPE)(posg
                                     - (CT_POS_TYPE)((CT_OFF_TYPE)(0)));
    _ASSERT(Int8(offg) == NcbiStreamposToInt8(posg));
    _ASSERT(NcbiInt8ToStreampos(Int8(offg)) == posg);

    if (posp != posg  ||  offp != offg) {
        ERR_POST("Off PUT("
                 << NStr::Int8ToString(Int8(offp)) << ") != "
                 "Off GET("
                 << NStr::Int8ToString(Int8(offg)) << ')');
        return 1;
    }

    ERR_POST(Info << "Test passed");

    delete[] orig;
    delete[] data;

    for (i = 0;  i < v.size();  i++)
        delete[] v[i];

    return 0/*okay*/;
}


END_NCBI_SCOPE
