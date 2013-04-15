#include <iostream>
#include "../Socket/Socket.hxx"
#include <netinet/in.h>

const static unsigned short listen_port = 5000;

int main(int argc, char* argv[]) {
	Socket s = Socket::socket(AF_INET, SOCK_STREAM, 0);

#if HAVE_DECL_IPPROTO_IP && HAVE_DECL_IP_TRANSPARENT
	int value = 1;
	s.setsockopt(IPPROTO_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPPROTO_IPV6
#endif

	std::auto_ptr<SockAddr::SockAddr> bind_addr(
		SockAddr::translate("0.0.0.0", listen_port) );

	s.bind(*bind_addr);
	s.listen(10);
	std::cout << "Listening on " << bind_addr->string() << std::endl;

	std::auto_ptr<SockAddr::SockAddr> client_addr;
	Socket con = s.accept(&client_addr);
	std::cout << "Connection established from " << client_addr->string() << std::endl;

	std::auto_ptr<SockAddr::SockAddr> server_addr;
	server_addr = con.getsockname();
	std::cout << "Connection established to   " << server_addr->string() << std::endl;

	con.send("Hello world\n");
	std::cout << "Received:\n" << con.recv();

	std::cout << "Exiting...\n";
	return 0;
}
