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
 * Authors:  Anton Butanayev, Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Test for CTime - the standard Date/Time class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//============================================================================
//
// TestMisc
//
//============================================================================

static void s_TestMisc(void)
{
    cout << "---------------------------" << endl;
    cout << "Test Misc" << endl;
    cout << "---------------------------" << endl << endl;

    // Print current time
    {{
        CTime t(CTime::eCurrent);
        cout << "[" << t.AsString() << "]" << endl;
        cout << "[" << (t++).AsString() << "]" << endl;
        cout << "[" << (t++).AsString() << "]" << endl;
        cout << "[" << (t++).AsString() << "]" << endl;
        cout << "[" << (++t).AsString() << "]" << endl;
        cout << "[" << (++t).AsString() << "]" << endl;
    }}

    // Month and Day name<->num conversion
    {{
        assert(CTime::MonthNameToNum("Jan")              == CTime::eJanuary); 
        assert(CTime::MonthNameToNum("January")          == 1); 
        assert(CTime::MonthNameToNum("Dec")              == CTime::eDecember); 
        assert(CTime::MonthNameToNum("December")         == 12); 
        assert(CTime::MonthNumToName(CTime::eJanuary)    == "January"); 
        assert(CTime::MonthNumToName(1, CTime::eAbbr)    == "Jan"); 
        assert(CTime::MonthNumToName(CTime::eDecember,
                                     CTime::eFull)       == "December"); 
        assert(CTime::MonthNumToName(12,CTime::eAbbr)    == "Dec"); 

        assert(CTime::DayOfWeekNameToNum("Sun")          == CTime::eSunday); 
        assert(CTime::DayOfWeekNameToNum("Sunday")       == 0); 
        assert(CTime::DayOfWeekNameToNum("Sat")          == CTime::eSaturday);
        assert(CTime::DayOfWeekNameToNum("Saturday")     == 6); 
        assert(CTime::DayOfWeekNumToName(CTime::eSunday) == "Sunday"); 
        assert(CTime::DayOfWeekNumToName(0,CTime::eAbbr) == "Sun"); 
        assert(CTime::DayOfWeekNumToName(CTime::eSaturday,
                                       CTime::eFull)     == "Saturday"); 
        assert(CTime::DayOfWeekNumToName(6,CTime::eAbbr) == "Sat"); 

        cout << "Throw exception below:" << endl;
        try {
            CTime::MonthNameToNum("Month"); 
        } catch (CTimeException& e) {
            NCBI_REPORT_EXCEPTION("", e);
        }
    }}

    // String <-> CTime conversion
    {{
        {{
            CTime t;
            cout << "[" << t.AsString() << "]" << endl;
            assert(t.AsString() == "");
        }}
        {{
            CTime t(2000, 365 / 2);
            CTime::SetFormat("M/D/Y h:m:s");
            cout << "[" << t.AsString() << "]" << endl;
            assert(t.AsString() == "06/30/2000 00:00:00");
        }}
        cout << endl;
        {{
            CTime::SetFormat("M/D/Y");

            cout << "Year 2000 problem:" << endl << endl;
            CTime t(1999, 12, 30); 
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "12/31/1999");
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "01/01/2000");
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "01/02/2000");
            t="02/27/2000";
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "02/28/2000");
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "02/29/2000");
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "03/01/2000");
            t++; cout << "[" << t.AsString() << "] " << endl;
            assert(t.AsString() == "03/02/2000");
        }}
        cout << endl;
        {{
            CTime::SetFormat("M/D/Y h:m:s");
            cout << "String assignment:" << endl;
            try {
                CTime t("02/15/2000 01:12:33");
                cout << "[" << t.AsString() << "]" << endl;
                assert(t.AsString() == "02/15/2000 01:12:33");
                t = "3/16/2001 02:13:34";
                cout << "[" << t.AsString() << "]" << endl;
                assert(t.AsString() == "03/16/2001 02:13:34");
            } catch (CException& e) {
                NCBI_REPORT_EXCEPTION("",e);
            }
        }}
        cout << endl;
    }}

    // Addition
    {{
        CTime::SetFormat("M/D/Y h:m:s.S");
        {{
            cout << "Adding Nanoseconds:" << endl;
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 59, 999999995);
                 tmp <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
                 t = tmp, tmp.AddNanoSecond(2)) {
                 cout << "[" << tmp.AsString() << "] " << endl;
            }
            assert(t.AsString() == "01/01/2000 00:00:00.000000003");
        }}
        cout << endl;
        {{
            cout << "Current time with NS (10 cicles)" << endl;
            CTime t;
            for (int i = 0; i < 10; i++) {
                 t.SetCurrent();
                 cout << "[" << t.AsString() << "] " << endl;
            }
        }}
        cout << endl;

        CTime::SetFormat("M/D/Y h:m:s");
        {{
            cout << "Adding seconds:" << endl;
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 5);
                 tmp <= CTime(2000, 1, 1, 0, 1, 20);
                 t = tmp, tmp.AddSecond(11)) {
                 cout << "[" << tmp.AsString() << "] " << endl;
            }
            assert(t.AsString() == "01/01/2000 00:01:17");
        }}
        cout << endl;
        {{
            cout << "Adding minutes:" << endl;
            for (CTime t(1999, 12, 31, 23, 45);
                 t <= CTime(2000, 1, 1, 0, 15);
                 t.AddMinute(11)) {
                 cout << "[" << t.AsString() << "] " << endl;
            }
        }}
        cout << endl;
        {{
            cout << "Adding hours:" << endl;
            for (CTime t(1999, 12, 31);
                 t <= CTime(2000, 1, 1, 15);
                 t.AddHour(11)) {
                 cout << "[" << t.AsString() << "] " << endl;
            }
        }}
        cout << endl;
        {{
            cout << "Adding months:" << endl;
            for (CTime t(1998, 12, 29);
                 t <= CTime(1999, 4, 1);
                 t.AddMonth()) {
                 cout << "[" << t.AsString() << "] " << endl;
            }
        }}
        cout << endl;
        {{
            cout << "Adding time span:" << endl;
            CTime t0(1999, 12, 31, 23, 59, 5);
            CTimeSpan ts(1, 2, 3, 4, 555555555);

            for (int i=0; i<10; i++) {
                 t0.AddTimeSpan(ts);
                 cout << "[" << t0.AsString() << "] " << endl;
            }
            assert(t0.AsString() == "01/11/2000 20:29:50");

            CTime t1;
            t1 = t0 + ts;
            cout << "[" << t1.AsString() << "] " << endl;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = ts + t0;
            cout << "[" << t1.AsString() << "] " << endl;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0; t1 += ts;
            cout << "[" << t1.AsString() << "] " << endl;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0 - ts;
            cout << "[" << t1.AsString() << "] " << endl;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            t1 = t0; t1 -= ts;
            cout << "[" << t1.AsString() << "] " << endl;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            ts = t0 - t1;
            cout << "[" << ts.AsString() << "] " << endl;
            assert(ts.AsString() == "93784.555555555");
            ts = t1 - t0;
            cout << "[" << ts.AsString() << "] " << endl;
            assert(ts.AsString() == "-93784.555555555");
        }}
        cout << endl;
    }}

    // Difference
    {{
        CTime t1(2000, 10, 1, 12, 3, 45,1);
        CTime t2(2000, 10, 2, 14, 55, 1,2);
        CTimeSpan ts(1,2,51,16,1);

        cout << "[" << t1.AsString() << " - " << t2.AsString() << "]" << endl;
        printf("DiffDay        = %.2f\n", t2.DiffDay   (t1));
        assert((t2.DiffDay(t1)-1.12) < 0.01);
        printf("DiffHour       = %.2f\n", t2.DiffHour  (t1));
        assert((t2.DiffHour(t1)-26.85) < 0.01);
        printf("DiffMinute     = %.2f\n", t2.DiffMinute(t1));
        assert((t2.DiffMinute(t1)-1611.27) < 0.01);
        printf("DiffSecond     = %d\n",   t2.DiffSecond(t1));
        assert(t2.DiffSecond(t1) == 96676);
        printf("DiffNanoSecond = %.0f\n", t2.DiffNanoSecond(t1));
        printf("DiffTimeSpan   = %s\n", ts.AsString().c_str());
        assert(t2.DiffTimeSpan(t1) == ts);
        assert(t1.DiffTimeSpan(t2) == -ts);
    }}

    // Datebase formats conversion
    {{
        CTime t1(2000, 1, 1, 1, 1, 1, 10000000);
        CTime::SetFormat("M/D/Y h:m:s.S");

        cout << endl << "DB time formats [" << t1.AsString() << "]" << endl;

        TDBTimeU dbu = t1.GetTimeDBU();
        TDBTimeI dbi = t1.GetTimeDBI();

        cout << "DBU days             = " << dbu.days << endl;
        cout << "DBU time (min)       = " << dbu.time << endl;
        cout << "DBI days             = " << dbi.days << endl;
        cout << "DBI time (1/300 sec) = " << dbi.time << endl;
        cout << endl;
        CTime t2;
        t2.SetTimeDBU(dbu);
        cout << "Time from DBU        = " << t2.AsString() << endl;
        t2.SetTimeDBI(dbi);
        cout << "Time from DBI        = " << t2.AsString() << endl;

        CTime::SetFormat("M/D/Y h:m:s");
        dbi.days = 37093;
        dbi.time = 12301381;
        t2.SetTimeDBI(dbi);
        cout << "Time from DBI        = " << t2.AsString() << endl;
        assert(t2.AsString() == "07/23/2001 11:23:24");
    }}

    // Set* functions
    {{
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t(2000, 1, 31);

        t.SetMonth(2);
        assert(t.AsString() == "02/29/2000 00:00:00");
        t.SetYear(2001);
        assert(t.AsString() == "02/28/2001 00:00:00");
        t.SetMonth(4);
        assert(t.AsString() == "04/28/2001 00:00:00");
        t.SetDay(31);
        assert(t.AsString() == "04/30/2001 00:00:00");
        t.SetHour(6);
        assert(t.AsString() == "04/30/2001 06:00:00");
        t.SetMinute(37);
        assert(t.AsString() == "04/30/2001 06:37:00");
        t.SetSecond(59);
        assert(t.AsString() == "04/30/2001 06:37:59");
    }}

    // Day of week
    {{   
        CTime t(1900, 1, 1);
        int i;
        for (i = 1; t <= CTime(2030, 12, 31); t++,i++) {
            assert(t.DayOfWeek() == (i%7));
        }
    }}

    // Number of days in the month
    {{
        CTime t(2000, 1, 31);
        assert(t.DaysInMonth() == 31);
        t.SetMonth(2);
        assert(t.DaysInMonth() == 29);
        t.SetYear(2001);
        assert(t.DaysInMonth() == 28);
        t.SetMonth(4);
        assert(t.DaysInMonth() == 30);
    }}

    // Week number in the year/month
    {{
        CTime t(1970, 1, 1);
        int i;
        char buf[3];

        time_t gt = t.GetTimeT();

        for (i = 1; t <= CTime(2030, 12, 31); i++, t++, gt += 24*3600) {
            struct tm *today = gmtime(&gt);
            assert(today != 0);
            int week_num_rtl, week_num, month_week_num;

            // Sunday-based weeks
            strftime(buf, sizeof(buf), "%U", today);
            week_num_rtl   = NStr::StringToInt(buf) + 1;
            week_num       = t.YearWeekNumber(/*CTime::eSunday*/);
            assert(week_num_rtl == week_num);
            month_week_num = t.MonthWeekNumber(/*CTime::eSunday*/);
            assert(month_week_num >= 1  &&  month_week_num <= 6);

            // Monday-based weeks
            strftime(buf, sizeof(buf), "%W", today);
            week_num_rtl   = NStr::StringToInt(buf) + 1;
            week_num       = t.YearWeekNumber(CTime::eMonday);
            assert(week_num_rtl == week_num);
            month_week_num = t.MonthWeekNumber(CTime::eMonday);
            assert(month_week_num >= 1  &&  month_week_num <= 6);
        }
    }}

    cout << endl;
}


