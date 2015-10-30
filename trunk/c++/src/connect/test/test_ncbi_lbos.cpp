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
 * Author:  Dmitriy Elisov
 *
 * File Description:
 *   lbos mapper tests
 *
 */

#include <ncbi_pch.hpp>
#include "test_ncbi_lbos_common.hpp"                                                        
#include    <corelib/test_boost.hpp>

USING_NCBI_SCOPE;


NCBITEST_INIT_TREE()
{}


NCBITEST_INIT_CMDLINE(args)
{
    args->AddOptionalPositional("lbos", "Primary address to LBOS",
                               CArgDescriptions::eString);
}

#ifdef NCBI_THREADS
static CHealthcheckThread* s_HealthchecKThread;

NCBITEST_AUTO_FINI()
{
    s_HealthchecKThread->Stop(); // Stop listening on the socket
    s_HealthchecKThread->Join();
}
#endif

/* We might want to clear ZooKeeper from nodes before running tests.
 * This is generally not good, because if this test application runs
 * on another host at the same moment, it will miss a lot of nodes and
 * tests will fail.
 */
#define DEANNOUNCE_ALL_BEFORE_TEST 0

NCBITEST_AUTO_INIT()
{
    CConnNetInfo net_info;
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "lbos mapper Unit Test");
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(&config);
    if (CNcbiApplication::Instance()->GetArgs()["lbos"]) {
        string custom_lbos = 
            CNcbiApplication::Instance()->GetArgs()["lbos"].AsString();
        CNcbiApplication::Instance()->GetConfig().Set("CONN", 
                                                      "LBOS", 
                                                      custom_lbos);
    }
    ERR_POST(Info << "LBOS=" <<
        CNcbiApplication::Instance()->GetConfig().Get("CONN", "LBOS"));
    LBOS_Deannounce("/lbostest", /* For initialization of LBOS mapper.    */
                    "1.0.0",     /* We actually do not want to deannounce */
                    "lbos.dev.be-md.ncbi.nlm.nih.gov", /* anything!       */
                    5000, NULL, NULL);
#ifdef NCBI_THREADS
    s_HealthchecKThread = new CHealthcheckThread;
    s_HealthchecKThread->Run();
#endif
#if DEANNOUNCE_ALL_BEFORE_TEST
    size_t start = 0, end = 0;
    /*
     * Deannounce all lbostest servers (they are left if previous
     * launch of test crashed)
     */
    CCObjHolder<char> lbos_output_orig(g_LBOS_UnitTesting_GetLBOSFuncs()->
            UrlReadAll(*net_info, "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080"
            "/lbos/text/service", NULL, NULL));
    if (*lbos_output_orig == NULL) 
        lbos_output_orig = strdup("");
    string lbos_output = *lbos_output_orig;
    const char* str1 = "/lbostest";
    const char* str2 = "/lbostest1";
    vector<string> to_find;
    to_find.push_back(str1);
    to_find.push_back(str2);

    unsigned int i = 0;
    for (i = 0;  i < to_find.size();  ++i) {
        while (start != string::npos) {
            start = lbos_output.find(to_find[i] + "\t", start);
            if (start == string::npos)
                break;
            // We already know service name since we searched for it.
            start = lbos_output.find("\t", start); //skip service name
            start += 1; //skip \t
            // Next, we extract version.
            end = lbos_output.find("\t", start); //version
            string version = lbos_output.substr(start, end-start);
            // Now we extract ip
            start = end + 1; //skip \t
            end = lbos_output.find(":", start); //skip ip
            string ip = lbos_output.substr(start, end-start);
            // Now we extract port
            start = end + 1; //skip ":"
            end = lbos_output.find("\t", start);
            unsigned short port =
                    NStr::StringToInt(lbos_output.substr(start, end-start));
            // We skip healthcheck
            start = end + 1; //skip \t
            end = lbos_output.find("\t", start);
            // Check if service is announced are just hangs until being deleted
            start = end + 1; //skip \t
            end = lbos_output.find("\t", start);
            string is_announced = lbos_output.substr(start, end-start);
            if (is_announced == "true") {
                try {
                    LBOS::Deannounce(to_find[i], version, ip, port);
                }
                catch (const CLBOSException&) {
                }
            }
        }
        start = 0; // reset search for the next service
    }
#endif /* DEANNOUNCE_ALL_BEFORE_TEST */
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Compose_LBOS_address )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */


/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos)
{
    Compose_LBOS_address::LBOSExists__ShouldReturnLbos();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL)
{
    Compose_LBOS_address::RoleFail__ShouldReturnNULL();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL)
{
    Compose_LBOS_address::DomainFail__ShouldReturnNULL();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Reset_iterator)/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should make capacity of elements in data->cand equal zero
 * 2. Should be able to reset iter N times consequently without crash
 * 3. Should be able to "reset iter, then getnextinfo" N times consequently
 *    without crash
 */

