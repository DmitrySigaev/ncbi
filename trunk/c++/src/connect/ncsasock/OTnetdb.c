/*
 * BSD-style socket emulation library for the Mac
 * Original author: Tom Milligan
 * Current author: Charlie Reiman - creiman@ncsa.uiuc.edu
 *
 * This source file is placed in the public domain.
 * Any resemblance to NCSA Telnet, living or dead, is purely coincidental.
 *
 *      National Center for Supercomputing Applications
 *      152 Computing Applications Building
 *      605 E. Springfield Ave.
 *      Champaign, IL  61820
 *
 *
 * RCS Modification History:
 * $Log$
 * Revision 6.2  2001/05/17 15:04:37  lavr
 * Typos corrected
 *
 * Revision 6.1  2000/03/20 21:50:35  kans
 * initial checkin - initial work on OpenTransport (Churchill)
 *
 * Revision 6.1  1999/11/17 20:52:50  kans
 * changes to allow compilation under c++
 *
 * Revision 6.0  1997/08/25 18:37:31  madden
 * Revision changed to 6.0
 *
 * Revision 4.1  1995/08/01 16:17:20  kans
 * New gethostbyname and gethostbyaddr contributed by Doug Corarito
 *
 * Revision 4.0  1995/07/26  13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.3  1995/05/17  17:56:47  epstein
 * add RCS log revision history
 *
 */

/*
 *	Internet name routines that every good unix program uses...
 *
 *		gethostbyname
 *		gethostbyaddr
 *      gethostid
 *		gethostname
 *		getdomainname
 *		inet_addr
 *		inet_ntoa
 *		getservbyname
 *		getprotobyname
 */
 
#include <Stdio.h>
#include <String.h>
#include <Types.h>
#include <Resources.h>
#include <Errors.h>
#include <OSUtils.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>	// for TCP/IP
#include "ncbiOT.h"

#include <s_types.h>
#include <netdb.h>
#include <neti_in.h>
#include <a_inet.h>

#include <s_socket.h>
#include <s_time.h>
#include <neterrno.h>
#include "sock_str.h"
#include "sock_int.h"
#include <sock_ext.h>

#include <Ctype.h>
#include <a_namesr.h>
#include <s_param.h>

//#define NETDB_DEBUG 5

extern SocketPtr sockets;
extern SpinFn spinroutine;

static int h_errno;

/*
 *   Gethostbyname and gethostbyaddr each return a pointer to an
 *   object with the following structure describing an Internet
 *   host referenced by name or by address, respectively. This
 *   structure contains the information obtained from the OpenTransport
 *   name server.
 *
 *   struct    hostent 
 *   {
 *        char *h_name;
 *        char **h_aliases;
 *        int  h_addrtype;
 *        int  h_length;
 *        char **h_addr_list;
 *   };
 *   #define   h_addr  h_addr_list[0]
 *
 *   The members of this structure are:
 *
 *   h_name       Official name of the host.
 *
 *   h_aliases    A zero terminated array of alternate names for the host.
 *
 *   h_addrtype   The type of address being  returned; always AF_INET.
 *
 *   h_length     The length, in bytes, of the address.
 *
 *   h_addr_list  A zero terminated array of network addresses for the host.
 *
 *   Error return status from gethostbyname and gethostbyaddr  is
 *   indicated by return of a null pointer.  The external integer
 *   h_errno may then  be checked  to  see  whether  this  is  a
 *   temporary  failure  or  an  invalid  or  unknown  host.  The
 *   routine herror  can  be  used  to  print  an error  message
 *   describing the failure.  If its argument string is non-NULL,
 *   it is printed, followed by a colon and a space.   The  error
 *   message is printed with a trailing newline.
 *
 *   h_errno can have the following values:
 *
 *     HOST_NOT_FOUND  No such host is known.
 *
 *     TRY_AGAIN	This is usually a temporary error and
 *					means   that  the  local  server  did  not
 *					receive a response from  an  authoritative
 *					server.   A  retry at some later time may
 *					succeed.
 *
 *     NO_RECOVERY	Some unexpected server failure was encountered.
 *	 				This is a non-recoverable error.
 *
 *     NO_DATA		The requested name is valid but  does  not
 *					have   an IP  address;  this  is not  a
 *					temporary error. This means that the  name
 *					is known  to the name server but there is
 *					no address  associated  with  this  name.
 *					Another type of request to the name server
 *					using this domain name will result in  an
 *					answer;  for example, a mail-forwarder may
 *					be registered for this domain.
 *					(NOT GENERATED BY THIS IMPLEMENTATION)
 */

