/*

*** Experimental ****

A Contiki/uIP based net_native implementation for Fuzix.  This runs
as a user-space daemon.


There are two event "threads" - one for the kernel messages, and one for
uIP's messages... these "threads" are round-robin'ed together in main().


issues:
* is this the correct kernel call / data struct for ticks ?
* select() is not used, but should be.

todo:

* deamonize (detach) this proggie. (will we always have setsid() ? )
* UDP local bind
* TCP passive open
* RAW pass protocol number via socket() not connect()
* refactor  RAW with UDP code as they are very similar

*/



#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/drivewire.h>
#include <sys/netdev.h>
#include <sys/net_native.h>

#include "uip.h"
#include "timer.h"
#include "uip_arp.h"
#include "clock.h"
#include "netd.h"
#include "uiplib.h"
#include "device.h"

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

int bfd;   /* fd of data backing file */
int knet;  /* fd of kernel's network inface */
int rc;    /* fd of rc file */
struct sockmsg sm; /* event from kernel iface */
struct netevent ne; /* event to kernel iface */
struct timer periodic_timer, arp_timer;

/* 'links' associate a kernel socket no to a uIP connection pointer
   and hold some useful state.  Fuzix inits a new socket well before
   uIP, so a separate linkage/state struct is needed.
*/
struct link map[NSOCKET];
int freelist[NSOCKET];
int freeptr = 0;


/* print an error message */
void printe( char *mesg )
{
	write(2, mesg, strlen(mesg));
}

/* print an error message an quit w/ error */
void exit_err( char *mesg )
{
	printe( mesg );
	exit(1);
}

/* wrapper around writing to kernel net device */
void ksend( int event )
{
	ne.ret = 0;
	ne.event = event;
	if ( write( knet, &ne, sizeof(ne) ) < 0 )
		exit_err("cannot write kernel net dev\n");
}


/* allocate a connection linkage */
int get_map( void )
{
	int new = freelist[ --freeptr ];
	memset( &map[new], 0, sizeof( map[new] ) );
	return new;
}

/* release a linkage */
void rel_map( int n )
{
	freelist[ freeptr++ ] = n;
}

/* initialize the free map of links */
void init_map( void )
{
	int n;
	for ( n = 0; n<NSOCKET; n++)
		rel_map( n );
}


/* send or resend tcp data */
void send_tcp( struct link *s )
{
	uint32_t base = s->socketn * RINGSIZ * 2 + RINGSIZ;
	if ( ! s->len ) return;
	lseek( bfd, base + s->tstart, SEEK_SET );
	if ( read( bfd, uip_appdata, s->len ) < 0 )
		exit_err("cannot read from backing file\n");
	uip_send( uip_appdata, s->len );
}

/* send udp data */
void send_udp( struct link *s )
{
	/* Send packet to net */
	uint32_t base = s->socketn * RINGSIZ * 2 + RINGSIZ;
	uint16_t len = s->tsize[s->tstart];
	lseek( bfd, base + s->tstart * TXPKTSIZ, SEEK_SET );
	if ( read( bfd, uip_appdata, len ) < 0 )
		exit_err("cannot read from backing file\n");
	uip_udp_send( len );
	if ( ++s->tstart == NSOCKBUF )
		s->tstart = 0;
	/* Send Room event back to kernel */
	ne.socket = s->socketn;
	ne.data = s->tstart;
	ksend( NE_ROOM );
}

/* send raw data FIXME: fold together with send_udp */
void send_raw( struct link *s )
{
	/* Send packet to net */
	uint32_t base = s->socketn * RINGSIZ * 2 + RINGSIZ;
	uint16_t len = s->tsize[s->tstart];
	lseek( bfd, base + s->tstart * TXPKTSIZ, SEEK_SET );
	if ( read( bfd, uip_appdata, len ) < 0 )
		exit_err("cannot read from backing file\n");
	uip_raw_send( len );
	if ( ++s->tstart == NSOCKBUF )
		s->tstart = 0;
	/* Send Room event back to kernel */
	ne.socket = s->socketn;
	ne.data = s->tstart;
	ksend( NE_ROOM );
}

/* return amout of free room for adding data in recv ring buffer */
uint16_t room( struct link *s )
{
	uint16_t t = 0;
	if ( s->rstart > s->rend )
		t = s->rstart - s->rend;
	else
		t = RINGSIZ - (s->rend - s->rstart);
	return t-1;
}

