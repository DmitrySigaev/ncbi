#ifndef CONNECT___NCBI_LBOS__HPP
#define CONNECT___NCBI_LBOS__HPP
/*
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
* Authors:  Dmitriy Elisov
* @file
* File Description:
*   A client for service discovery API based on LBOS. 
*   LBOS is a client for ZooKeeper cloud-based DB. 
*   LBOS allows to announce, deannounce and resolve services.
*/

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XNCBI_EXPORT LBOS
{
public:
    /** Announce server.
    *
    * @param [in] service
    *  Name of service as it will appear in ZK. For services this means that the
    *  name should start with '/'.
    * @param [in] version
    *  Any non-null non-empty string that will help to identify the version
    *  of service. A good idea is to use [semantic versioning]
    *  (http://semver.org/) like "4.7.2"
    * @param [in] host
    *  Optional parameter. Set to "" (empty string) to use host from 
    *  healthcheck_url to make sure that server and healthcheck actually reside 
    *  on the same machine (it can be important when you specify hostname that 
    *  can be resolved to multiple IPs, and LBOS ends up registering server 
    *  on one host and healthcheck on another)
    * @param [in] port
    *  Port for the service. Can differ from healthcheck port.
    * @param [in] healthcheck_url
    *  Full absolute URL starting with "http://" or "https://". Must include 
    *  hostname or IP (and port, if necessary)
    * @note
    *  If you want to announce a server that is on the same machine that
    *  announces it (i.e., if server announces itself), you can write
    *  "0.0.0.0" for IP in both "host" and "healthcheck_url". You still have to
    *  provide port, even if you write "0.0.0.0".
    * @return
    *  Returns nothing if announcement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa AnnounceFromRegistry(), Deannounce(), DeannounceAll(), CLBOSException
    */
    static void 
        Announce(const string&   service,
                 const string&   version,
                 const string&   host,
                 unsigned short  port,
                 const string&   healthcheck_url);
                                  

   /** Modification of Announce() that gets all the needed parameters from 
    * registry.
    *
    * @param [in] registry_section
    *  Name of section in registry where to look for 
    *  announcement parameters. Please check documentation for Announce() to
    *  to see requirements for the arguments.
    *  Parameters are:
    *  service, version, host, port, health
    *  Example:
    *  --------------
    *  [LBOS_ANNOUNCEMENT]
    *  service=MYSERVICE
    *  version=1.0.0
    *  host=0.0.0.0
    *  port=8080
    *  health=http://0.0.0.0:8080/health
    *
    * @return
    *  Returns nothing if announcement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa Announce(), Deannounce(), DeannounceAll(), CLBOSException
    */
    static void AnnounceFromRegistry(string  registry_section);


    /** Deannounce service.
    * @param [in] service
    *  Name of service to be de-announced.
    * @param [in] version
    *  Version of service to be de-announced.
    * @param [in] host
    *  IP or hostname of service to be de-announced. Provide empty string
    *  (NOT "0.0.0.0") to use local host address.
    * @param [in] port
    *  Port of service to be de-announced.
    * @return
    *  Returns nothing if de-announcement was successful. Otherwise, throws an
    *  exception.
    * @exception CLBOSException
    * @sa Announce(), DeannounceAll(), CLBOSException
    */
    static void Deannounce(const string&  service,
                           const string&  version,
                           const string&  host,
                           unsigned short port);


    /** Deannounce all servers that were announced during runtime.
    * @note
    *  There is no guarantee that all servers were de-announced successfully
    *  after this function returned. There is a guarantee that de-announcement
    *  request was sent to LBOS for each server that was announced by the 
    *  running application during runtime.
    * @return
    *  Returns nothing, never throws.
    * @sa Announce(), Deannounce()
    */
    static void DeannounceAll(void);


    /** Show default version of a service in ZooKeeper configuration. 
    *  Does no change anything in ZooKeeper or LBOS configuration.
    * @param service[in]
    *  Name of service for which to ask default version.
    * @return
    *  Current default version.
    * @exception CLBOSException
    *  If no record for the service was found, the exception will contain 
    *  e_LBOSNotFound code.
    */
    static
    string ServiceVersionGet(const string&  service,
                             bool* existed = NULL);


    /** Set default version for a service in ZooKeeper configuration.
    * @param[in] service
    *  Name of service for which to change the default version.
    * @param new_version[out]
    *  Version that will be used by default for specified service.
    * @return
    *  Version before request.
    */
    static
    string ServiceVersionSet(const string&  service,
                             const string&  new_version,
                             bool* existed = NULL);


    /** Remove service from ZooKeeper configuration. Default
    *   version will be empty.
    * @param[in] service
    *  Name of service to delete from ZooKeeper configuration.
    * @return
    *  Version before request 
    */
    static
    string ServiceVersionDelete(const string&  service,
                                bool* existed = NULL);
};