static char	cname[255];
static struct InetHostInfo hinfo;

#define MAXALIASES 2
static char	aliases[MAXALIASES+1][kMaxHostNameLen + 1];
static char* aliasPtrs[MAXALIASES+1] = {NULL};
//static char* aliasPtrs[MAXALIASES+1] = {&aliases[0][0],&aliases[1][0],&aliases[2][0]};
static InetHost* addrPtrs[kMaxHostAddrs+1] = {0};


static struct hostent unixHost = 
{
	cname,
	aliasPtrs,
	AF_INET,
	sizeof(UInt32),
	(char **) addrPtrs
};


/* Gethostbyname
 *
 *	favorite UNIX function implemented on top of OpenTransport
 */

struct hostent *
gethostbyname(const char *name)
{
	extern EndpointRef gDNRep;
	OSStatus	theErr;
	int i, l;

	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			// Need to add functionality to set our errno
			return (NULL);
		}
	}

	// Call OT to resolve the address...
	theErr = OTInetStringToAddress( gDNRep, (char*) name, &hinfo);

	// map possible OT errors into the 4 specified by bsd...
	//theErr = OTErrorToBSDError( theErr);

	if( theErr != kOTNoError){
		// set errno
		return( NULL);
	}

	// sometimes the hostname returned has a trailing dot?? (nuke it)
	l = strlen( hinfo.name);
	if( hinfo.name[l-1] == '.'){
		hinfo.name[l-1] = '\0';
	}

#if NETDB_DEBUG >= 5
	printf("gethostbyname: name '%s' returned the following addresses:\n", name);
	for( i = 0; i < kMaxHostAddrs; i++){
		if( hinfo.addrs[i] == 0) break;
		printf("%08x\n", hinfo.addrs[i]);
	}
#endif

	// copy the official hostname to the return struct and first alias
	strcpy( unixHost.h_name, hinfo.name);

	// copy the name passed in as the first alias
	strncpy( aliases[0], name, kMaxHostNameLen);
	aliases[0][kMaxHostNameLen] = '\0';

	// if the answer is different from the query, copy the query as another alias
	theErr = strcmp( name, hinfo.name);
	if( theErr != 0){
		strncpy( aliases[1], hinfo.name, kMaxHostNameLen);
		aliases[1][kMaxHostNameLen] = '\0';
	}

	// This block will not need to be changed if we find a way to determine
	// more aliases than are currently implemented
	for( i = 0; i <= MAXALIASES; i++){
		if( aliases[i][0] != '\0'){
			aliasPtrs[i] = &aliases[i][0];
		} else{
			aliasPtrs[i] = NULL;
		}
	}
	
	// copy all of the returned addresses
	for( i = 0; i < kMaxHostAddrs && hinfo.addrs[i] != 0; i++){
		addrPtrs[i] = &hinfo.addrs[i];
	}

	// make the rest NULL
	for( ; i < kMaxHostAddrs; i++){
		addrPtrs[i] = NULL;
	}
	return( &unixHost);
}


/* gethostbyaddr
 *
 *  Currently only supports IPv4
 */
 
struct hostent *
gethostbyaddr( InetHost *addrP,int len,int type)
{
	extern EndpointRef gDNRep;
	OSStatus	theErr = noErr;
	char	addrString[255];
	int		l;
  
	/* Check that the arguments are appropriate */
	if( len != INADDRSZ || (type != AF_INET && type != AF_UNSPEC)){
		theErr = EAFNOSUPPORT;
		// need to add functionality to set errno...
		return( NULL);
	}
  
	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			// again, set errno
			return (NULL);
		}
	}