/* return capacity of data in transmit ring buffer */
uint16_t cap( struct link *s )
{
	if ( s->tstart > s->tend )
		return RINGSIZ - (s->tstart - s->tend);
	else
		return s->tend - s->tstart;
}

/* uIP callback for TCP events */
void netd_appcall()
{
	/*
	  printe( "appcall: " );
	  if( uip_aborted() ) printe("aborted.\n");
	  if( uip_acked() )  printe("acked.\n");
	  if( uip_closed() ) printe("closed.\n");
	  if( uip_newdata() ) printe("new data.\n");
	  if( uip_poll() ) printe("polled.\n");
	  if( uip_rexmit() ) printe("rexmit.\n");
	  if( uip_timedout() ) printe("timed out.\n");
	  if( uip_connected() ) printe("connected.\n" );
	*/
	struct link *s = & map[uip_conn->appstate];
	ne.socket = s->socketn;

	/* uIP sends up a connected event for both
	   active and passive opens */
	if ( uip_connected() ){
		int flag = 0;
		/* find see if anything is listening on this port */
		{
			int i;
			for ( i = 0; i< NSOCKET; i++ ){
				if ( map[i].port == uip_conn->lport ){
					/* found a listening port */
					/* FIXME: what am I supposed to send to the
					   listening socket?
					*/
					ne.data = SS_ACCEPTWAIT;
					ksend( NE_EVENT );
					/* make a new link and Associate the new uip_connection
					   with some lcn/socket the kernel readied */
					flag = 1;
					break;
				}
			}
		}
		if ( ! flag ){
			/* Not a listing port so assume a active open */
			ne.data = SS_CONNECTED;
			ksend( NE_NEWSTATE );
		}
	}


	if ( uip_newdata() ){
		/* there new data in the packet buffer */
		uint16_t len = uip_datalen();
		uint16_t l;
		char *ptr = uip_appdata;
		uint32_t base = ne.socket * RINGSIZ * 2;
		if ( room(s) < len )
			exit_err("RECV OVERFLOW!\n");
		while ( len ){
			if ( s->rend >= s->rstart ) /* not wrapped */
				l = RINGSIZ - s->rend;
			else
				l = s->rstart - s->rend - 1;
			if ( l > len )
				l = len;
			lseek( bfd, base + s->rend, SEEK_SET );
			if ( write( bfd, ptr, l ) < 0 )
			    exit_err("cannot write to backing file\n");
			ptr += l;
			s->rend += l;
			if ( s->rend == RINGSIZ )
				s->rend = 0;
			len -= l;
		}
		/* throttle incoming data if there's no room in ring buf */
		if ( room(s) < UIP_RECEIVE_WINDOW )
			uip_stop();
		/* tell kernel we have data */
		ne.data = s->rend;
		/* ne.info = ???; for udp only? */
		ksend( NE_DATA );
	}

	if ( uip_acked() ){
		/* actually remove the bytes from the ring
		 * and send ROOM notice to kernel */
		ne.data = (s->tstart + s->len) & (RINGSIZ - 1);
		s->tstart = ne.data;  /* adjust local copy also!!! */
		ksend( NE_ROOM );
		s->len = 0;
	}

	if ( uip_poll() ){
		uint16_t len;

		/* if flow stopped, check to see if there's enough room
		   to start it again.
		*/
		if ( uip_stopped(uip_conn) && ( room(s) >= UIP_RECEIVE_WINDOW) )
			uip_restart();

		/* send data if there is some in the xmit ringbuf */
		if ( ! s->len ){
			if ( s->tend != s->tstart ){
				if ( s->tend > s->tstart ) /* not wrapped */
					len = s->tend - s->tstart;
				else  /* wrapped - just go to logical end of data */
					len = RINGSIZ - s->tstart;
				/* don't send more than TCP will actually send */
				if ( uip_mss() < len )
					s->len = uip_mss();
				else
					s->len = len;
				send_tcp( s );
			}
		}

		/* test for local close - LINK_CLOSED is set by kernel event thread */
		if ( s->flags & LINK_CLOSED ){
			uip_close();
			s->flags &= ~LINK_CLOSED;
			s->flags |= LINK_SHUTW;
			if ( s->flags & LINK_SHUTR )
				rel_map( uip_conn->appstate );
		}

	}

	if ( uip_rexmit() ){
		/* resend data */
		send_tcp( s );
	}

	if ( uip_closed() || uip_aborted() ){
		int e;
		switch ( s->flags & (LINK_SHUTR | LINK_SHUTW) ){
		case 0:
			e = NE_SHUTR;
			break;
		case LINK_SHUTR:
			goto close;
		case LINK_SHUTW:
			e = NE_NEWSTATE;
			break;
		case LINK_SHUTR | LINK_SHUTW:
			return;
		}
		/* formulate and send reset of message */
		ne.data = SS_CLOSED;
		ksend( e );
		/* release link resource if both sides closed */
		/* mark link as remote closed */
	close:
		s->flags |= LINK_SHUTR;
		if ( s->flags & LINK_SHUTW )
			rel_map( uip_conn->appstate );
	}
}



