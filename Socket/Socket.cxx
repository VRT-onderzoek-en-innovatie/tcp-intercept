#include "Socket.hxx"
#include <sstream>
#include <errno.h>
#include <fcntl.h>

Socket Socket::socket(int const domain, int const type, int const protocol) throw(Errno) {
	int s = ::socket(domain, type, protocol);
	if( s == -1 ) {
		throw Errno("Could not create socket", errno);
	}
	return Socket(s);
}

void Socket::bind(struct sockaddr const *address, socklen_t const address_len) throw(Errno) {
	if( ::bind(m_socket, address, address_len) == -1 ) {
		throw Errno("Could not bind", errno);
	}
}

void Socket::connect(struct sockaddr const *address, socklen_t const address_len) throw(Errno) {
	if( ::connect(m_socket, address, address_len) ) {
		throw Errno("Could not connect", errno);
	}
}

void Socket::listen(int const backlog) throw(Errno) {
	if( ::listen(m_socket, backlog) == -1 ) {
		throw Errno("Could not listen", errno);
	}
}

Socket Socket::accept(int const socket, struct sockaddr *address, socklen_t *address_len) throw(Errno) {
	int s = ::accept(socket, address, address_len);
	if( s == -1 ) {
		throw Errno("Could not accept()", errno);
	}
	return Socket(s);
}

Socket Socket::accept(std::auto_ptr<SockAddr::SockAddr> *client_address) throw(Errno) {
	struct sockaddr_storage a;
	socklen_t a_len = sizeof(a);
	Socket s( accept(m_socket, reinterpret_cast<sockaddr*>(&a), &a_len) );
	if( client_address != NULL ) {
		*client_address = SockAddr::create(&a);
	}
	return s;
}

std::auto_ptr<SockAddr::SockAddr> Socket::getsockname() const throw(Errno) {
	struct sockaddr_storage a;
	socklen_t a_len = sizeof(a);
	if( -1 == ::getsockname(m_socket, reinterpret_cast<sockaddr*>(&a), &a_len) ) {
		throw Errno("Could not getsockname()", errno);
	}
	std::auto_ptr<SockAddr::SockAddr> addr( SockAddr::create(&a) );
	return addr;
}

std::auto_ptr<SockAddr::SockAddr> Socket::getpeername() const throw(Errno) {
	struct sockaddr_storage a;
	socklen_t a_len = sizeof(a);
	if( -1 == ::getpeername(m_socket, reinterpret_cast<sockaddr*>(&a), &a_len) ) {
		throw Errno("Could not getpeername()", errno);
	}
	std::auto_ptr<SockAddr::SockAddr> addr( SockAddr::create(&a) );
	return addr;
}

std::string Socket::recv(size_t const max_length ) throw(Errno) {
	std::auto_ptr<char> buf( new char[max_length] );
	ssize_t length = ::recv(m_socket, buf.get(), max_length, 0);
	if( length == -1 ) {
		throw Errno("Could not recv()", errno);
	}
	return std::string(buf.get(), length); // TODO: avoid needless copy of data
}

ssize_t Socket::send(char const *data, size_t len) throw(Errno) {
	ssize_t rv = ::send(m_socket, data, len, 0);
	if( rv == -1 ) {
		throw Errno("Could not send()", errno);
	}
	return rv;
}
void Socket::send(std::string const &data) throw(Errno,std::runtime_error) {
	ssize_t rv = this->send(data.data(), data.length());
	if( rv != (signed)data.length() ) {
		std::ostringstream e;
		e << "Could not send(): Not enough bytes sent: " << rv << " < " << data.length();
		throw std::runtime_error(e.str());
	}
}

void Socket::shutdown(int how) throw(Errno) {
	if( ::shutdown(m_socket, how) == -1 ) {
		throw Errno("Could not shutdown()", errno);
	}
}

void Socket::setsockopt(int const level, int const optname, const void *optval, socklen_t optlen) throw(Errno) {
	if( ::setsockopt(m_socket, level, optname, optval, optlen) == -1 ) {
		throw Errno("Could not setsockopt()", errno);
	}
}

void Socket::getsockopt(int const level, int const optname, void *optval, socklen_t *optlen) throw(Errno) {
	if( ::getsockopt(m_socket, level, optname, optval, optlen) == -1 ) {
		throw Errno("Could not getsockopt()", errno);
	}
}

bool Socket::non_blocking() throw(Errno) {
	int flags = fcntl(m_socket, F_GETFL);
	if( flags == -1 ) {
		throw Errno("Could not fcntl(, F_GETFL)", errno);
	}
	return flags | O_NONBLOCK;
}
bool Socket::non_blocking(bool new_state) throw(Errno) {
	int flags = fcntl(m_socket, F_GETFL);
	if( flags == -1 ) {
		throw Errno("Could not fcntl(, F_GETFL)", errno);
	}
	bool non_block_state = flags | O_NONBLOCK;

	if( new_state ) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}

	int rv = fcntl(m_socket, F_SETFL, flags);
	if( rv == -1 ) {
		throw Errno("Could not fcntl(, F_SETFL)", errno);
	}

	return non_block_state;
}
