#include "../config.h"

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <netinet/in.h>
#include <ev.h>
#include <sysexits.h>

#include <boost/ptr_container/ptr_list.hpp>

#include "../Socket/Socket.hxx"
#include "../Log/TimestampLog.hxx"

static const size_t READ_SIZE = 4096;
static const int MAX_CONN_BACKLOG = 32;

std::string logfilename;
std::ofstream logfile;
std::auto_ptr<std::ostream> log;

std::auto_ptr<SockAddr::SockAddr> bind_addr_outgoing;

struct connection {
	Socket s_client;
	Socket s_server;

	ev_io e_s_connect;
	ev_io e_c_read, e_c_write;
	ev_io e_s_read, e_s_write;
};
boost::ptr_list< struct connection > connections;


void received_sigint(EV_P_ ev_signal *w, int revents) throw() {
	*log << "Received SIGINT, exiting\n" << std::flush;
	ev_break(EV_A_ EVUNLOOP_ALL);
}
void received_sigterm(EV_P_ ev_signal *w, int revents) throw() {
	*log << "Received SIGTERM, exiting\n" << std::flush;
	ev_break(EV_A_ EVUNLOOP_ALL);
}

void received_sighup(EV_P_ ev_signal *w, int revents) throw() {
	*log << "Received SIGHUP, closing this logfile\n" << std::flush;
	logfile.close();
	logfile.open(logfilename.c_str(), std::ios_base::app | std::ios_base::out );
	*log << "Received SIGHUP, (re)opening this logfile\n" << std::flush;
}

void kill_connection(EV_P_ struct connection *con) {
	// Remove from event loops
	ev_io_stop(EV_A_ &con->e_c_read );
	ev_io_stop(EV_A_ &con->e_c_write );
	ev_io_stop(EV_A_ &con->e_s_read );
	ev_io_stop(EV_A_ &con->e_s_write );

	// Find and erase this connection in the list
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

	if( con->s_server.getsockopt_so_error() != 0 ) {
		// TODO: Delete from connections[]?
		*log << "Connection seems BAD\n" << std::flush;
	}

	*log << "Connection seems fine\n" << std::flush;
	// TODO: connect the client & server socket together

	kill_connection(EV_A_ con);
}

static void listening_socket_ready_for_read(EV_P_ ev_io *w, int revents) {
	Socket* s_listen = reinterpret_cast<Socket*>( w->data );

	std::auto_ptr<struct connection> new_con( new struct connection );

	std::auto_ptr<SockAddr::SockAddr> client_addr;
	std::auto_ptr<SockAddr::SockAddr> server_addr;
	try {
		new_con->s_client = s_listen->accept(&client_addr);

		server_addr = new_con->s_client.getsockname();

		new_con->s_client.non_blocking(true);

		*log << "Connection intercepted "
			 << client_addr->string() << "-->"
			 << server_addr->string() << "\n" << std::flush;

		new_con->s_server = Socket::socket(AF_INET, SOCK_STREAM, 0);

		if( bind_addr_outgoing.get() != NULL ) {
			new_con->s_server.bind( *bind_addr_outgoing );
			*log << "Connecting " << bind_addr_outgoing->string()
				<< "-->";
		} else {
#if HAVE_DECL_IP_TRANSPARENT
			int value = 1;
			new_con->s_server.setsockopt(SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
#endif
			new_con->s_server.bind( *client_addr );
			*log << "Connecting " << client_addr->string()
				<< "-->";
		}
		*log << server_addr->string() << "\n" << std::flush;

		new_con->s_server.non_blocking(true);
	} catch( Errno &e ) {
		*log << "Error: " << e.what() << std::flush;
		return;
		// Sockets will go out of scope, and close() themselves
	}

	ev_io_init( &new_con->e_s_connect, server_socket_connect_done, new_con->s_server, EV_WRITE );
	ev_io_init( &new_con->e_c_read,  NULL, new_con->s_client, EV_READ );
	ev_io_init( &new_con->e_c_write, NULL, new_con->s_client, EV_WRITE );
	ev_io_init( &new_con->e_s_read,  NULL, new_con->s_server, EV_READ );
	ev_io_init( &new_con->e_s_write, NULL, new_con->s_server, EV_WRITE );
	new_con->e_s_connect.data =
		new_con->e_c_read.data =
		new_con->e_c_write.data =
		new_con->e_s_read.data =
		new_con->e_s_write.data =
			new_con.get();

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
			*log << "Error: " << e.what() << std::flush;
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
	log.reset( new TimestampLog( std::cerr ) );

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
			case '?':
			case 'h':
				std::cerr <<
				//  >---------------------- Standard terminal width ---------------------------------<
					"Options:\n"
					"  -h -? --help                    Displays this help message and exits\n"
					"  -V --version                    Displays the version and exits\n"
					"  --foreground -f                 Don't fork into the background after init\n"
					"  --pid-file -p file              The file to write the PID to, especially\n"
					"                                  usefull when running as a daemon\n"
					"  --bind-listen -b host:port      Bind to the specified address for incomming\n"
					"                                  connections.\n"
					"                                  host and port resolving can be bypassed by\n"
					"                                  placing [] around them\n"
					"  --bind-outgoing -b host:port    Bind to the specified address for outgoing\n"
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
				          << " (" << PACKAGE_GITREVISION << ")\n";
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
				logfile.open(logfilename.c_str(), std::ios_base::app | std::ios_base::out );
				log.reset( new TimestampLog( logfile ) );
				break;
			}
		}
	}

	*log << "Parsed options, opening listening socket on "
	     << options.bind_addr_listen << "\n" << std::flush;

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

		*log << "Listening on " << (*bind_sa)[0].string() << "\n" << std::flush;
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

		*log << "Outgoing connections will connect from "
		     << bind_addr_outgoing->string() << "\n" << std::flush;
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

		*log << "Setup done, starting event loop\n" << std::flush;
		ev_run(EV_DEFAULT_ 0);
	}

	*log << "Exiting...\n" << std::flush;
	return 0;
}