/* uIP callbck for UDP event */
void netd_udp_appcall()
{
	/* debug
	   printe( "appcall udp: " );
	   if( uip_aborted() ) printe("aborted");
	   if( uip_acked() )  printe("acked");
	   if( uip_closed() ) printe("closed");
	   if( uip_newdata() ) printe("new data");
	   if( uip_poll() ) printe("polled");
	   if( uip_rexmit() ) printe("rexmit");
	   if( uip_timedout() ) printe("timed out");
	   if( uip_connected() ) printe("connected" );
	   printe("\n");
	*/

	struct link *s = & map[uip_udp_conn->appstate];
	ne.socket = s->socketn;

	if ( uip_poll() ){
		/* send data if there's data waiting on this connection */
		/*    doing this before testing for close ??? */
		if ( s->tend != s->tstart ){
			send_udp( s );
			return; /* short circuit LINK_CLOSED until EOD. */
		}
		/* if flagged close from kernel, then really close, confirm w/ kernel */
		if ( s->flags & LINK_CLOSED ){
			/* send closed event back to kernel */
			ne.socket = sm.s.s_num;
			ne.data = SS_CLOSED;
			ksend( NE_NEWSTATE );
			/* release private link resource */
			rel_map( uip_udp_conn->appstate );
			uip_udp_remove( uip_udp_conn );
			return;
		}
	}

	if ( uip_newdata() ){
		/* there new data in the packet buffer */
		uint16_t len = uip_datalen();
		char *ptr = uip_appdata;
		uint32_t base = ne.socket * RINGSIZ * 2;

		if ( (s->rend + 1)&(NSOCKBUF-1) == s->rstart )
			return; /* full - drop it */
		s->rsize[s->rend] = len;
		memcpy( &ne.info, s->rsize, sizeof(uint16_t) * NSOCKBUF );
		lseek( bfd, base + s->rend * RXPKTSIZ, SEEK_SET );
		if ( write( bfd, ptr, len ) < 0 )
		    exit_err("cannot write to backing file\n");
		if ( ++s->rend == NSOCKBUF )
			s->rend = 0;
		/* FIXME: throttle incoming data if there's no room in ring buf */
		/* tell kernel we have data */
		ne.data = s->rend;
		/* ne.info = ???; for udp only? */
		ksend( NE_DATA );
	}
}

/* uIP callbck for UDP event */
void netd_raw_appcall()
{
	/* debug
	   printe( "appcall udp: " );
	   if( uip_aborted() ) printe("aborted");
	   if( uip_acked() )  printe("acked");
	   if( uip_closed() ) printe("closed");
	   if( uip_newdata() ) printe("new data");
	   if( uip_poll() ) printe("polled");
	   if( uip_rexmit() ) printe("rexmit");
	   if( uip_timedout() ) printe("timed out");
	   if( uip_connected() ) printe("connected" );
	   printe("\n");
	*/

	struct link *s = & map[uip_raw_conn->appstate];
	ne.socket = s->socketn;

	if ( uip_poll() ){
		/* send data if there's data waiting on this connection */
		/*    doing this before testing for close ??? */
		if ( s->tend != s->tstart ){
			send_raw( s );
			return; /* short circuit LINK_CLOSED until EOD. */
		}
		/* if flagged close from kernel, then really close, confirm w/ kernel */
		if ( s->flags & LINK_CLOSED ){
			/* send closed event back to kernel */
			ne.socket = sm.s.s_num;
			ne.data = SS_CLOSED;
			ksend( NE_NEWSTATE );
			/* release private link resource */
			rel_map( uip_raw_conn->appstate );
			uip_raw_remove( uip_raw_conn );
			return;
		}
	}

	if ( uip_newdata() ){
		/* there new data in the packet buffer */
		uint16_t len = uip_datalen();
		char *ptr = uip_appdata;
		uint32_t base = ne.socket * RINGSIZ * 2;

		if ( (s->rend + 1)&(NSOCKBUF-1) == s->rstart )
			return; /* full - drop it */
		s->rsize[s->rend] = len;
		memcpy( &ne.info, s->rsize, sizeof(uint16_t) * NSOCKBUF );
		lseek( bfd, base + s->rend * RXPKTSIZ, SEEK_SET );
		if ( write( bfd, ptr, len ) < 0 )
		    exit_err("cannot write to backing file\n");
		if ( ++s->rend == NSOCKBUF )
			s->rend = 0;
		/* FIXME: throttle incoming data if there's no room in ring buf */
		/* tell kernel we have data */
		ne.data = s->rend;
		/* ne.info = ???; for udp only? */
		ksend( NE_DATA );
	}
}