BOOST_AUTO_TEST_CASE(SERV_Reset__NoConditions__IterContainsZeroCandidates)
{
    Reset_iterator::NoConditions__IterContainsZeroCandidates();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__MultipleReset__ShouldNotCrash)
{
    Reset_iterator::MultipleReset__ShouldNotCrash();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash)
{
    Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Close_iterator )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should work immediately after Open
 * 2. Should work immediately after Reset
 * 3. Should work immediately after Open, GetNextInfo
 * 4. Should work immediately after Open, GetNextInfo, Reset
 */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterOpen__ShouldWork)
{
    Close_iterator::AfterOpen__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterReset__ShouldWork)
{
    Close_iterator::AfterReset__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterGetNextInfo__ShouldWork)
{
    Close_iterator::AfterGetNextInfo__ShouldWork();
}

/* In this test we check three different situations: when getnextinfo was
 * called once, was called to see half of all found servers, and was called to
 * iterate through all found servers. */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__FullCycle__ShouldWork)
{
    Close_iterator::FullCycle__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Dtab )//////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Mix of registry DTab and HTTP Dtab: registry goes first */
BOOST_AUTO_TEST_CASE(DTab__DTabRegistryAndHttp__RegistryGoesFirst) 
{
    DTab::DTabRegistryAndHttp__RegistryGoesFirst();
}

/** 2. Announce server with non-standard version and server with no standard 
    version at all. Both should be found via DTab                            */
BOOST_AUTO_TEST_CASE(DTab__NonStandardVersion__FoundWithDTab)
{
    DTab::NonStandardVersion__FoundWithDTab();
}
BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Resolve_via_LBOS )//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if lbos answered "not found"
 * 3. Should return NULL if lbos is not reachable
 * 4. Should be able to support up to M IP:port combinations (not checking for
 *    repeats) with storage overhead not more than same as size needed (that
 *    is, all space consumed is twice as size needed, used and unused space
 *    together)
 * 5. Should be able to correctly process incorrect lbos output. If from 5
 *    servers one is not valid, 4 other servers should be still returned
 *    by mapper
 */

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP)
{
    Resolve_via_LBOS::ServiceExists__ReturnHostIP();
}


BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL)
{
    Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL)
{
    Resolve_via_LBOS::NoLBOS__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess)
{
    Resolve_via_LBOS::FakeMassiveInput__ShouldProcess();
}



BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_LBOS_address_suite )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__SpecificMethod__FirstInResult)
{
    Get_LBOS_address::SpecificMethod__FirstInResult();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(
        g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost)
{
    Get_LBOS_address::CustomHostNotProvided__SkipCustomHost();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__NoConditions__AddressDefOrder)
{
    Get_LBOS_address::NoConditions__AddressDefOrder();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_candidates )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Iterate through received lbos's addresses, if there is not response from
 *    current lbos
 * 2. If one lbos works, do not try another lbos
 * 3. If net_info was provided for Serv_OpenP, the same net_info should be
 *    available while getting candidates via lbos to provide DTABs*/

/* We need this variables and function to count how many times algorithm will
 * try to resolve IP, and to know if it uses the headers we provided.
 */

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS) {
    Get_candidates::LBOSNoResponse__SkipLBOS();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSResponse__Finish) {
    Get_candidates::LBOSResponds__Finish();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo) {
    Get_candidates::NetInfoProvided__UseNetInfo();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_Next_Info )/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * GetNextInfo:
 * 1. If no candidates found yet, or reset was just made, get candidates
 *    and return first
 * 2. If no candidates found yet, or reset was just made, and unrecoverable
 *    error while getting candidates, return 0
 * 3. If candidates already found, return next
 * 4. If last candidate was already returned, return 0
 * 5. If data is NULL for some reason, construct new data
 * 6. If iter is NULL, return NULL
 * 7. If SERV_MapperName(iter) returns name of another mapper, return NULL
 */

/* To be sure that mapper returns objects correctly, we need to be sure of
 * what lbos sends to mapper. The best way is to emulate lbos
 */

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates)
{
    GetNextInfo::EmptyCands__RunGetCandidates();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull)
{
    GetNextInfo::ErrorUpdating__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__HaveCands__ReturnNext)
{
    GetNextInfo::HaveCands__ReturnNext();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull)
{
    GetNextInfo::LastCandReturned__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__DataIsNull__ReconstructData)
{
    GetNextInfo::DataIsNull__ReconstructData();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__IterIsNull__ReturnNull)
{
    GetNextInfo::IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__WrongMapper__ReturnNull)
{
    GetNextInfo::WrongMapper__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Open )//////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* Open:
 * 1. If iter is NULL, return NULL
 * 2. If net_info is NULL, construct own net_info
 * 3. If read from lbos successful, return s_op
 * 4. If read from lbos successful and host info pointer != NULL, write NULL
 *    to host info
 * 5. If read from lbos unsuccessful or no such service, return 0
 */
BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__IterIsNull__ReturnNull)
{
    Open::IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NetInfoNull__ConstructNetInfo)
{
    Open::NetInfoNull__ConstructNetInfo();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations)
{
    Open::ServerExists__ReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__InfoPointerProvided__WriteNull)
{
    Open::InfoPointerProvided__WriteNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NoSuchService__ReturnNull)
{
    Open::NoSuchService__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( GeneralLBOS )///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * We use such name of service that we MUST get lbos's answer
 * We check that it finds service and returns non-NULL list of
 * operations
 */
BOOST_AUTO_TEST_CASE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations)
{
    GeneralLBOS::ServerExists__ShouldReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull)
{
    GeneralLBOS::ServerDoesNotExist__ShouldReturnNull();
}

BOOST_AUTO_TEST_CASE(TestLbos_FindMethod__LbosExist__ShouldWork)
{
    GeneralLBOS::LbosExist__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Initialization )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
*    initialized should not crash
* 2. At initialization if no lbos found, mapper must turn OFF
* 3. At initialization if lbos found, mapper should be ON
* 4. If lbos has not yet been initialized, it should be initialized
*    at SERV_LBOS_Open()
* 5. If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()
* 6. s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_
*    Initialize()
* 7. s_LBOS_InstancesList MUST not be NULL at beginning of
*    s_LBOS_FillCandidates()
* 8. s_LBOS_FillCandidates() should switch first and good lbos
*    addresses, if first is not responding
*/

/** Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
 * initialized should not crash                                              */
BOOST_AUTO_TEST_CASE(Initialization__MultithreadInitialization__ShouldNotCrash)
{
     Initialization::MultithreadInitialization__ShouldNotCrash();
}
/** At initialization if no lbos found, mapper must turn OFF                 */
BOOST_AUTO_TEST_CASE(Initialization__InitializationFail__TurnOff)
{
     Initialization::InitializationFail__TurnOff();
}
/** At initialization if lbos found, mapper should be ON                     */
BOOST_AUTO_TEST_CASE(Initialization__InitializationSuccess__StayOn)
{
     Initialization::InitializationSuccess__StayOn();
}
/** If lbos has not yet been initialized, it should be initialized at
 * SERV_LBOS_Open()                                                          */
BOOST_AUTO_TEST_CASE(Initialization__OpenNotInitialized__ShouldInitialize)
{
     Initialization::OpenNotInitialized__ShouldInitialize();
}
/** If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()              */
BOOST_AUTO_TEST_CASE(Initialization__OpenWhenTurnedOff__ReturnNull)
{
     Initialization::OpenWhenTurnedOff__ReturnNull();
}
/** s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_Initialize()*/
BOOST_AUTO_TEST_CASE(
                Initialization__s_LBOS_Initialize__s_LBOS_InstancesListNotNULL)
{
     Initialization::s_LBOS_Initialize__s_LBOS_InstancesListNotNULL();
}
/** s_LBOS_InstancesList MUST not be NULL at beginning of
 * s_LBOS_FillCandidates()                                                   */
BOOST_AUTO_TEST_CASE(
            Initialization__s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL)
{
     Initialization::s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL();
}
/** s_LBOS_FillCandidates() should switch first and good lbos addresses, if
   first is not responding                                                   */
BOOST_AUTO_TEST_CASE(Initialization__PrimaryLBOSInactive__SwapAddresses)
{
     Initialization::PrimaryLBOSInactive__SwapAddresses();
}
BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceTest )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*  1. Successfully announced: return SUCCESS
 *  2. Successfully announced: SLBOS_AnnounceHandle deannounce_handle 
 *     contains info needed to later deannounce announced server
 *  3. Successfully announced: char* lbos_answer contains answer of lbos
 *  4. Successfully announced: information about announcement is saved to 
 *     hidden lbos mapper's storage
 *  5. Could not find lbos: return NO_LBOS
 *  6. Could not find lbos: char* lbos_answer is set to NULL
 *  7. Could not find lbos: SLBOS_AnnounceHandle deannounce_handle is set to
 *     NULL
 *  8. lbos returned error: return LBOS_ERROR
 *  9. lbos returned error: char* lbos_answer contains answer of lbos
 * 10. lbos returned error: SLBOS_AnnounceHandle deannounce_handle is set to 
 *     NULL
 * 11. Server announced again (service name, IP and port coincide) and 
 *     announcement in the same zone, replace old info about announced 
 *     server in internal storage with new one.
 * 12. Server announced again and trying to announce in another 
 *     zone - return MULTIZONE_ANNOUNCE_PROHIBITED
 * 13. Was passed incorrect healthcheck URL (NULL or empty not starting with 
 *     "http(s)://"): do not announce and return INVALID_ARGS
 * 14. Was passed incorrect port (zero): do not announce and return 
 *     INVALID_ARGS
 * 15. Was passed incorrect version(NULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 16. Was passed incorrect service nameNULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 17. Real-life test: after announcement server should be visible to 
 *     resolve
 * 18. If was passed "0.0.0.0" as IP, should replace it with local IP or 
 *     hostname
 * 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host 
 *     IP: do not announce and return DNS_RESOLVE_ERROR
 * 20. lbos is OFF - return eLBOS_Off                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   */

/*  1. Successfully announced : return SUCCESS                               */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__ReturnSuccess)
{
    Announcement::AllOK__ReturnSuccess();
}