//============================================================================
//
// TestFormats
//
//============================================================================

static void s_TestFormats(void)
{
    static const char* s_Fmt[] = {
        "w, D b Y h:m:s Z",
        "M/D/Y h:m:s",
        "M/D/Y h:m:s.S",
        "M/D/y h:m:s",
        "M/DY  h:m:s",
        "M/Dy  h:m:s",
        "M/D/Y hm:s",
        "M/D/Y h:ms",
        "M/D/Y hms",
        "MD/y  h:m:s",
        "MD/Y  h:m:s",
        "MYD   m:h:s",
        "M/D/Y smh",
        "YMD   h:sm",
        "yDM   h:ms",
        "yMD   h:ms",
        "D B Y h:m:s",
        "B d, Y h:m:s",
        "D b Y h:m:s",
        "M/D/Y h:m:s z",
        "M/D/Y Z h:m:s",
        "smhyMD",
        "y||||M++++D   h===ms",
        "   yM[][D   h:,.,.,ms  ",
        "\tkkkMy++D   h:ms\n",
        0
    };

    cout << "---------------------------" << endl;
    cout << "Test Formats" << endl;
    cout << "---------------------------" << endl << endl;

    for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
        CTime t1(2001, 4, 2, 3, 4, 5, 0,
                 strchr(*fmt, 'Z') ? CTime::eGmt : CTime::eLocal);

        CTime::SetFormat(*fmt);
        string t1_str = t1.AsString();
        cout << "[" << t1_str << "]";

        CTime::SetFormat("MDY__s");

        CTime t2(t1_str, *fmt);
        cout << " --> [" << t1_str << "]" << endl;
        assert(t1 == t2);

        CTime::SetFormat(*fmt);
        string t2_str = t2;
        assert(t1_str.compare(t2_str) == 0);
    }

    // Check against well-known dates
    const char fmtstr[] = "M/D/Y h:m:s Z W";
    {{
        CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("02/10/2003 20:40:30 GMT Monday") == 0);
    }}
    {{
        CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("02/10/1998 20:40:30 GMT Tuesday") == 0);
    }}
    {{
        CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("03/13/2003 15:49:30 GMT Thursday") == 0);
    }}
    {{
        CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("03/13/2001 15:49:30 GMT Tuesday") == 0);
    }}
    {{
        CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("12/31/2002 23:59:59 GMT Tuesday") == 0);
    }}
    {{
        CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("01/01/2003 00:00:00 GMT Wednesday") == 0);
    }}
    {{
        CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eGmt);
        t.SetFormat(fmtstr);
        string s = t.AsString();
        assert(s.compare("12/13/2002 12:34:56 GMT Friday") == 0);
    }}
}