#if NETDB_DEBUG >= 5
	printf("gethostbyaddr: Looking up hostname for address 0x%8x\n", *addrP);
#endif

	/*  OpenTransport call to get the address as a string */
	theErr = OTInetAddressToName( gDNRep, *(InetHost*)addrP, addrString);
	if( theErr == noErr){
		// for some reason the names have a trailing "."???  
		// Next couple of lines remove it
		l = strlen( addrString);
		if( addrString[l-1] == '.'){
			addrString[l-1] = '\0';
		}
		// with the full name, use gethostbyname to fill in all the blanks...
		return (gethostbyname(addrString) );
	}
	// else...
	// set errno
	return( NULL);
}

/*
 * gethostid()
 *
 * Get Internet address from the primary enet interface.
 */
 
unsigned long gethostid( void)
{
	OSStatus          theErr = kOTNoError;
	InetInterfaceInfo info;
	extern EndpointRef gDNRep;
  
	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			// set errno
			return 0;
		}
	}
	theErr = OTInetGetInterfaceInfo( &info, kDefaultInetInterface);
	if(theErr != kOTNoError){
		// set errno
		return 0;
	}
	return info.fAddress;
}
 

/*
 * gethostname()
 *
 * Try to get my host name from DNR. If it fails, just return my
 * IP address as ASCII. This is non-standard, but it's a mac,
 * what do you want me to do?
 */

int gethostname( char* machname, size_t buflen)
{
	OSStatus          theErr = kOTNoError;
	InetHost		  addr;
	struct hostent   *host;
	char* firstDot;

	if( (machname == NULL) || (buflen < 31)){
		// set errno
		return( -1);
	}

	addr = gethostid();

	host = gethostbyaddr( &addr, INADDRSZ, AF_INET);
	if(host == NULL){
		//check and or set errno
		return( -1);
	}
  
	firstDot = strchr(host->h_name, '.');
	if(firstDot != NULL && *firstDot == '.'){
		*firstDot = '\0';
	}

	strncpy( machname, host->h_name, buflen);
	machname[buflen-1] = '\0';  // strncpy doesn't always null terminate
    
	if(theErr != kOTNoError)
		return(-1);
	else
		return(0);
}

/* inet_ntoa()
 *
 *	Converts an IP address in 32 bit IPv4 address to a dotted string notation
 *  Function returns 1 on success and 0 on failure.
 */

char *inet_ntoa(struct in_addr addr)
{
	static char addrString[INET_ADDRSTRLEN + 1 /* '\0' */];
	InetHost    host;

	host = addr.s_addr;
	OTInetHostToString(host, addrString);
  
	return (addrString);
}

/*
 * inet_aton() - converts an IP address in dotted string notation to a 32 bit IPv4 address
 *               Function returns 1 on success and 0 on failure.
 */
int inet_aton(const char *str, struct in_addr *addr)
{
	InetHost host;
	OSStatus theError;

	if((str == NULL) || (addr == NULL)){
		return( INET_FAILURE);
	}
  
	theError = OTInetStringToHost((char *) str, &host);
	if(theError != kOTNoError){
		return( INET_FAILURE);
	}
	addr->s_addr = host;
	return(INET_SUCCESS);  
}


/* inet_addr
 *
 *	does the same thing as inet_aton, but returns the value rather than
 *	modifying a parameter that's passed in.
 *		Note: failure is reported by returning INADDR_NONE (255.255.255.255) 
 */

InetHost inet_addr(const char *str)
{
	OSStatus       theError = noErr;
	struct in_addr addr;

	theError = inet_aton(str, &addr);

	if(theError != INET_SUCCESS){
		addr.s_addr = INADDR_NONE;
	}

	return(addr.s_addr);
}


/*
 *	getservbybname()
 *
 *	Real kludgy.  Should at least consult a resource file as the service
 *	database.
 */
typedef struct services {
	char		sv_name[12];
	short		sv_number;
	char		sv_protocol[5];
} services_t, *services_p;