/*  3. Successfully announced : char* lbos_answer contains answer of lbos    */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__LBOSAnswerProvided)
{
    Announcement::AllOK__LBOSAnswerProvided();
}

/*  3. Successfully announced : char* lbos_answer contains answer of lbos    */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__LBOSStatusMessageIsOK)
{
    Announcement::AllOK__LBOSStatusMessageIsOK();
}

/*  4. Successfully announced: information about announcement is saved to
 *     hidden lbos mapper's storage                                          */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__AnnouncedServerSaved)
{
    Announcement::AllOK__AnnouncedServerSaved();
}
/*  5. Could not find lbos: return NO_LBOS                                   */
BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__ReturnNoLBOSAndNotFind)
{
    Announcement::NoLBOS__ReturnNoLBOSAndNotFind();
}
/*  6. Could not find lbos : char* lbos_answer is set to NULL                */
BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__LBOSAnswerNull)
{
    Announcement::NoLBOS__LBOSAnswerNull();
}

BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__LBOSStatusMessageNull)
{
    Announcement::NoLBOS__LBOSStatusMessageNull();
}

/*  8. lbos returned error: return eLBOS_ServerError                         */
BOOST_AUTO_TEST_CASE(Announcement__LBOSError__ReturnServerErrorCode)
{
    Announcement::LBOSError__ReturnServerErrorCode();
}

BOOST_AUTO_TEST_CASE(Announcement__LBOSError__ReturnServerStatusMessage)
{
    Announcement::LBOSError__ReturnServerStatusMessage();
}

/*  9. lbos returned error : char* lbos_answer contains answer of lbos       */
BOOST_AUTO_TEST_CASE(Announcement__LBOSError__LBOSAnswerProvided)
{
    Announcement::LBOSError__LBOSAnswerProvided();
}
/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
BOOST_AUTO_TEST_CASE(
                 Announcement__AlreadyAnnouncedInTheSameZone__ReplaceInStorage)
{
    Announcement::AlreadyAnnouncedInTheSameZone__ReplaceInStorage();
}
/* 12. Trying to announce in another domain - do nothing                     */
BOOST_AUTO_TEST_CASE(
        Announcement__AnotherRegion__NoAnnounce)
{
    Announcement::AnotherRegion__NoAnnounce();
}
/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectURL__ReturnInvalidArgs)
{
    Announcement::IncorrectURL__ReturnInvalidArgs();
}
/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectPort__ReturnInvalidArgs)
{
    Announcement::IncorrectPort__ReturnInvalidArgs();
}
/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectVersion__ReturnInvalidArgs)
{
    Announcement::IncorrectVersion__ReturnInvalidArgs();
}
/* 16. Was passed incorrect service name (NULL or empty): do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectServiceName__ReturnInvalidArgs)
{
    Announcement::IncorrectServiceName__ReturnInvalidArgs();
}
/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
BOOST_AUTO_TEST_CASE(Announcement__RealLife__VisibleAfterAnnounce)
{
    Announcement::RealLife__VisibleAfterAnnounce();
}
/* 18. If was passed "0.0.0.0" as IP, should replace it with local IP or
 *     hostname                                                              */
BOOST_AUTO_TEST_CASE(Announcement__IP0000__DoNotReplace)
{
    Announcement::IP0000__DoNotReplace();
}
/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP : do not announce and return DNS_RESOLVE_ERROR                     */
BOOST_AUTO_TEST_CASE(
                   Announcement__ResolveLocalIPError__AllOK)
{
    Announcement::ResolveLocalIPError__AllOK();
}
/* 20. lbos is OFF - return eLBOS_Off                                        */
BOOST_AUTO_TEST_CASE(Announcement__LBOSOff__ReturnKLBOSOff)
{
    Announcement::LBOSOff__ReturnKLBOSOff();
}
/*21. Announced successfully, but LBOS return corrupted answer -
      return SERVER_ERROR                                                    */
BOOST_AUTO_TEST_CASE(
                    Announcement__LBOSAnnounceCorruptOutput__Return454)
{
    Announcement::LBOSAnnounceCorruptOutput__Return454();
}
/*22. Trying to announce server and providing dead healthcheck URL - 
      return eLBOS_NotFound                                                  */
BOOST_AUTO_TEST_CASE(Announcement__HealthcheckDead__ReturnKLBOSSuccess)
{
    Announcement::HealthcheckDead__ReturnKLBOSSuccess();
}
/*23. Trying to announce server and providing dead healthcheck URL -
      server should not be announced                                         */