//============================================================================
//
// TestGMT
//
//============================================================================

static void s_TestGMT(void)
{
    cout << "---------------------------" << endl;
    cout << "Test GMT and Local time"     << endl;
    cout << "---------------------------" << endl << endl;

    {{
        cout << "Write time in timezone format" << endl;

        CTime::SetFormat("M/D/Y h:m:s Z");

        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eGmt);
        cout << "[" << t1.AsString() << "]" << endl;
        assert(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        cout << "[" << t2.AsString() << "]" << endl;
        assert(t2.AsString() == "03/12/2001 11:22:33 ");

        CTime t3(CTime::eCurrent, CTime::eLocal);
        cout << "Local time [" << t3.AsString() << "]" << endl;
        CTime t4(CTime::eCurrent, CTime::eGmt);
        cout << "GMT time   [" << t4.AsString() << "]" << endl;
        cout << endl;
    }}
    {{   
        cout << "Process timezone string" << endl;

        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t = "03/12/2001 11:22:33 GMT";
        cout << "[" << t.AsString() << "]" << endl;
        assert(t.AsString() == "03/12/2001 11:22:33 GMT");
        t = "03/12/2001 11:22:33 ";
        cout << "[" << t.AsString() << "]" << endl;
        assert(t.AsString() == "03/12/2001 11:22:33 ");
        cout << endl;
    }}
    {{   
        cout << "Day of week" << endl;

        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i = 0; t <= CTime(2001, 4, 10); t++,i++) {
            cout << t.AsString() << " is " << t.DayOfWeek() << endl;
            assert(t.DayOfWeek() == (i%7));
        }
        cout << endl;
    }}
    //------------------------------------------------------------------------
    {{   
        cout << "Test GetTimeT" << endl;

        time_t timer=time(0);
        CTime tg(CTime::eCurrent, CTime::eGmt, CTime::eTZPrecisionDefault);
        CTime tl(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        CTime t(timer);
        cout << "[" << t.AsString() << "] " << endl;
        cout << tg.GetTimeT()/3600 << " - " << tl.GetTimeT()/3600 << " - ";
        cout << t.GetTimeT()/3600 << " - " << timer/3600 << endl;
        assert(timer == tg.GetTimeT());
        assert(timer == tl.GetTimeT());
        assert(timer == t.GetTimeT());
        cout << endl;

        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 4, 1, i>0 ? 2 : 1, i>0 ? (i-1) : 59, 
                     0, 0, CTime::eLocal, CTime::eHour);
            cout << tt.AsString() << " - " << tt.GetTimeT() / 3600 << endl; 
        }
        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 10, 28, i > 0 ? 1 : 0, i > 0 ? (i-1) : 59,
                     0, 0, CTime::eLocal, CTime::eHour);
            cout << tt.AsString() << " - " << tt.GetTimeT() / 3600 << endl; 
        }
        cout << endl;
    }}
    //------------------------------------------------------------------------
    {{   
        cout << "Test TimeZoneDiff (1)" << endl;

        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);

        cout << "[" << tw.AsString() << "] diff from GMT = " << \
            tw.TimeZoneDiff() / 3600 << endl;
        assert(tw.TimeZoneDiff() / 3600 == -5);
        cout << "[" << ts.AsString() << "] diff from GMT = " << \
            ts.TimeZoneDiff() / 3600 << endl;
        assert(ts.TimeZoneDiff()/3600 == -4);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -4) {
                cout << "First daylight saving day = [" << \
                    tw.AsString() << "]" << endl;
                break;
            }
        }
        cout << endl;
    }}
    //------------------------------------------------------------------------
    {{   
        cout << "Test TimeZoneDiff (2)" << endl;

        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        cout << "[" << tw.AsString() << "] diff from GMT = " << \
            tw.TimeZoneDiff() / 3600 << endl;
        assert(tw.TimeZoneDiff() / 3600 == -4);
        cout << "[" << ts.AsString() << "] diff from GMT = " << \
            ts.TimeZoneDiff() / 3600 << endl;
        assert(ts.TimeZoneDiff() / 3600 == -5);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -5) {
                cout << "First non daylight saving day = [" << \
                    tw.AsString() << "]" << endl;
                break;
             
            }
        }
        cout << endl;
    }}
    //------------------------------------------------------------------------
    {{   
        cout << "Test AdjustTime" << endl;

        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("04/01/2001 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eTZPrecisionDefault);
        cout << "init  [" << t.AsString() << "]" << endl;

        t.SetTimeZoneFormat(CTime::eGmt);
        cout << "GMT" << endl;
        tn = t + 5;  
        cout << "+5d   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "04/06/2001 01:01:00");
        tn = t + 40; 
        cout << "+40d  [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZoneFormat(CTime::eLocal);
        cout << "Local eNone" << endl;
        t.SetTimeZonePrecision(CTime::eNone);
        tn=t+5;  cout << "+5d   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "04/06/2001 01:01:00");
        tn=t+40; cout << "+40d  [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZonePrecision(CTime::eMonth);
        cout << "Local eMonth" << endl;
        tn = t + 5;
        cout << "+5d   [" << tn.AsString() << "]" << endl;
        tn = t; 
        tn.AddMonth(-1);
        cout << "-1m   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "03/01/2001 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        cout << "+1m   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "05/01/2001 02:01:00");

        t.SetTimeZonePrecision(CTime::eDay);
        cout << "Local eDay" << endl;
        tn = t - 1; 
        cout << "-1d   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "03/31/2001 01:01:00");
        tn++;   
        cout << "+0d   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "04/01/2001 01:01:00");
        tn = t + 1; 
        cout << "+1d   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "04/02/2001 02:01:00");

        cout << "Local eHour" << endl;
        t.SetTimeZonePrecision(CTime::eHour);
        tn = t; 
        tn.AddHour(-3);
        CTime te = t; 
        te.AddHour(3);
        cout << "-3h   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "03/31/2001 22:01:00");
        cout << "+3h   [" << te.AsString() << "]" << endl;
        assert(te.AsString() == "04/01/2001 05:01:00");
        CTime th = tn; 
        th.AddHour(49);
        cout << "+49h  [" << th.AsString() << "]" << endl;
        assert(th.AsString() == "04/03/2001 00:01:00");

        for (int i = 0;  i < 8;  i++,  tn.AddHour()) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn.AddHour(-1);
        for (int i = 0;  i < 8;  i++,  tn.AddHour(-1)) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn = "10/28/2001 00:01:00"; 
        cout << "init  [" << tn.AsString() << "]" << endl;
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-3); 
        te.AddHour(9);
        cout << "-3h   [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "10/27/2001 21:01:00");
        cout << "+9h   [" << te.AsString() << "]" << endl;
        assert(te.AsString() == "10/28/2001 08:01:00");
        th = tn; 
        th.AddHour(49);
        cout << "+49h  [" << th.AsString() << "]" << endl;
        assert(th.AsString() == "10/29/2001 21:01:00");

        tn.AddHour(+2);
        for (int i = 0;  i < 10;  i++,  tn.AddHour()) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;
        tn.AddHour(-1);
        for (int i = 0;  i < 10;  i++,  tn.AddHour(-1)) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn = "10/28/2001 09:01:00"; 
        cout << "init  [" << tn.AsString() << "]" << endl;
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-10); 
        te.AddHour(+10);
        cout << "-10h  [" << tn.AsString() << "]" << endl;
        assert(tn.AsString() == "10/28/2001 00:01:00");
        cout << "+10h  [" << te.AsString() << "]" << endl;
        assert(te.AsString() == "10/28/2001 19:01:00");
        
        cout << endl; 
        cout << endl;
    }}
}


