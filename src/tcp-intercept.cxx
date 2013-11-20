#include "../config.h"

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <netinet/in.h>
#include <ev.h>
#include <sysexits.h>

#include <boost/ptr_container/ptr_list.hpp>

#include "../Socket/Socket.hxx"
#include <libsimplelog.h>

std::string logfilename;
FILE *logfile;

static const int MAX_CONN_BACKLOG = 32;

std::auto_ptr<SockAddr::SockAddr> bind_addr_outgoing;

struct connection {
	std::string id;

	Socket s_client;
	Socket s_server;

	ev_io e_s_connect;
	ev_io e_c_read, e_c_write;
	ev_io e_s_read, e_s_write;

	std::string buf_c_to_s, buf_s_to_c;
	bool con_open_c_to_s, con_open_s_to_c;
};
boost::ptr_list< struct connection > connections;


void received_sigint(EV_P_ ev_signal *w, int revents) throw() {
	LogInfo("Received SIGINT, exiting");
	ev_break(EV_A_ EVUNLOOP_ALL);
}
void received_sigterm(EV_P_ ev_signal *w, int revents) throw() {
	LogInfo("Received SIGTERM, exiting");
	ev_break(EV_A_ EVUNLOOP_ALL);
}

void received_sighup(EV_P_ ev_signal *w, int revents) throw() {
	LogInfo("Received SIGHUP, closing this logfile");
	if( logfilename.size() > 0 ) {
		fclose(logfile);
		logfile = fopen(logfilename.c_str(), "a");
		LogSetOutputFile(NULL, logfile);
	} /* else we're still logging to stderr, which doesn't need reopening */
	LogInfo("Received SIGHUP, (re)opening this logfile");
}

void kill_connection(EV_P_ struct connection *con) {
	// Remove from event loops
	ev_io_stop(EV_A_ &con->e_c_read );
	ev_io_stop(EV_A_ &con->e_c_write );
	ev_io_stop(EV_A_ &con->e_s_read );
	ev_io_stop(EV_A_ &con->e_s_write );

	LogInfo("%s: closed", con->id.c_str());

	// Find and erase this connection in the list
	// TODO scaling issue: this is O(n) with the number of connections.
	for( typeof(connections.begin()) i = connections.begin(); i != connections.end(); ++i ) {
		if( &(*i) == con ) {
			connections.erase(i);
			break; // Stop searching
		}
	}
}

static void server_socket_connect_done(EV_P_ ev_io *w, int revents) {
	struct connection* con = reinterpret_cast<struct connection*>( w->data );

	ev_io_stop(EV_A_ &con->e_s_connect); // We connect only once

	Errno connect_error("connect()", con->s_server.getsockopt_so_error());
	if( connect_error.error_number() != 0 ) {
		LogWarn("%s: connect to server failed: %s", con->id.c_str(),
		     connect_error.what() );
		kill_connection(EV_A_ con);
		return;
	}

	LogInfo("%s: server accepted connection, splicing", con->id.c_str());
	ev_io_start(EV_A_ &con->e_c_write);
	ev_io_start(EV_A_ &con->e_s_write);
}

inline static void peer_ready_write(EV_P_ struct connection* con,
                                    std::string const &dir,
                                    bool &con_open,
                                    Socket &rx, ev_io *e_rx_read,
                                    std::string &buf,
                                    Socket &tx, ev_io *e_tx_write ) {
	if( buf.length() == 0 ) {
		// All is written, and we're still ready to write, read some more
		ev_io_start( EV_A_ e_rx_read );
		ev_io_stop( EV_A_ e_tx_write );
		return;
	}
	// buf.length() > 0
	try {
		ssize_t rv = tx.send(buf.data(), buf.length());
		assert( rv > 0 );
		buf = buf.substr( rv );
	} catch( Errno &e ) {
		LogError("%s %s: %s", con->id.c_str(), dir.c_str(), e.what());
		kill_connection(EV_A_ con);
	}
}
inline static void peer_ready_read(EV_P_ struct connection* con,
                                   std::string const &dir,
                                   bool &con_open,
                                   Socket &rx, ev_io *e_rx_read,
                                   std::string &buf,
                                   Socket &tx, ev_io *e_tx_write ) {
	// TODO allow read when we still have data in the buffer to streamline things
	assert( buf.length() == 0 );
	try {
		buf = rx.recv();
		ev_io_stop( EV_A_ e_rx_read );
		if( buf.length() == 0 ) { // EOF has been read
			LogInfo("%s %s: EOF", con->id.c_str(), dir.c_str());
			tx.shutdown(SHUT_WR); // shutdown() does not block
			con_open = false;
			if( !con->con_open_s_to_c && !con->con_open_c_to_s ) {
				// Connection fully closed, clean up
				kill_connection(EV_A_ con);
			}
		} else { // data has been read
			ev_io_start( EV_A_ e_tx_write );
		}
	} catch( Errno &e ) {
		LogError("%s %s: %s", con->id.c_str(), dir.c_str(), e.what());
		kill_connection(EV_A_ con);
	}
}

