#include "../config.h"

#include <iostream>
#include "../Socket/Socket.hxx"
#include "../Log/TimestampLog.hxx"
#include <netinet/in.h>
#include <ev.h>

const static unsigned short listen_port = 5000;
const static bool connect_from_client_address = false;

std::auto_ptr<std::ostream> log;

void received_sigint(EV_P_ ev_signal *w, int revents) throw() {
	*log << "Received SIGINT, exiting\n" << std::flush;
	ev_break(EV_A_ EVUNLOOP_ALL);
}
void received_sigterm(EV_P_ ev_signal *w, int revents) throw() {
	*log << "Received SIGTERM, exiting\n" << std::flush;
	ev_break(EV_A_ EVUNLOOP_ALL);
}

static void listening_socket_ready_for_read(EV_P_ ev_io *w, int revents) {
	Socket* socket = reinterpret_cast<Socket*>( w->data );

	std::auto_ptr<SockAddr::SockAddr> client_addr;
	Socket client_socket = socket->accept(&client_addr);
	*log << "Connection established from " << client_addr->string() << "\n" << std::flush;

	std::auto_ptr<SockAddr::SockAddr> server_addr;
	server_addr = client_socket.getsockname();
	*log << "Connection established to   " << server_addr->string() << "\n" << std::flush;

	Socket server_socket = Socket::socket(AF_INET, SOCK_STREAM, 0);

	if( connect_from_client_address ) {
#if HAVE_DECL_IP_TRANSPARENT
		int value = 1;
		server_socket.setsockopt(SOL_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPPROTO_IPV6
#endif
		server_socket.bind( *client_addr );
		*log << "Bound server socket\n" << std::flush;
	}

	server_socket.connect( *server_addr );
	*log << "Connected server socket\n" << std::flush;
}

int main(int argc, char* argv[]) {
	Socket s_listen = Socket::socket(AF_INET, SOCK_STREAM, 0);

	log.reset( new TimestampLog( std::cerr ) );

#if HAVE_DECL_IP_TRANSPARENT
	{
		int value = 1;
		s_listen.setsockopt(SOL_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPPROTO_IPV6
	}
#endif

	std::auto_ptr<SockAddr::SockAddr> bind_addr(
		SockAddr::translate("0.0.0.0", listen_port) );

	s_listen.bind(*bind_addr);
	s_listen.listen(10);
	*log << "Listening on " << bind_addr->string() << "\n" << std::flush;

	{
		ev_signal ev_sigint_watcher;
		ev_signal_init( &ev_sigint_watcher, received_sigint, SIGINT);
		ev_signal_start( EV_DEFAULT_ &ev_sigint_watcher);
		ev_signal ev_sigterm_watcher;
		ev_signal_init( &ev_sigterm_watcher, received_sigterm, SIGTERM);
		ev_signal_start( EV_DEFAULT_ &ev_sigterm_watcher);

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