BOOST_AUTO_TEST_CASE(Announcement__HealthcheckDead__AnnouncementOK)
{
    Announcement::HealthcheckDead__AnnouncementOK();
}
BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceRegistryTest )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*   1. All parameters good (Custom section has all parameters correct in 
 *      config) - return eLBOS_Success
 *   2. Custom section has nothing in config - return eLBOS_InvalidArgs
 *   3. Section empty or NULL (should use default section and return 
 *      eLBOS_Success)
 *   4. Service is empty or NULL - return eLBOS_InvalidArgs
 *   5. Version is empty or NULL - return eLBOS_InvalidArgs
 *   6. port is empty or NULL - return eLBOS_InvalidArgs
 *   7. port is out of range - return eLBOS_InvalidArgs
 *   8. port contains letters - return eLBOS_InvalidArgs
 *   9. healthcheck is empty or NULL - return eLBOS_InvalidArgs
 *  10. healthcheck does not start with http:// or https:// - return 
 *      eLBOS_InvalidArgs                                                    
 *  11. Trying to announce server and providing dead healthcheck URL -
 *      return eLBOS_NotFound                                                */

/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success                                       */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__ParamsGood__ReturnSuccess) 
{
    return AnnouncementRegistry::ParamsGood__ReturnSuccess();
}
/*  2.  Custom section has nothing in config - return eLBOS_InvalidArgs      */
BOOST_AUTO_TEST_CASE(
                  AnnouncementRegistry__CustomSectionNoVars__ReturnInvalidArgs)
{
    return AnnouncementRegistry::CustomSectionNoVars__ReturnInvalidArgs();
}
/*  3.  Section empty or NULL (should use default section and return 
        eLBOS_Success)                                                       */
BOOST_AUTO_TEST_CASE(
AnnouncementRegistry__CustomSectionEmptyOrNullAndDefaultSectionIsOk__ReturnSuccess)
{
    return AnnouncementRegistry::
                 CustomSectionEmptyOrNullAndDefaultSectionIsOk__ReturnSuccess();
}
/*  4.  Service is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
                   AnnouncementRegistry__ServiceEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::ServiceEmptyOrNull__ReturnInvalidArgs();
}
/*  5.  Version is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
                   AnnouncementRegistry__VersionEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::VersionEmptyOrNull__ReturnInvalidArgs();
}
/*  6.  port is empty or NULL - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__PortEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortEmptyOrNull__ReturnInvalidArgs();
}
/*  7.  port is out of range - return eLBOS_InvalidArgs                      */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__PortOutOfRange__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortOutOfRange__ReturnInvalidArgs();
}
/*  8.  port contains letters - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(
                  AnnouncementRegistry__PortContainsLetters__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortContainsLetters__ReturnInvalidArgs();
}
/*  9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs              */
BOOST_AUTO_TEST_CASE(
              AnnouncementRegistry__HealthchecktEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::HealthchecktEmptyOrNull__ReturnInvalidArgs();
}
/*  10. healthcheck does not start with http:// or https:// - return         
        eLBOS_InvalidArgs                                                    */  
BOOST_AUTO_TEST_CASE(
      AnnouncementRegistry__HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs)
{
    return AnnouncementRegistry::
                          HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs();
}
/*  11. Trying to announce server and providing dead healthcheck URL -
        return eLBOS_NotFound                                                */
BOOST_AUTO_TEST_CASE(
                   AnnouncementRegistry__HealthcheckDead__ReturnKLBOSSuccess)
{
    return AnnouncementRegistry::HealthcheckDead__ReturnKLBOSSuccess();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Deannounce )////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Successfully deannounced : return 1
 * 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it
 * 3. Could not connect to provided lbos : fail and return 0
 * 4. Successfully connected to lbos, but deannounce returned error : return 0
 * 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                            
 * 6. Another domain - do not deannounce 
 * 7. Deannounce without IP specified - deannounce from local host 
 * 8. lbos is OFF - return eLBOS_Off                                         */
 /* 1. Successfully deannounced: return 1                                    */
BOOST_AUTO_TEST_CASE(Deannouncement__Deannounced__Return1)
{
    /* Here we specifiy port (specify that we do not intend to use specific 
     * port), because this function is used inside one another test 
     * (Deannouncement__RealLife__InvisibleAfterDeannounce)    */
    Deannouncement::Deannounced__Return1(0);
}
/* 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it                                                              */
BOOST_AUTO_TEST_CASE(Deannouncement__Deannounced__AnnouncedServerRemoved)
{
    Deannouncement::Deannounced__AnnouncedServerRemoved();
}
/* 3. Could not connect to provided lbos : fail and return 0                 */
BOOST_AUTO_TEST_CASE(Deannouncement__NoLBOS__Return0)
{
    Deannouncement::NoLBOS__Return0();
}
/* 4. Successfully connected to lbos, but deannounce returned error: 
 *    return 0                                                               */
