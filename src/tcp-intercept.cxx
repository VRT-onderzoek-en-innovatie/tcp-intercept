#include "../config.h"

#include <iostream>
#include "../Socket/Socket.hxx"
#include <netinet/in.h>

const static unsigned short listen_port = 5000;

int main(int argc, char* argv[]) {
	Socket listening_socket = Socket::socket(AF_INET, SOCK_STREAM, 0);

#if HAVE_DECL_IP_TRANSPARENT
	{
		int value = 1;
		listening_socket.setsockopt(IPPROTO_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPPROTO_IPV6
	}
#endif

	std::auto_ptr<SockAddr::SockAddr> bind_addr(
		SockAddr::translate("0.0.0.0", listen_port) );

	listening_socket.bind(*bind_addr);
	listening_socket.listen(10);
	std::cout << "Listening on " << bind_addr->string() << std::endl;

	std::auto_ptr<SockAddr::SockAddr> client_addr;
	Socket client_socket = listening_socket.accept(&client_addr);
	std::cout << "Connection established from " << client_addr->string() << std::endl;

	std::auto_ptr<SockAddr::SockAddr> server_addr;
	server_addr = client_socket.getsockname();
	std::cout << "Connection established to   " << server_addr->string() << std::endl;

	Socket server_socket = Socket::socket(AF_INET, SOCK_STREAM, 0);

#if HAVE_DECL_IP_TRANSPARENT
	{
		int value = 1;
		server_socket.setsockopt(IPPROTO_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPPROTO_IPV6
	}
#endif

	server_socket.bind( *client_addr );
	std::cout << "Bound server socket" << std::endl;
	server_socket.connect( *server_addr );
	std::cout << "Connected server socket" << std::endl;

	sleep(5);

	std::cout << "Exiting...\n";
	return 0;
}