//============================================================================
//
// TestGMTSpeedRun
//
//============================================================================

static void s_TestGMTSpeedRun(string comment, CTime::ETimeZone tz, 
                              CTime::ETimeZonePrecision tzp)
{
    CTime t(CTime::eCurrent, tz, tzp);
    CStopWatch timer;
    double duration;

#if defined    NCBI_OS_MSWIN
    const long kCount=100000L;
#elif defined  NCBI_OS_UNIX
    const long kCount=10000L;
#else       // NCBI_OS_MAC
    const long kCount=100000L;
#endif

    t.SetFormat("M/D/Y h:m:s");
    t = "03/31/2001 00:00:00"; 

    cout << "Minute add, "<< comment << endl;
    cout << "Iterations  = " << kCount << endl;

    timer.Start();
    for (long i = 0; i < kCount; i++) {
        t.AddMinute();
    }
    duration = timer.Elapsed();
    cout << "Duration    = " << duration << " sec." << endl;
    cout << endl;
}


//============================================================================
//
// TestGMTSpeed
//
//============================================================================

static void s_TestGMTSpeed(void)
{
    cout << "---------------------------" << endl;
    cout << "Test CTime Speed"            << endl;
    cout << "---------------------------" << endl << endl;

    s_TestGMTSpeedRun("eLocal - eMinute", CTime::eLocal, CTime::eMinute);
    s_TestGMTSpeedRun("eLocal - eHour"  , CTime::eLocal, CTime::eHour);
    s_TestGMTSpeedRun("eLocal - eMonth" , CTime::eLocal, CTime::eMonth);
    s_TestGMTSpeedRun("eLocal - eNone"  , CTime::eLocal, CTime::eNone);
    s_TestGMTSpeedRun("eGmt   - eNone"  , CTime::eGmt, CTime::eNone);
}


