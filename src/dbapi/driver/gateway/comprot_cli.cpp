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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   Conveniency shortcut functions for CompactProtocol client,
 *   returning bool/int/void and accepting 0 or 1 parameters.
 *
 */

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/gateway/comprot_cli.hpp>

CSSSConnection* conn=NULL;

USING_NCBI_SCOPE;

void comprot_errmsg()
{
  const char* pchAnswer;
  unsigned sz;
  IGate* pGate = conn->getProtocol();

  if( pGate->get_input_arg("Error", &pchAnswer, &sz) == IGate::eGood ) {
    conn->setErrMsg(pchAnswer);

    cerr << "comprot_errmsg=" << pchAnswer << "\n";
  }
  else {
    conn->setErrMsg("");
    cerr << "comprot_errmsg - no Error message from server\n";
  }

  int exceptionType;
  if( pGate->get_input_arg("exception", &exceptionType) == IGate::eGood ) {
    // Re-throw the exception caught on the server
    int severity = (int)eDB_Unknown; // EDB_Severity::
    pGate->get_input_arg("severity", &severity);
    int code = 0;
    pGate->get_input_arg("code"    , &code    );
    const char* from="";
    pGate->get_input_arg("from"    , &from    );
    const char* msg="";
    pGate->get_input_arg("msg"     , &msg     );

    switch( (CDB_Exception::EType)exceptionType ) {
      case CDB_Exception::eRPC:
      {
        const char* proc="";
        pGate->get_input_arg("proc", &proc);
        int line = 0;
        pGate->get_input_arg("line", &line);

        throw CDB_RPCEx( (EDB_Severity)severity,code,from,msg,  proc,line );
      }
      case CDB_Exception::eSQL:
      {
        const char* state="";
        pGate->get_input_arg("state", &state);
        int line = 0;
        pGate->get_input_arg("line" , &line );

        throw CDB_SQLEx( (EDB_Severity)severity,code,from,msg,  state,line );
      }
      case CDB_Exception::eDS      : throw CDB_DSEx      ( (EDB_Severity)severity, code, from, msg );
      case CDB_Exception::eDeadlock: throw CDB_DeadlockEx( from, msg );
      case CDB_Exception::eTimeout : throw CDB_TimeoutEx ( code, from, msg );
      case CDB_Exception::eClient  : throw CDB_ClientEx  ( (EDB_Severity)severity, code, from, msg );
      default:
        cerr<< "Unrecognized exception in comprot_errmsg(): \n  type=" << exceptionType;
        cerr << " code=" << code << " severity=" << severity << "\n";
        cerr<< "  from=" << from << "\n";
        cerr<< "  msg=" << msg << "\n";
        //throw CDB_Exception( (CDB_Exception::EType)exceptionType,  (EDB_Severity)severity,code,from,msg );
    }
  }
}


bool comprot_bool( const char *procName, int object )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "bool " << procName << " object=" << object << "\n";

  pGate->send_data();

  int nOk;
  if (pGate->get_input_arg("Ok", &nOk) != IGate::eGood) {
    comprot_errmsg();
    return false;
  }

  // cerr << "  result=" << nOk << "\n";

  return nOk;
}


int comprot_int( const char *procName, int object )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "int " << procName << " object=" << object << "\n";

  pGate->send_data();

  int res;
  if (pGate->get_input_arg("result", &res) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }

  // cerr << "  result=" << res << "\n";

  return res;
}


void comprot_void( const char *procName, int object )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "void " << procName << " object=" << object << "\n";

  pGate->send_data();
}

char* comprot_chars( const char *procName, int object, char* buf, int len )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "chars1 " << procName << " object=" << object << "\n";
  pGate->send_data();

  if (pGate->get_input_arg("result", buf, len) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }

  // cerr << "  result=" << buf << "\n";

  return buf;
}
