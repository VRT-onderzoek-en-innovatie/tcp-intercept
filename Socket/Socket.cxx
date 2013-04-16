#include "Socket.hxx"
#include <sstream>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

Socket Socket::socket(int const domain, int const type, int const protocol) throw(std::runtime_error) {
	int s = ::socket(domain, type, protocol);
	if( s == -1 ) {
		std::ostringstream e;
		e << "Could not create socket: " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	return Socket(s);
}

void Socket::bind(struct sockaddr const *address, socklen_t const address_len) throw(std::runtime_error) {
	if( ::bind(m_socket, address, address_len) == -1 ) {
		std::ostringstream e;
		e << "Could not bind: " << strerror(errno);
		throw std::runtime_error(e.str());
	}
}

void Socket::connect(struct sockaddr const *address, socklen_t const address_len) throw(std::runtime_error) {
	if( ::connect(m_socket, address, address_len) ) {
		std::ostringstream e;
		e << "Could not connect: " << strerror(errno);
		throw std::runtime_error(e.str());
	}
}

void Socket::listen(int const backlog) throw(std::runtime_error) {
	if( ::listen(m_socket, backlog) == -1 ) {
		std::ostringstream e;
		e << "Could not listen: " << strerror(errno);
		throw std::runtime_error(e.str());
	}
}

Socket Socket::accept(int const socket, struct sockaddr *address, socklen_t *address_len) throw(std::runtime_error) {
	int s = ::accept(socket, address, address_len);
	if( s == -1 ) {
		std::ostringstream e;
		e << "Could not accept(): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	return Socket(s);
}

Socket Socket::accept(std::auto_ptr<SockAddr::SockAddr> *client_address) throw(std::runtime_error) {
	struct sockaddr_storage a;
	socklen_t a_len = sizeof(a);
	Socket s( accept(m_socket, reinterpret_cast<sockaddr*>(&a), &a_len) );
	if( client_address != NULL ) {
		*client_address = SockAddr::create(&a);
	}
	return s;
}

std::auto_ptr<SockAddr::SockAddr> Socket::getsockname() const throw(std::runtime_error) {
	struct sockaddr_storage a;
	socklen_t a_len = sizeof(a);
	if( -1 == ::getsockname(m_socket, reinterpret_cast<sockaddr*>(&a), &a_len) ) {
		std::ostringstream e;
		e << "Could not getsockname(): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	std::auto_ptr<SockAddr::SockAddr> addr( SockAddr::create(&a) );
	return addr;
}

std::string Socket::recv(size_t const max_length ) throw(std::runtime_error) {
	std::auto_ptr<char> buf( new char[max_length] );
	ssize_t length = ::recv(m_socket, buf.get(), max_length, 0);
	if( length == -1 ) {
		std::ostringstream e;
		e << "Could not recv(): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	return std::string(buf.get(), length); // TODO: avoid needless copy of data
}

void Socket::send(std::string const &data) throw(std::runtime_error) {
	ssize_t rv = ::send(m_socket, data.data(), data.length(), 0);
	if( rv == -1 ) {
		std::ostringstream e;
		e << "Could not send(): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	if( rv != data.length() ) {
		std::ostringstream e;
		e << "Could not send(): Not enough bytes sent: " << rv << " < " << data.length();
		throw std::runtime_error(e.str());
	}
}

void Socket::setsockopt(int const level, int const optname, void *optval, socklen_t optlen) throw(std::runtime_error) {
	if( ::setsockopt(m_socket, level, optname, optval, optlen) == -1 ) {
		std::ostringstream e;
		e << "Could not setsockopt(): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
}

bool Socket::non_blocking() throw(std::runtime_error) {
	int flags = fcntl(m_socket, F_GETFL);
	if( flags == -1 ) {
		std::ostringstream e;
		e << "Could not fcntl(, F_GETFL): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	return flags | O_NONBLOCK;
}
bool Socket::non_blocking(bool new_state) throw(std::runtime_error) {
	int flags = fcntl(m_socket, F_GETFL);
	if( flags == -1 ) {
		std::ostringstream e;
		e << "Could not fcntl(, F_GETFL): " << strerror(errno);
		throw std::runtime_error(e.str());
	}
	bool non_block_state = flags | O_NONBLOCK;

	if( new_state ) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}

	int rv = fcntl(m_socket, F_SETFL, flags);
	if( rv == -1 ) {
		std::ostringstream e;
		e << "Could not fcntl(, F_SETFL): " << strerror(errno);
		throw std::runtime_error(e.str());
	}

	return non_block_state;
}