/* unused (for now) */
void uip_log(char *m)
{
	fprintf( stderr, "uIP: %s\n", m);
}


/* handle events from kernel */
/* returns 0 if nothing going on */
int dokernel( void )
{
	struct link *m;
	int i = read( knet, &sm, sizeof(sm) );
	if ( i < 0 ){
		return 0;
	}
	else if ( i == sizeof( sm ) && sm.sd.event & 127 ){
		/* debug
		   fprintf(stderr,"read size: %d ", i );
		   fprintf(stderr,"knet lcn: %d ", sm.sd.lcn );
		   fprintf(stderr,"event: %x ", sm.sd.event );
		   fprintf(stderr,"newstat: %x\n", sm.sd.newstate );
		*/
		m = & map[sm.sd.lcn];
		if ( sm.sd.event & NEV_STATE ){
			ne.socket = sm.s.s_num;
			switch ( sm.sd.newstate ){
			case SS_UNCONNECTED:
				ne.data = get_map(); /* becomes lcn */
				map[ne.data].len = 0;
				map[ne.data].socketn = sm.s.s_num;
				ksend( NE_INIT );
				break;
			case SS_BOUND:
				/* FIXME: probably needs to do something here  */
				ne.data = SS_BOUND;
				ksend( NE_NEWSTATE );
				break;
			case SS_CONNECTING:
				if ( sm.s.s_type == SOCKTYPE_TCP ){
					struct uip_conn *conptr;
					uip_ipaddr_t addr;
					int port = sm.s.s_addr[SADDR_DST].port;
					uip_ipaddr_copy( &addr, (uip_ipaddr_t *)
							 &sm.s.s_addr[SADDR_DST].addr );
					/* need some HTONS'ing done here? */
					conptr = uip_connect( &addr, port );
					if ( !conptr ){
						break; /* fixme: actually handler the error */
					}
					m->conn = conptr; /* fixme: needed? */
					conptr->appstate = sm.sd.lcn;
					break;
				}
				else if ( sm.s.s_type == SOCKTYPE_UDP ){
					struct uip_udp_conn *conptr;
					uip_ipaddr_t addr;
					int port = sm.s.s_addr[SADDR_DST].port;
					uip_ipaddr_copy( &addr, (uip_ipaddr_t *)
							 &sm.s.s_addr[SADDR_DST].addr );
					/* need some HTONS'ing done here? */
					conptr = uip_udp_new( &addr, port );
					if ( !conptr ){
						break; /* fixme: actually handler the error */
					}
					m->conn = ( struct uip_conn *)conptr; /* fixme: needed? */
					conptr->appstate = sm.sd.lcn;
					/* fixme: assign local address/port !!! */
					/* refactor: same as tcp action from connect event */
					ne.data = SS_CONNECTED;
					ksend( NE_NEWSTATE );
					break;
				}
				else if ( sm.s.s_type == SOCKTYPE_RAW ){
					struct uip_raw_conn *conptr;
					uip_ipaddr_t addr;
					int port = sm.s.s_addr[SADDR_DST].port;
					uip_ipaddr_copy( &addr, (uip_ipaddr_t *)
							 &sm.s.s_addr[SADDR_DST].addr );
					/* need some HTONS'ing done here? */
					conptr = uip_raw_new( &addr, port );
					if ( !conptr ){
						break; /* fixme: actually handler the error */
					}
					m->conn = ( struct uip_conn *)conptr; /* fixme: needed? */
					conptr->appstate = sm.sd.lcn;
					/* refactor: same as tcp action from connect event */
					ne.data = SS_CONNECTED;
					ksend( NE_NEWSTATE );
					break;
				}
				break; /* FIXME: handle unknown/unhandled sock types here */
			case SS_CLOSED:
				if ( sm.s.s_type == SOCKTYPE_TCP ){
					m->flags += LINK_SHUTW;
					/* if remote already closed then send closing synch event
					   directly back to kernel, and release our link */
					if ( m->flags & LINK_SHUTR ){
						ne.data = SS_CLOSED;
						ksend( NE_NEWSTATE );
						rel_map( sm.sd.lcn );
					}
					/* tell uIP thread that local has closed, but we
					   cant send anything directly to uIP from here -
					   so set a flag and let uip_poll() handler do the job
					   the next time through the main program loop
					*/
					else {
						m->flags += LINK_CLOSED;
					}
				}
				else if ( sm.s.s_type == SOCKTYPE_UDP ||
					sm.s.s_type == SOCKTYPE_RAW ){
					m->flags += LINK_CLOSED;
				}
				break;
			case SS_LISTENING:
				/* htons here? */
				uip_listen( sm.s.s_addr[0].port );
				m->port = sm.s.s_addr[0].port;
				ne.data = SS_LISTENING;
				ksend( NE_NEWSTATE );
				break;
			default:
				break;
			}
		}
		if ( sm.sd.event & NEV_WRITE ){
			/* can't send data now, so store needed buffer info */
			int last;
			m->tstart = sm.sd.tbuf;
			m->tend = sm.sd.tnext;
			last = m->tend-1 & NSOCKBUF-1;
			/* tsize is for udp only, but copy it anyway */
			m->tsize[last] = sm.sd.tlen[last];
		}
		if ( sm.sd.event & NEV_READ ){
			int last;
			m->rstart = sm.sd.rbuf;
			m->rend = sm.sd.rnext;
		}
		return 1;
	}
	return 0;
}