static void client_ready_write(EV_P_ ev_io *w, int revents) {
	struct connection* con = reinterpret_cast<struct connection*>( w->data );
	assert( w == &con->e_c_write );
	return peer_ready_write(EV_A_ con, "S>C", con->con_open_s_to_c,
	                        con->s_server, &con->e_s_read,
	                        con->buf_s_to_c,
	                        con->s_client, &con->e_c_write);
}
static void server_ready_write(EV_P_ ev_io *w, int revents) {
	struct connection* con = reinterpret_cast<struct connection*>( w->data );
	assert( w == &con->e_s_write );
	return peer_ready_write(EV_A_ con, "C>S", con->con_open_c_to_s,
	                        con->s_client, &con->e_c_read,
	                        con->buf_c_to_s,
	                        con->s_server, &con->e_s_write);
}

static void client_ready_read(EV_P_ ev_io *w, int revents) {
	struct connection* con = reinterpret_cast<struct connection*>( w->data );
	assert( w == &con->e_c_read );
	return peer_ready_read(EV_A_ con, "C>S", con->con_open_c_to_s,
	                       con->s_client, &con->e_c_read,
	                       con->buf_c_to_s,
	                       con->s_server, &con->e_s_write);
}
static void server_ready_read(EV_P_ ev_io *w, int revents) {
	struct connection* con = reinterpret_cast<struct connection*>( w->data );
	assert( w == &con->e_s_read );
	return peer_ready_read(EV_A_ con, "S>C", con->con_open_s_to_c,
	                       con->s_server, &con->e_s_read,
	                       con->buf_s_to_c,
	                       con->s_client, &con->e_c_write);
}