static	struct services	slist[] = 
{ 
	{"echo", 7, "udp"},
	{"discard", 9, "udp"},
	{"time", 37, "udp"},
	{"domain", 53, "udp"},
	{"sunrpc", 111, "udp"},
	{"tftp", 69, "udp"},
	{"biff", 512, "udp"},
	{"who", 513, "udp"},
	{"talk", 517, "udp"},
	{"route", 520, "udp"},
	{"new-rwho", 550, "udp"},
	{"netstat", 15, "tcp"},
	{"ftp-data", 20, "tcp"},
	{"ftp", 21, "tcp"},
	{"telnet", 23, "tcp"},
	{"smtp", 25, "tcp"},
	{"time", 37, "tcp"},
	{"whois", 43, "tcp"},
	{"domain", 53, "tcp"},
	{"hostnames", 101, "tcp"},
	{"nntp", 119, "tcp"},
	{"finger", 79, "tcp"},
	{"uucp-path", 117, "tcp"},
	{"untp", 119, "tcp"},
	{"ntp", 123, "tcp"},
	{"exec", 512, "tcp"},
	{"login", 513, "tcp"},
	{"shell", 514, "tcp"},
	{"printer", 515, "tcp"},
	{"courier", 530, "tcp"},
	{"uucp", 540, "tcp"},
	{"", 0, "" }
};
					 
#define	MAX_SERVENT			10
static 	struct servent		servents[MAX_SERVENT];
static 	int					servent_count=0;

struct servent *
getservbyname (name, proto)
char		*name, *proto;
{
	int				i;
	struct	servent	*se;
	
	if (strcmp (proto, "udp") == 0 || strcmp (proto, "tcp") == 0) 
	{
		for (i=0; slist[i].sv_number != 0; i++)
			if (strcmp (slist[i].sv_name, name) == 0)
				if (strcmp (slist[i].sv_protocol, proto) == 0) 
				{
					se = &servents[servent_count];
					se->s_name = slist[i].sv_name;
					se->s_aliases = NULL;
					se->s_port = slist[i].sv_number;
					se->s_proto = slist[i].sv_protocol;
					servent_count = (servent_count +1) % MAX_SERVENT;
					return (se);
				}
		return (NULL);
	}
	else 
	{
		errno = errno_long = EPROTONOSUPPORT;
		return(NULL);
	}
}

static	char	tcp[] = "tcp";
static	char	udp[] = "udp";
#define	MAX_PROTOENT			10
static 	struct protoent		protoents[MAX_PROTOENT];
static 	int					protoent_count=0;
struct protoent *
getprotobyname (name)
char		*name;
{
	struct protoent *pe;
	
	pe = &protoents[protoent_count];
	if (strcmp (name, "udp") == 0) 
	{
		pe->p_name = udp;
		pe->p_proto = IPPROTO_UDP;
	}
	else if (strcmp (name, "tcp") == 0) 
	{
		pe->p_name = tcp;
		pe->p_proto = IPPROTO_TCP;
	}
	else 
	{	
		errno = errno_long = EPROTONOSUPPORT;
		return(NULL);
	}
	pe->p_aliases = NULL;
	protoent_count = (protoent_count +1) % MAX_PROTOENT;
	return (pe);
}

char *h_errlist[] = 
{
	"Error 0",
	"Unknown host",						/* 1 HOST_NOT_FOUND */
	"Host name lookup failure",			/* 2 TRY_AGAIN */
	"Unknown server error",				/* 3 NO_RECOVERY */
	"No address associated with name",	/* 4 NO_ADDRESS */
};

const int	h_nerr = { sizeof(h_errlist)/sizeof(h_errlist[0]) };

void herror(char *s)
	{
	fprintf(stderr,"%s: ",s);
	if (h_errno < h_nerr)
		fprintf(stderr,h_errlist[h_errno]);
	else
		fprintf(stderr,"error %d",h_errno);
	fprintf(stderr,"\n");
	}

char *herror_str(int theErr) {
	if (theErr > h_nerr )
		return NULL;
	else
		return h_errlist[theErr];
		}