BOOST_AUTO_TEST_CASE(Deannouncement__LBOSExistsDeannounce400__Return400)
{
    Deannouncement::LBOSExistsDeannounce400__Return400();
}
/* 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                             */
BOOST_AUTO_TEST_CASE(Deannouncement__RealLife__InvisibleAfterDeannounce)
{
    Deannouncement::RealLife__InvisibleAfterDeannounce();
}
/*6. If trying to deannounce in another domain - do not deannounce           */
BOOST_AUTO_TEST_CASE(Deannouncement__AnotherDomain__DoNothing)
{
    Deannouncement::AnotherDomain__DoNothing();
}
/* 7. Deannounce without IP specified - deannounce from local host           */
BOOST_AUTO_TEST_CASE(Deannouncement__NoHostProvided__LocalAddress)
{
    Deannouncement::NoHostProvided__LocalAddress();
}
/* 8. lbos is OFF - return eLBOS_Off                                         */
BOOST_AUTO_TEST_CASE(Deannouncement__LBOSOff__ReturnKLBOSOff)
{
    Deannouncement::LBOSOff__ReturnKLBOSOff();
}
/* 9. Trying to deannounce non-existent service - return eLBOS_NotFound      */
BOOST_AUTO_TEST_CASE(Deannouncement__NotExists__ReturnKLBOSNotFound)
{
    Deannouncement::NotExists__ReturnKLBOSNotFound();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( DeannounceAll )/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(DeannounceAll__AllDeannounced__NoSavedLeft)
{
    DeannouncementAll::AllDeannounced__NoSavedLeft();
}
BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceTest_CXX )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*  1. Successfully announced: return SUCCESS
 *  2. Successfully announced: SLBOS_AnnounceHandle deannounce_handle 
 *     contains info needed to later deannounce announced server
 *  3. Successfully announced: char* lbos_answer contains answer of lbos
 *  4. Successfully announced: information about announcement is saved to 
 *     hidden lbos mapper's storage
 *  5. Could not find lbos: return NO_LBOS
 *  6. Could not find lbos: char* lbos_answer is set to NULL
 *  7. Could not find lbos: SLBOS_AnnounceHandle deannounce_handle is set to
 *     NULL
 *  8. lbos returned error: return LBOS_ERROR
 *  9. lbos returned error: char* lbos_answer contains answer of lbos
 * 10. lbos returned error: SLBOS_AnnounceHandle deannounce_handle is set to 
 *     NULL
 * 11. Server announced again (service name, IP and port coincide) and 
 *     announcement in the same zone, replace old info about announced 
 *     server in internal storage with new one.
 * 12. Server announced again and trying to announce in another 
 *     zone - return MULTIZONE_ANNOUNCE_PROHIBITED
 * 13. Was passed incorrect healthcheck URL (NULL or empty not starting with 
 *     "http(s)://"): do not announce and return INVALID_ARGS
 * 14. Was passed incorrect port (zero): do not announce and return 
 *     INVALID_ARGS
 * 15. Was passed incorrect version(NULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 16. Was passed incorrect service nameNULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 17. Real-life test: after announcement server should be visible to 
 *     resolve
 * 18. If was passed "0.0.0.0" as IP, should replace it with local IP or 
 *     hostname
 * 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host 
 *     IP: do not announce and return DNS_RESOLVE_ERROR
 * 20. lbos is OFF - return eLBOS_Off                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   */

/*  1. Successfully announced : return SUCCESS                               */
BOOST_AUTO_TEST_CASE(Announcement_CXX__AllOK__ReturnSuccess)
{
    Announcement_CXX::AllOK__ReturnSuccess();
}

/*  4. Successfully announced: information about announcement is saved to
 *     hidden lbos mapper's storage                                          */
BOOST_AUTO_TEST_CASE(Announcement_CXX__AllOK__AnnouncedServerSaved)
{
    Announcement_CXX::AllOK__AnnouncedServerSaved();
}

/*  5. Could not find lbos: return NO_LBOS                                   */
BOOST_AUTO_TEST_CASE(Announcement_CXX__NoLBOS__ThrowNoLBOSAndNotFind)
{
    Announcement_CXX::NoLBOS__ThrowNoLBOSAndNotFind();
}

/*  6. Could not find lbos : char* lbos_answer is set to NULL                */
BOOST_AUTO_TEST_CASE(Announcement_CXX__NoLBOS__LBOSAnswerNull)
{
    Announcement_CXX::NoLBOS__LBOSAnswerNull();
}

/*  8. lbos returned error: return eLBOS_ServerError                         */
BOOST_AUTO_TEST_CASE(Announcement_CXX__LBOSError__ThrowServerError)
{
    Announcement_CXX::LBOSError__ThrowServerError();
}

/*  9. lbos returned error : char* lbos_answer contains answer of lbos       */
BOOST_AUTO_TEST_CASE(Announcement_CXX__LBOSError__LBOSAnswerProvided)
{
    Announcement_CXX::LBOSError__LBOSAnswerProvided();
}

/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
BOOST_AUTO_TEST_CASE(
             Announcement_CXX__AlreadyAnnouncedInTheSameZone__ReplaceInStorage)
{
    Announcement_CXX::AlreadyAnnouncedInTheSameZone__ReplaceInStorage();
}

/* 12. Trying to announce in another domain - do nothing                     */
BOOST_AUTO_TEST_CASE(
        Announcement_CXX__AnotherRegion__NoAnnounce)
{
    Announcement_CXX::AnotherRegion__NoAnnounce();
}
/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
BOOST_AUTO_TEST_CASE(Announcement_CXX__IncorrectURL__ThrowInvalidArgs)
{
    Announcement_CXX::IncorrectURL__ThrowInvalidArgs();
}
/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
BOOST_AUTO_TEST_CASE(Announcement_CXX__IncorrectPort__ThrowInvalidArgs)
{
    Announcement_CXX::IncorrectPort__ThrowInvalidArgs();
}
/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement_CXX__IncorrectVersion__ThrowInvalidArgs)
{
    Announcement_CXX::IncorrectVersion__ThrowInvalidArgs();
}
/* 16. Was passed incorrect service name (NULL or empty): do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement_CXX__IncorrectServiceName__ThrowInvalidArgs)
{
    Announcement_CXX::IncorrectServiceName__ThrowInvalidArgs();
}
/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
BOOST_AUTO_TEST_CASE(Announcement_CXX__RealLife__VisibleAfterAnnounce)
{
    Announcement_CXX::RealLife__VisibleAfterAnnounce();
}
/* 18. If was passed "0.0.0.0" as IP, should replace it with local IP or
 *     hostname                                                              */