//============================================================================
//
// TestTimeSpan
//
//============================================================================

static void s_TestTimeSpan(void)
{
    cout << "---------------------------" << endl;
    cout << "Test TimeSpan"               << endl;
    cout << "---------------------------" << endl << endl;

    // Common constructors
    {{
        CTimeSpan t1(0,0,0,1,-2);
        cout << t1.AsString() << endl;
        assert(t1.AsString() == "0.999999998");
        CTimeSpan t2(0,0,0,-1,2);
        cout << t2.AsString() << endl;
        assert(t2.AsString() == "-0.999999998");
        CTimeSpan t3(0,0,0,0,-2);
        cout << t3.AsString() << endl;
        assert(t3.AsString() == "-0.000000002");
        CTimeSpan t4(0,0,0,0,2);
        cout << t4.AsString() << endl;
        assert(t4.AsString() == "0.000000002");
    }}
    {{
        CTimeSpan t1(2,3,4,5,6);
        assert(t1.GetCompleteHours()   == 51);
        assert(t1.GetCompleteMinutes() == (51*60+4));
        assert(t1.GetCompleteSeconds() == ((51*60+4)*60+5));
        assert(t1.GetNanoSecondsAfterSecond() == 6);
        cout << t1.AsString() << endl;
        assert(t1.AsString() == "183845.000000006");

        CTimeSpan t2(-2,-3,-4,-5,-6);
        assert(t2.GetCompleteHours()   == -51);
        assert(t2.GetCompleteMinutes() == -(51*60+4));
        assert(t2.GetCompleteSeconds() == -((51*60+4)*60+5));
        assert(t2.GetNanoSecondsAfterSecond() == -6);
        cout << t2.AsString() << endl;
        assert(t2.AsString() == "-183845.000000006");

        CTimeSpan t3(-2,+3,-4,-5,+6);
        assert(t3.GetCompleteHours()   == -45);
        assert(t3.GetCompleteMinutes() == -(45*60+4));
        assert(t3.GetCompleteSeconds() == -((45*60+4)*60+4));
        assert(t3.GetNanoSecondsAfterSecond() == -999999994);
        cout << t3.AsString() << endl;
        assert(t3.AsString() == "-162244.999999994");
    }}

    // Comparison
    {{
        CTimeSpan t0;
        CTimeSpan t1(123.4);
        CTimeSpan t2(123.45);
        CTimeSpan t3(123.4);

        assert(t0.GetSign() == eZero);
        assert(t1.GetSign() == ePositive);
        assert(t0 == CTimeSpan(0,0,0,0,0));
        assert(t1 != t2);
        assert(t1 == t3);
        assert(t1 <  t2);
        assert(t2 >  t1);
        assert(t1 <= t2);
        assert(t2 >= t2);
    }}

    // Assignment
    {{
        CTimeSpan t0;
        CTimeSpan t1(-123.4);
        CTimeSpan t2(123.45);

        t1 = t2;
        assert(t1 == t2);
        t1 = "-123.450000000";
        assert(t1 == -t2);
    }}

    // Arithmetics
    {{
        CTimeSpan t;
        t = CTimeSpan("100.3") - CTimeSpan("123.4");
        assert(t == CTimeSpan("-23.1"));
        t = CTimeSpan("63.7")  + CTimeSpan("2.3");
        assert(t == CTimeSpan("65.10"));
        t = CTimeSpan("63.7")  - CTimeSpan("72.6");
        assert(t == CTimeSpan("-8.999999999"));

        t = "-123.4";
        t += CTimeSpan("1.0");  
        assert(t == CTimeSpan("-122.4"));
        t += CTimeSpan("222.4");  
        assert(t == CTimeSpan("100.0") );
        t -= CTimeSpan("50.1");  
        assert(t == CTimeSpan("49.999999999"));
        t += CTimeSpan("0.1");  
        assert(t == CTimeSpan("50.0"));
        t += CTimeSpan("3.7");  
        assert(t == CTimeSpan("53.7"));
        t -= CTimeSpan("3.8");  
        assert(t == CTimeSpan("49.999999999"));
    }}

    // Formats
    {{
        static const char* s_Fmt[] = {
            "d h:m:s.n",
            "H m:s",
            "S",
            "H",
            "M",
            "d",
            "-d h:m:s.n",
            "-H m:s",
            "-S",
            "-H",
            "-M",
            "-d",
            0
        };

        for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
            CTimeSpan t1(-123456789.987654321);

            CTimeSpan::SetFormat(*fmt);
            string t1_str = t1.AsString();
            cout << "[" << t1_str << "]";
            CTimeSpan::SetFormat("_s_");

            CTimeSpan t2(t1_str, *fmt);
            CTimeSpan::SetFormat(*fmt);
            string t2_str = t2;
            cout << " --> [" << t2_str << "]" << endl;
            assert(t1_str.compare(t2_str) == 0);
        }
    }}

    cout << endl;
}