/* handle events from uIP */
/* returns 0 if nothing going on */
int douip( void )
{
	int ret = 0;
	int i = device_read( uip_buf, UIP_BUFSIZE );
	if (i > 0){
		ret = 1;
		uip_len = i;
		if (uip_len > 0) {
			if ( BUF->type == UIP_HTONS(UIP_ETHTYPE_IP)){
				uip_arp_ipin();
				uip_input();
				if (uip_len > 0) {
					uip_arp_out();
					device_send( uip_buf, uip_len);
				}
			}else if ( BUF->type == UIP_HTONS(UIP_ETHTYPE_ARP)){
				uip_arp_arpin();
				if (uip_len > 0 )
					device_send( uip_buf, uip_len);
			}
		}
	} else if (timer_expired(&periodic_timer)) {
		timer_reset(&periodic_timer);
		for (i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);
			if (uip_len > 0) {
				uip_arp_out();
				device_send( uip_buf, uip_len );
			}
		}
		for (i = 0; i < UIP_UDP_CONNS; i++) {
			uip_udp_periodic(i);
			if (uip_len > 0) {
				uip_arp_out();
				device_send( uip_buf, uip_len );
			}
		}
		for (i = 0; i < UIP_RAW_CONNS; i++) {
			uip_raw_periodic(i);
			if (uip_len > 0) {
				uip_arp_out();
				device_send( uip_buf, uip_len );
			}
		}
		if ( timer_expired(&arp_timer)){
			timer_reset(&arp_timer);
			uip_arp_timer();
		}
	}
	return ret;
}




/* Get charactor from rc file */
/*   returns charactor from file, -1 on EOF */
int mygetc( )
{
	static char *ibuf[80];
	static char *pos;
	static int len = 0;
	if ( ! len ){
		pos = (char *)ibuf;
		len = read( rc, ibuf, 80 );
		if( len < -1 )
			exit_err( "Error Reading rc file.\n" );
		if( ! len )
			return -1;
	}
	len--;
	return *pos++;
}

/* parse next token from rc file */
/*   return -1 on EOF, */
int word( char *buf, int max)
{
	int c;
	int len = 0;

	max -= 1;
	/* skip spaces */
	do {
		c = mygetc();
		if ( c < 0 )
			return -1;
	} while ( isspace( c ) );
	/* build token */
	while ( ! isspace( c ) && c != -1 && len < max ){
		*buf++ = c;
		len++;
		c = mygetc();
	}
	*buf = 0;
	return 0;
}