static void listening_socket_ready_for_read(EV_P_ ev_io *w, int revents) {
	Socket* s_listen = reinterpret_cast<Socket*>( w->data );

	std::auto_ptr<struct connection> new_con( new struct connection );

	std::auto_ptr<SockAddr::SockAddr> client_addr;
	std::auto_ptr<SockAddr::SockAddr> server_addr;
	SockAddr::SockAddr *server_bind_addr;
	try {
		new_con->s_client = s_listen->accept(&client_addr);

		server_addr = new_con->s_client.getsockname();

		new_con->id.assign( client_addr->string() );
		new_con->id.append( "-->" );
		new_con->id.append( server_addr->string() );

		new_con->s_client.non_blocking(true);

		LogInfo("%s: Connection intercepted", new_con->id.c_str());

		// TODO: make IPv6 ready
		new_con->s_server = Socket::socket(AF_INET, SOCK_STREAM, 0);

		if( bind_addr_outgoing.get() != NULL ) {
			server_bind_addr = bind_addr_outgoing;
		} else {
#if HAVE_DECL_IP_TRANSPARENT
			int value = 1;
			new_con->s_server.setsockopt(SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
#endif
			server_bind_addr = client_addr;
		}
		new_con->s_server.bind( *server_bind_addr );

		new_con->s_server.non_blocking(true);
	} catch( Errno &e ) {
		LogError("Error: %s", e.what());
		return;
		// Sockets will go out of scope, and close() themselves
	}

	ev_io_init( &new_con->e_s_connect, server_socket_connect_done, new_con->s_server, EV_WRITE );
	ev_io_init( &new_con->e_c_read,  client_ready_read,  new_con->s_client, EV_READ );
	ev_io_init( &new_con->e_c_write, client_ready_write, new_con->s_client, EV_WRITE );
	ev_io_init( &new_con->e_s_read,  server_ready_read,  new_con->s_server, EV_READ );
	ev_io_init( &new_con->e_s_write, server_ready_write, new_con->s_server, EV_WRITE );
	new_con->e_s_connect.data =
		new_con->e_c_read.data =
		new_con->e_c_write.data =
		new_con->e_s_read.data =
		new_con->e_s_write.data =
			new_con.get();
	new_con->con_open_c_to_s = new_con->con_open_s_to_c = true;

	LogInfo("%s: Connecting %s-->%s", new_con->id.c_str(),
			server_bind_addr->string().c_str(), server_addr->string().c_str());

	try {
		new_con->s_server.connect( *server_addr );
		// Connection succeeded right away, flag the callback right away
		ev_feed_event(EV_A_ &new_con->e_s_connect, 0);

	} catch( Errno &e ) {
		if( e.error_number() == EINPROGRESS ) {
			// connect() is started, wait for socket to become write-ready
			// Have libev call the callback
			ev_io_start( EV_A_ &new_con->e_s_connect );

		} else {
			LogError("Error: %s", e.what());
			return;
			// Sockets will go out of scope, and close() themselves
		}
	}

	connections.push_back( new_con.release() );
}


int main(int argc, char* argv[]) {
	// Default options
	struct {
		bool fork;
		std::string pid_file;
		std::string bind_addr_listen;
		std::string bind_addr_outgoing;
	} options = {
		/* fork = */ true,
		/* pid_file = */ "",
		/* bind_addr_listen = */ "[0.0.0.0]:[5000]",
		/* bind_addr_outgoing = */ "[0.0.0.0]:[0]"
		};

	{ // Parse options
		char optstring[] = "hVfp:b:B:l:";
		struct option longopts[] = {
			{"help",			no_argument, NULL, 'h'},
			{"version",			no_argument, NULL, 'V'},
			{"forgeground",		no_argument, NULL, 'f'},
			{"pid-file",		required_argument, NULL, 'p'},
			{"bind-listen",		required_argument, NULL, 'b'},
			{"bind-outgoing",	required_argument, NULL, 'B'},
			{"log",				required_argument, NULL, 'l'},
			{NULL, 0, 0, 0}
		};
		int longindex;
		int opt;
		while( (opt = getopt_long(argc, argv, optstring, longopts, &longindex)) != -1 ) {
			switch(opt) {
			case 'h':
			case '?':
				std::cerr <<
				//  >---------------------- Standard terminal width ---------------------------------<
					"Options:\n"
					"  -h --help                       Displays this help message and exits\n"
					"  -V --version                    Displays the version and exits\n"
					"  --foreground -f                 Don't fork into the background after init\n"
					"  --pid-file -p file              The file to write the PID to, especially\n"
					"                                  usefull when running as a daemon\n"
					"  --bind-listen -b host:port      Bind to the specified address for incomming\n"
					"                                  connections.\n"
					"                                  host and port resolving can be bypassed by\n"
					"                                  placing [] around them\n"
					"  --bind-outgoing -B host:port    Bind to the specified address for outgoing\n"
					"                                  connections.\n"
					"                                  host and port resolving can be bypassed by\n"
					"                                  placing [] around them\n"
					"                                  the special string \"client\" can be used to\n"
					"                                  reuse the client's source address. Note that\n"
					"                                  you should take care that the return packets\n"
					"                                  pass through this process again!\n"
					"  --log -l file                   Log to file\n"
					;
				if( opt == '?' ) exit(EX_USAGE);
				exit(EX_OK);
			case 'V':
				std::cout << PACKAGE_NAME << " version " << PACKAGE_VERSION
				          << " (" << PACKAGE_GITREVISION << ")\n"
				          << " configured with: " << CONFIGURE_ARGS << "\n"
				          << " CFLAGS=\"" << CFLAGS << "\" CXXFLAGS=\"" << CXXFLAGS << "\""
				          << " CPPFLAGS=\"" << CPPFLAGS << "\"\n"
				          << " Options:\n"
				          << "   IPv6: "
#ifdef ENABLE_IPV6
				                        << "yes\n"
#else
				                        << "no\n"
#endif
				          << "\n";
				exit(EX_OK);
			case 'f':
				options.fork = false;
				break;
			case 'p':
				options.pid_file = optarg;
				break;
			case 'b':
				options.bind_addr_listen = optarg;
				break;
			case 'B':
				options.bind_addr_outgoing = optarg;
				break;
			case 'l':
				logfilename = optarg;
				logfile = fopen(logfilename.c_str(), "a");
				LogSetOutputFile(NULL, logfile);
				break;
			}
		}
	}

	LogInfo(PACKAGE_NAME " version " PACKAGE_VERSION " (" PACKAGE_GITREVISION ") starting up");

	Socket s_listen;
	{ // Open listening socket
		std::string host, port;

		/* Address format is
		 *   - hostname:portname
		 *   - [numeric ip]:portname
		 *   - hostname:[portnumber]
		 *   - [numeric ip]:[portnumber]
		 */
		size_t c = options.bind_addr_listen.rfind(":");
		if( c == std::string::npos ) {
			std::cerr << "Invalid bind string \"" << options.bind_addr_listen << "\": could not find ':'\n";
			exit(EX_DATAERR);
		}
		host = options.bind_addr_listen.substr(0, c);
		port = options.bind_addr_listen.substr(c+1);

		std::auto_ptr< boost::ptr_vector< SockAddr::SockAddr> > bind_sa
			= SockAddr::resolve( host, port, 0, SOCK_STREAM, 0);
		if( bind_sa->size() == 0 ) {
			std::cerr << "Can not bind to \"" << options.bind_addr_listen << "\": Could not resolve\n";
			exit(EX_DATAERR);
		} else if( bind_sa->size() > 1 ) {
			// TODO: allow this
			std::cerr << "Can not bind to \"" << options.bind_addr_listen << "\": Resolves to multiple entries:\n";
			for( typeof(bind_sa->begin()) i = bind_sa->begin(); i != bind_sa->end(); i++ ) {
				std::cerr << "  " << i->string() << "\n";
			}
			exit(EX_DATAERR);
		}
		s_listen = Socket::socket( (*bind_sa)[0].proto_family() , SOCK_STREAM, 0);
		s_listen.set_reuseaddr();
		s_listen.bind((*bind_sa)[0]);
		s_listen.listen(MAX_CONN_BACKLOG);

#if HAVE_DECL_IP_TRANSPARENT
		int value = 1;
		s_listen.setsockopt(SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
#endif

		LogInfo("Listening on %s", (*bind_sa)[0].string().c_str());
	}

	if( options.bind_addr_outgoing == "client" ) {
		bind_addr_outgoing.reset(NULL);
	} else { // Resolve client address
		std::string host, port;

		/* Address format is
		 *   - hostname:portname
		 *   - [numeric ip]:portname
		 *   - hostname:[portnumber]
		 *   - [numeric ip]:[portnumber]
		 */
		size_t c = options.bind_addr_outgoing.rfind(":");
		if( c == std::string::npos ) {
			std::cerr << "Invalid bind string \"" << options.bind_addr_outgoing << "\": could not find ':'\n";
			exit(EX_DATAERR);
		}
		host = options.bind_addr_outgoing.substr(0, c);
		port = options.bind_addr_outgoing.substr(c+1);

		std::auto_ptr< boost::ptr_vector< SockAddr::SockAddr> > bind_sa
			= SockAddr::resolve( host, port, 0, SOCK_STREAM, 0);
		if( bind_sa->size() == 0 ) {
			std::cerr << "Can not bind to \"" << options.bind_addr_outgoing << "\": Could not resolve\n";
			exit(EX_DATAERR);
		} else if( bind_sa->size() > 1 ) {
			std::cerr << "Can not bind to \"" << options.bind_addr_outgoing << "\": Resolves to multiple entries:\n";
			for( typeof(bind_sa->begin()) i = bind_sa->begin(); i != bind_sa->end(); i++ ) {
				std::cerr << "  " << i->string() << "\n";
			}
			exit(EX_DATAERR);
		}
		bind_addr_outgoing.reset( bind_sa->release(bind_sa->begin()).release() ); // Transfer ownership; TODO: this should be simpeler that double release()

		LogInfo("Outgoing connections will connect from %s", bind_addr_outgoing->string().c_str());
	}


	{
		ev_signal ev_sigint_watcher;
		ev_signal_init( &ev_sigint_watcher, received_sigint, SIGINT);
		ev_signal_start( EV_DEFAULT_ &ev_sigint_watcher);

		ev_signal ev_sigterm_watcher;
		ev_signal_init( &ev_sigterm_watcher, received_sigterm, SIGTERM);
		ev_signal_start( EV_DEFAULT_ &ev_sigterm_watcher);

		ev_signal ev_sighup_watcher;
		ev_signal_init( &ev_sighup_watcher, received_sighup, SIGHUP);
		ev_signal_start( EV_DEFAULT_ &ev_sighup_watcher);


		ev_io e_listen;
		e_listen.data = &s_listen;
		ev_io_init( &e_listen, listening_socket_ready_for_read, s_listen, EV_READ );
		ev_io_start( EV_DEFAULT_ &e_listen );

		LogInfo("Setup done, starting event loop");
		try {
			ev_run(EV_DEFAULT_ 0);
		} catch( std::exception &e ) {
			std::cerr << e.what() << "\n";
			return EX_SOFTWARE;
		}
	}

	LogInfo("Exiting...");
	return 0;
}