BOOST_AUTO_TEST_CASE(Announcement_CXX__IP0000__DoNotReplace)
{
    Announcement_CXX::IP0000__DoNotReplace();
}
/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP : do not announce and return DNS_RESOLVE_ERROR                     */
BOOST_AUTO_TEST_CASE(Announcement_CXX__ResolveLocalIPError__AllOK)
{
    Announcement_CXX::ResolveLocalIPError__AllOK();
}
/* 20. lbos is OFF - return eLBOS_Off                                        */
BOOST_AUTO_TEST_CASE(Announcement_CXX__LBOSOff__ThrowKLBOSOff)
{
    Announcement_CXX::LBOSOff__ThrowKLBOSOff();
}
/*21. Announced successfully, but LBOS return corrupted answer -
      return SERVER_ERROR                                                    */
BOOST_AUTO_TEST_CASE(
                    Announcement_CXX__LBOSAnnounceCorruptOutput__ThrowServerError)
{
    Announcement_CXX::LBOSAnnounceCorruptOutput__ThrowServerError();
}
/*22. Trying to announce server and providing dead healthcheck URL - 
      throw e_NotFound                                                  */
BOOST_AUTO_TEST_CASE(Announcement_CXX__HealthcheckDead__ThrowE_NotFound)
{
    Announcement_CXX::HealthcheckDead__ThrowE_NotFound();
}
/*23. Trying to announce server and providing dead healthcheck URL -
      server should not be announced                                         */
BOOST_AUTO_TEST_CASE(Announcement_CXX__HealthcheckDead__AnnouncementOK)
{
    Announcement_CXX::HealthcheckDead__AnnouncementOK();
}
BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceRegistryTest_CXX )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*  1.  All parameters good (Custom section has all parameters correct in 
 *      config) - return eLBOS_Success
 *  2.  Custom section has nothing in config - return eLBOS_InvalidArgs
 *  3.  Section empty or NULL (should use default section and return 
 *      eLBOS_Success)
 *  4.  Service is empty or NULL - return eLBOS_InvalidArgs
 *  5.  Version is empty or NULL - return eLBOS_InvalidArgs
 *  6.  port is empty or NULL - return eLBOS_InvalidArgs
 *  7.  port is out of range - return eLBOS_InvalidArgs
 *  8.  port contains letters - return eLBOS_InvalidArgs
 *  9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs
 *  10. healthcheck does not start with http:// or https:// - return 
 *      eLBOS_InvalidArgs                                                    
 *  11. Trying to announce server and providing dead healthcheck URL -
 *      return eLBOS_NotFound                                                */

/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success                                       */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry_CXX__ParamsGood__ReturnSuccess) 
{
    return AnnouncementRegistry_CXX::ParamsGood__ReturnSuccess();
}
/*  2.  Custom section has nothing in config - return eLBOS_InvalidArgs      */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__CustomSectionNoVars__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::CustomSectionNoVars__ThrowInvalidArgs();
}
/*  3.  Section empty or NULL (should use default section and return 
        eLBOS_Success)                                                       */
BOOST_AUTO_TEST_CASE(
AnnouncementRegistry_CXX__CustomSectionEmptyOrNullAndSectionIsOk__AllOK
                                                                              )
{
    return AnnouncementRegistry_CXX::
                       CustomSectionEmptyOrNullAndSectionIsOk__AllOK();
}
/*  4.  Service is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__ServiceEmptyOrNull__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::ServiceEmptyOrNull__ThrowInvalidArgs();
}
/*  5.  Version is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__VersionEmptyOrNull__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::VersionEmptyOrNull__ThrowInvalidArgs();
}
/*  6.  port is empty or NULL - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__PortEmptyOrNull__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::PortEmptyOrNull__ThrowInvalidArgs();
}
/*  7.  port is out of range - return eLBOS_InvalidArgs                      */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__PortOutOfRange__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::PortOutOfRange__ThrowInvalidArgs();
}
/*  8.  port contains letters - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(
              AnnouncementRegistry_CXX__PortContainsLetters__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::PortContainsLetters__ThrowInvalidArgs();
}
/*  9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs              */
BOOST_AUTO_TEST_CASE(
          AnnouncementRegistry_CXX__HealthchecktEmptyOrNull__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::
            HealthchecktEmptyOrNull__ThrowInvalidArgs();
}
/*  10. healthcheck does not start with http:// or https:// - return         
        eLBOS_InvalidArgs                                                    */  