/* parse a mac address from rc file */
/*    need dynamic conversion base ? */
int macaddrconv( char *buf, uip_eth_addr *addr )
{
	int i;
	char *e;
	for ( i = 0; i < 6; i++){
		addr->addr[i] = strtol( buf, &e, 16 );
		if ( e == buf )
			return 0;
		buf = ++e;
	}
	return -1;
}

/* return various errors for rcfile problems */
void exit_badrc( void )
{
	exit_err("error reading rc file\n" );
}

void exit_badaddr( void )
{
	exit_err("rc file: bad address" );
}

/* get ip address from rc file */
int getaddr( uip_ipaddr_t *ipaddr )
{
	char buf[80];
	if ( word( buf, 80 ) )
		exit_badrc();
	if ( ! uiplib_ip4addrconv( buf, ipaddr) )
		exit_badaddr();
	return -1;
}


/* read, parse, and apply rc file */
int parse_rcfile( void ){

	int x;
	char buf[80];
	uip_ipaddr_t ipaddr;        /* ip address buffer */
	uip_eth_addr ethaddr;       /* mac address buffer */

	rc = open("/etc/netrc", O_RDONLY, 0 );
	if ( rc < 0 ){
		exit_err("cannot open rc file\n");
	}
	while ( ! word( buf, 80 ) ){
		if ( ! strcmp( buf, "ipaddr" ) ){
			getaddr( &ipaddr );
			uip_sethostaddr(&ipaddr);
			continue;
		}
		if ( ! strcmp( buf, "gateway" ) ){
			getaddr( &ipaddr );
			uip_setdraddr(&ipaddr);
			continue;
		}
		if ( ! strcmp( buf, "netmask" ) ){
			getaddr( &ipaddr );
			uip_setnetmask(&ipaddr);
			continue;
		}
		if ( ! strcmp( buf, "mac" ) ){
			if ( word( buf, 80 ) )
				exit_badrc();
			if ( ! macaddrconv( buf, &ethaddr ) )
				exit_err("bad mac address\n");
			uip_setethaddr( ethaddr );
			continue;
		}
		if ( buf[0]== '#' ){
			int c;
			do {
				c = mygetc();
			} while ( c != '\n' && c != -1 );
		}
	}
	close(rc);
	return 0;
}



int main( int argc, char *argv[] )
{
	int ret;
	uip_ipaddr_t ipaddr;
	uip_eth_addr ethaddr;       /* mac address buffer */
	
	/* where should backing file go? /var, /tmp, other?  */
	bfd = open( "/tmp/net.back", O_RDWR|O_CREAT, 0644 );
	if ( bfd < 0 ){
		exit_err( "cannot open backing file\n");
	}

	/*open Kernel's net device */
	knet = open( "/dev/net", O_RDWR|O_NDELAY, 0 );
	if ( knet < 0 ){
		exit_err( "cannot open kernel net iface device\n");
	}

	/* Attach our backing file to the network device */
	ret = ioctl( knet, NET_INIT, &bfd );
	if ( ret < 0 ){
		exit_err( "cannot attach backing file\n");
	}

	/* initialize our map */
	init_map();


	/*
	 * Set up uIP
	 */

	timer_set(&periodic_timer, CLOCK_SECOND / 4);
	timer_set(&arp_timer, CLOCK_SECOND * 10 );

	uip_init();

	/*
	 * set default addresses structures for uIP.
	 */

	/* set hostname - need a config file or cmd line here */
	/* what are a good defaults here? */
	uip_ipaddr(&ipaddr, 192,168,42,2);
	uip_sethostaddr(&ipaddr);
	uip_ipaddr(&ipaddr, 192,168,42,1);
	uip_setdraddr(&ipaddr);
	uip_ipaddr(&ipaddr, 255,255,255,0);
	uip_setnetmask(&ipaddr);

	/* set default MAC address */
	ethaddr.addr[0] = 0x00;
	ethaddr.addr[1] = 0x01;
	ethaddr.addr[2] = 0x02;
	ethaddr.addr[3] = 0x03;
	ethaddr.addr[4] = 0x04;
	ethaddr.addr[5] = 0x05;
	uip_setethaddr(ethaddr);


	parse_rcfile();

	if( device_init() ){
		exit_err( "cannot init net device\n");
	}


	while(1) {
		int a,b;
		a = dokernel();
		b = douip();
		if( ! (a || b) )
			_pause(3);
	}
}