//============================================================================
//
// DemoStopWatch
//
//============================================================================

static void s_DemoStopWatch(void)
{
    cout << "---------------------------" << endl;
    cout << "Demo StopWatch"            << endl;
    cout << "---------------------------" << endl << endl;
    
    CStopWatch sw;
    sw.SetFormat("S.n");
    sw.Start();
    for (int i=0; i<10; i++) {
        cout << sw << endl;
    }
}


//============================================================================
//
// MAIN
//
//============================================================================

int main()
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Reinit global timezone variables
    tzset();

    // Run tests
    try {
        s_TestMisc();
        s_TestFormats();
        s_TestGMT();
        s_TestGMTSpeed();
        s_TestTimeSpan();
        s_DemoStopWatch();
    } catch (CException& e) {
        ERR_POST(Fatal << e);
    }
    // Success
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.24  2004/09/07 18:48:06  ivanov
 * CTimeSpan:: renamed GetTotal*() -> GetComplete*()
 *
 * Revision 6.23  2004/09/07 16:33:19  ivanov
 * + s_TestTimeSpan(), s_DemoStopWatch()
 *
 * Revision 6.22  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.21  2004/03/25 17:35:17  ivanov
 * Added tests for 'z' time format
 *
 * Revision 6.20  2003/11/25 20:03:56  ivanov
 * Fixed misspelled eTZPrecisionDefault
 *
 * Revision 6.19  2003/11/25 19:56:38  ivanov
 * Renamed eDefault to eTZPrecisionDefault.
 * Some cosmetic changes.
 *
 * Revision 6.18  2003/10/03 18:27:20  ivanov
 * Added tests for month and day of week names conversion functions
 *
 * Revision 6.17  2003/07/15 19:37:50  vakatov
 * Added test with weekday and timezone
 *
 * Revision 6.16  2003/04/16 20:29:26  ivanov
 * Using class CStopWatch instead clock()
 *
 * Revision 6.15  2003/02/10 17:46:25  lavr
 * Added more checks for well-known dates
 *
 * Revision 6.14  2002/10/17 16:56:03  ivanov
 * Added tests for 'b' and 'B' time format symbols
 *
 * Revision 6.13  2002/07/15 18:17:26  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.12  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.11  2002/04/16 18:49:09  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.10  2001/09/27 18:02:45  ivanov
 * Fixed division at zero in test of speed CTime class on fast computers.
 *
 * Revision 6.9  2001/07/23 15:51:46  ivanov
 * Changed test for work with DB-time formats
 *
 * Revision 6.8  2001/07/06 15:08:36  ivanov
 * Added tests for DataBase-time's
 *
 * Revision 6.7  2001/05/29 16:14:02  ivanov
 * Return to nanosecond-revision. Corrected mistake of the work with local
 * time on Linux. Polish and improvement source code.
 * Renamed AsTimeT() -> GetTimerT().
 *
 * Revision 6.6  2001/04/30 22:01:31  lavr
 * Rollback to pre-nanosecond-revision due to necessity to use
 * configure to figure out names of global variables governing time zones
 *
 * Revision 6.5  2001/04/29 03:06:10  lavr
 * #include <time.h>" moved from .cpp to ncbitime.hpp
 *
 * Revision 6.4  2001/04/27 20:39:47  ivanov
 * Tests for check Local/UTC time and Nanoseconds added.
 * Also speed test added.
 *
 * Revision 6.3  2000/11/21 18:15:05  butanaev
 * Fixed bug in operator ++/-- (int)
 *
 * Revision 6.2  2000/11/21 15:22:57  vakatov
 * Do not enforce "_DEBUG" -- it messes up the MSVC++ compilation
 *
 * Revision 6.1  2000/11/20 22:17:49  vakatov
 * Added NCBI date/time class CTime ("ncbitime.[ch]pp") and
 * its test suite ("test/test_ncbitime.cpp")
 *
 * ===========================================================================
 */