BOOST_AUTO_TEST_CASE(
  AnnouncementRegistry_CXX__HealthcheckDoesNotStartWithHttp__ThrowInvalidArgs)
{
    return AnnouncementRegistry_CXX::
                          HealthcheckDoesNotStartWithHttp__ThrowInvalidArgs();
}      
/*  11. Trying to announce server and providing dead healthcheck URL -
        return eLBOS_NotFound                                                */
BOOST_AUTO_TEST_CASE(
    AnnouncementRegistry_CXX__HealthcheckDead__ThrowE_NotFound)
{
    return AnnouncementRegistry_CXX::HealthcheckDead__ThrowE_NotFound();
}      
BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Deannounce_CXX )////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Successfully deannounced : return 1
 * 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it
 * 3. Could not connect to provided lbos : fail and return 0
 * 4. Successfully connected to lbos, but deannounce returned error : return 0
 * 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                            
 * 6. Another domain - do not deannounce 
 * 7. Deannounce without IP specified - deannounce from local host 
 * 8. lbos is OFF - return eLBOS_Off                                         */
 /* 1. Successfully deannounced: return 1                                    */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__Deannounced__Return1)
{
    /* Here we specifiy port (specify that we do not intend to use specific 
     * port), because this function is used inside one another test 
     * (Deannouncement_CXX__RealLife__InvisibleAfterDeannounce)    */
    Deannouncement_CXX::Deannounced__Return1(0);
}
/* 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it                                                              */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__Deannounced__AnnouncedServerRemoved)
{
    Deannouncement_CXX::Deannounced__AnnouncedServerRemoved();
}
/* 3. Could not connect to provided lbos : fail and return 0                 */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__NoLBOS__Return0)
{
    Deannouncement_CXX::NoLBOS__Return0();
}
/* 4. Successfully connected to lbos, but deannounce returned error: 
 *    return 0                                                               */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__LBOSExistsDeannounceError__Return0)
{
    Deannouncement_CXX::LBOSExistsDeannounceError__Return0();
}
/* 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                             */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__RealLife__InvisibleAfterDeannounce)
{
    Deannouncement_CXX::RealLife__InvisibleAfterDeannounce();
}
/*6. If trying to deannounce in another domain - do not deannounce           */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__AnotherDomain__DoNothing)
{
    Deannouncement_CXX::AnotherDomain__DoNothing();
}
/* 7. Deannounce without IP specified - deannounce from local host           */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__NoHostProvided__LocalAddress)
{
    Deannouncement_CXX::NoHostProvided__LocalAddress();
}
/* 8. lbos is OFF - return eLBOS_Off                                         */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__LBOSOff__ThrowKLBOSOff)
{
    Deannouncement_CXX::LBOSOff__ThrowKLBOSOff();
}
/* 9. Trying to deannounce non-existent service - throw e_NotFound           */
BOOST_AUTO_TEST_CASE(Deannouncement_CXX__NotExists__ThrowE_NotFound)
{
    Deannouncement_CXX::NotExists__ThrowE_NotFound();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( DeannounceAll_CXX )/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(DeannounceAll_CXX__AllDeannounced__NoSavedLeft)
{
    DeannouncementAll_CXX::AllDeannounced__NoSavedLeft();
}
BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Stability )/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Just reset
 * 2. Full cycle: open mapper, get all servers, close iterator, repeat.
 */
/*  A simple stability test. We open iterator, and then, not closing it,
 * we get all servers and reset iterator and start getting all servers again
 */
BOOST_AUTO_TEST_CASE(Stability__GetNext_Reset__ShouldNotCrash)
{
    Stability::GetNext_Reset__ShouldNotCrash();
}

BOOST_AUTO_TEST_CASE(Stability__FullCycle__ShouldNotCrash)
{
    Stability::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Performance )///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 *  Perform full cycle: open mapper, get all servers, close iterator, repeat.
 * Test lasts 60 seconds and measures min, max and avg performance.
 */
BOOST_AUTO_TEST_CASE(Performance__FullCycle__ShouldNotCrash)
{
    Performance::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( MultiThreading )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 *  Run all tests at once in concurrent threads and expect all tests to run
 * with the same results, except those in which mocks are not thread safe
 */
BOOST_AUTO_TEST_CASE(MultiThreading_test1)
{
#ifdef NCBI_THREADS
    MultiThreading::TryMultiThread();
#endif /* NCBI_THREADS */
}

BOOST_AUTO_TEST_SUITE_END()


/* Moved to be last because not very important and very long                 */
BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash)
{
    Resolve_via_LBOS::FakeErrorInput__ShouldNotCrash();
}
