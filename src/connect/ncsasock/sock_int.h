/*
 * BSD-style socket emulation library for the Mac
 * Original author: Tom Milligan
 * Current author: Charlie Reiman - creiman@ncsa.uiuc.edu
 *
 * This source file is placed in the public domian.
 * Any resemblance to NCSA Telnet, living or dead, is purely coincidental.
 *
 *      National Center for Supercomputing Applications
 *      152 Computing Applications Building
 *      605 E. Springfield Ave.
 *      Champaign, IL  61820
 */

/*
 * Internal prototypes for the socket library.
 * There is duplication between socket.ext.h and socket.int.h, but
 * there are too many complications in combining them.
 */

typedef enum spin_msg
	{
	SP_MISC,			/* some weird thing */
	SP_SELECT,			/* in a select call */
	SP_NAME,			/* getting a host by name */
	SP_ADDR,			/* getting a host by address */
	SP_TCP_READ,		/* TCP read call */
	SP_TCP_WRITE,		/* TCP write call */
	SP_UDP_READ,		/* UDP read call */
	SP_UDP_WRITE,		/* UDP write call */
	SP_SLEEP			/* sleeping */
	} spin_msg;
	

/* spin routine prototype, doesn't work. Sigh. */

extern int (*spinroutine)(int mesg,int param);

/* tcpglue.c */

OSErr xPBControlSync(TCPiopb *pb);
OSErr xPBControl(TCPiopb *pb, TCPIOCompletionProc completion);


OSErr xOpenDriver(void);
OSErr xTCPCreate(int buflen, TCPNotifyProc notify, TCPiopb *pb);
OSErr xTCPPassiveOpen(TCPiopb *pb, short port, TCPIOCompletionProc completion);
OSErr xTCPActiveOpen(TCPiopb *pb, short port, long rhost, short rport, TCPIOCompletionProc completion);
OSErr xTCPRcv(TCPiopb *pb, char *buf, int buflen, int timeout, TCPIOCompletionProc completion);
OSErr xTCPNoCopyRcv(TCPiopb *,rdsEntry *,int,int,TCPIOCompletionProc);
OSErr xTCPBufReturn(TCPiopb *pb,rdsEntry *rds,TCPIOCompletionProc completion);
OSErr xTCPSend(TCPiopb *pb, wdsEntry *wds, Boolean push, Boolean urgent, TCPIOCompletionProc completion);
OSErr xTCPClose(TCPiopb *pb,TCPIOCompletionProc completion);
OSErr xTCPAbort(TCPiopb *pb);
OSErr xTCPRelease(TCPiopb *pb);
int xTCPBytesUnread(SocketPtr sp);
int xTCPBytesWriteable(SocketPtr sp);
int xTCPWriteBytesLeft(SocketPtr sp);
int xTCPState(TCPiopb *pb);
OSErr xUDPCreate(SocketPtr sp,int buflen,ip_port port);
OSErr xUDPRead(SocketPtr sp,UDPIOCompletionProc completion);
OSErr xUDPBfrReturn(SocketPtr sp);
OSErr xUDPWrite(SocketPtr sp,ip_addr host,ip_port port,miniwds *wds,
		UDPIOCompletionProc completion);
OSErr xUDPRelease(SocketPtr sp);
ip_addr xIPAddr(void);
long xNetMask(void);
unsigned short xMaxMTU(void);


/* socket.tcp.c */
pascal void sock_tcp_notify(
	StreamPtr tcpStream,
	unsigned short eventCode,
	Ptr userDataPtr,
	unsigned short terminReason,
	struct ICMPReport *icmpMsg);
int sock_tcp_new_stream(SocketPtr sp);
int sock_tcp_connect(SocketPtr sp, struct sockaddr_in *addr);
int sock_tcp_listen(SocketPtr sp);
int sock_tcp_accept(SocketPtr sp, struct sockaddr_in *from, Int4 *fromlen);
int sock_tcp_accept_once(SocketPtr sp, struct sockaddr_in *from, Int4 *fromlen);
int sock_tcp_recv(SocketPtr sp, char *buffer, int buflen, int flags);
int sock_tcp_can_read(SocketPtr sp);
int sock_tcp_send(SocketPtr sp, char *buffer, int count, int flags);
int sock_tcp_can_write(SocketPtr sp);
int sock_tcp_close(SocketPtr sp);

/* socket.udp.c */

int sock_udp_new_stream(SocketPtr sp);
int sock_udp_connect(SocketPtr sp,struct sockaddr_in *addr);
int sock_udp_recv(SocketPtr sp, char *buffer, int buflen, int flags, struct sockaddr_in *from, int *fromlen);
int sock_udp_can_recv(SocketPtr sp);
int sock_udp_send(SocketPtr sp, struct sockaddr_in *to, char *buffer, int count, int flags);
int sock_udp_can_send(SocketPtr sp);
int sock_udp_close(SocketPtr sp);

/* socket.util.c */

int sock_init(void);
void sock_close_all(void);
int sock_free_fd(int f);
void sock_dup_fd(int s,int s1);
void sock_clear_fd(int s);
void sock_init_fd(int s);
int sock_err(int err_code);
void sock_copy_addr(void *from, void *to, Int4 *tolen);
void sock_dump(void);
void sock_print(char *title, SocketPtr sp);
StreamHashEntPtr sock_find_shep(StreamPtr);
StreamHashEntPtr sock_new_shep(StreamPtr);
void *sock_fetch_pb(SocketPtr);
