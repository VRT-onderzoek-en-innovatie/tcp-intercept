#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

#include "SockAddr.hxx"

class Socket {
private:
	int m_socket;

public:
	/**
	 * Take responsibility of a socket
	 * Will close when this object is destroyed
	 */
	Socket(int const socket = -1) throw() : m_socket(socket) {}

	~Socket() throw() { this->reset(); }

	/**
	 * Release responsibility of the socket
	 * The socket FD is returned.
	 */
	int release() throw() { int tmp = m_socket; m_socket = -1; return tmp; }

	void reset(int const socket = -1) { if( m_socket != -1 ) close(m_socket); m_socket = socket; }
	Socket & operator =(Socket &rhs) { this->reset( rhs.m_socket ); rhs.release(); return *this; }
	Socket & operator =(Socket rhs) { this->reset( rhs.m_socket ); rhs.release(); return *this; }

	/*
	 * Factory methods
	 */
	static Socket socket(int const domain, int const type, int const protocol) throw(std::runtime_error);
	static Socket accept(int const socket, struct sockaddr *address, socklen_t *address_len) throw(std::runtime_error);

	operator int() throw() { return m_socket; }

	void bind(struct sockaddr const *address, socklen_t const address_len) throw(std::runtime_error);
	void bind(SockAddr::SockAddr const &addr) throw(std::runtime_error) {
		return this->bind(static_cast<struct sockaddr const*>(addr), addr.addr_len()); }

	void connect(struct sockaddr const *address, socklen_t const address_len) throw(std::runtime_error);
	void connect(SockAddr::SockAddr const &addr) throw(std::runtime_error) {
		return this->connect(static_cast<struct sockaddr const*>(addr), addr.addr_len()); }
	
	void listen(int const backlog) throw(std::runtime_error);

	/**
	 * Accept a new connection on this socket (must be in listening mode)
	 * The new socket FD is returned
	 * if client_address is not NULL, the target auto_ptr is reassigned to
	 * own the SockAddr of the client.
	 */
	Socket accept(std::auto_ptr<SockAddr::SockAddr> *client_address) throw(std::runtime_error);

	std::auto_ptr<SockAddr::SockAddr> getsockname() const throw(std::runtime_error);

	std::string recv(size_t const max_length = 4096) throw(std::runtime_error);
	void send(std::string const &data) throw(std::runtime_error);

	void setsockopt(int const level, int const optname, void *optval, socklen_t optlen) throw(std::runtime_error);
	void set_reuseaddr(bool state = true) throw(std::runtime_error) {
		int optval = state;
		return this->setsockopt(SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	/**
	 * Get or set the non-blocking state of the socket
	 * Both functions return the non-blocking state at the moment of the call
	 * (i.e. before it is changed)
	 */
	bool non_blocking() throw(std::runtime_error);
	bool non_blocking(bool new_state) throw(std::runtime_error);
};

#endif // __SOCKET_HPP__
