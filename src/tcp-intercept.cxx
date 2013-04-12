#include <iostream>
#include "../Socket/Socket.hxx"
#include <netinet/in.h>

int main(int argc, char* argv[]) {
	Socket s = Socket::socket(AF_INET, SOCK_STREAM, 0);

	int value = 1;
	s.setsockopt(IPPROTO_IP, IP_TRANSPARENT, &value, sizeof(value)); // TODO: IPv6

	std::auto_ptr<SockAddr::SockAddr> bind_addr(
		SockAddr::translate("0.0.0.0", 5000) );

	s.bind(*bind_addr);

	s.listen(10);
	Socket con = s.accept(NULL);

	con.send("Hello world\n");
	std::cout << "Received:\n" << con.recv();

	std::cout << "Exiting...\n";
	return 0;
}