/** CLBOSException is thrown if a request to LBOS fails for any
 * reason. CLBOSException has overloaded "what()" method that returns
 * message from LBOS (if there is one), which should contain status code and 
 * status message. If announcement failed not because of LBOS, but because of 
 * bad arguments, memory error, etc., status code is a non-HTTP status code. 
 * To get its meaning you can call CLBOSException::GetErrCodeString(void) to 
 * see human language description of the exception.
 */
class NCBI_XNCBI_EXPORT CLBOSException : public CException
{
public:
    typedef int TErrCode;
    enum EErrCode {
        e_LBOSNoLBOS          = 0,  /**< LBOS was not found                  */
        e_LBOSDNSResolveError = 1,  /**< Local address not resolved          */
        e_LBOSInvalidArgs     = 2,  /**< Arguments not valid                 */
        e_LBOSNotFound        = 3,  /**< For de-announcement only. Did not
                                         find such server to deannounce      */
        e_LBOSOff             = 4,  /**< LBOS client is off for any of the 
                                         two reasons: either it is not 
                                         enabled in registry, or no LBOS 
                                         working LBOS instance was found at
                                         initialization                      */
        e_LBOSMemAllocError   = 5,  /**< Memory allocation error             */
        e_LBOSCorruptOutput   = 6,  /**< LBOS returned unexpected output     */
        e_LBOSBadRequest      = 7,  /**< LBOS returned "400 Bad Request"     */
        e_LBOSUnknown         = 8,  /**< No information about this error
                                         code meaning                        */
        e_LBOSServerError     = 9   /**< LBOS returned "500 Internal Server 
                                         Error"                              */
    };

    CLBOSException(const CDiagCompileInfo& info,
                   const CException* prev_exception, EErrCode err_code,
                   const string& message, unsigned short status_code,
                   EDiagSev severity = eDiag_Error);

    CLBOSException(const CDiagCompileInfo& info, 
                   const CException* prev_exception, 
                   const CExceptionArgs<EErrCode>& args, 
                   const string& message,
                   unsigned short status_code);
    virtual ~CLBOSException(void) throw();
    
    /** Get original status code and status message from LBOS in a string */
    virtual const char* what() const throw();

    /** Translate from the error code value to its string representation
    *  (only for internal errors; real LBOS
    *  errors will not be processed) */
    virtual const char* GetErrCodeString(void) const;

    /** Translate from numerical HTTP status code to LBOS-specific
     * error code */
    static EErrCode s_HTTPCodeToEnum(unsigned short http_code);

    unsigned short GetStatusCode(void) const;
    virtual const char* GetType(void) const;
    TErrCode GetErrCode(void) const;
    NCBI_EXCEPTION_DEFAULT_THROW(CLBOSException)
protected:
    CLBOSException(void);
    virtual const CException* x_Clone(void) const;
private:
    unsigned short m_StatusCode;
    string m_Message;
};

END_NCBI_SCOPE

#endif /* CONNECT___NCBI_LBOS__HPP */
