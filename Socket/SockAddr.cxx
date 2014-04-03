#include "SockAddr.hxx"
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sstream>

namespace SockAddr {

bool SockAddr::operator ==(SockAddr const &b) const throw() {
	if( this->addr_family() != b.addr_family() ) return false;
	switch( this->addr_family() ) {
	case AF_INET:	return this->operator==( dynamic_cast<Inet4 const &>(b) );
	case AF_INET6:	return this->operator==( dynamic_cast<Inet6 const &>(b) );
	}
}

std::auto_ptr<SockAddr> create(struct sockaddr_storage const *addr) throw(std::invalid_argument) {
	if( addr == NULL ) throw std::invalid_argument("Empty address");

	switch( addr->ss_family ) {
	case AF_INET:
		return std::auto_ptr<SockAddr>( new Inet4( *reinterpret_cast<const struct sockaddr_in*>(addr) ) );
	case AF_INET6:
		return std::auto_ptr<SockAddr>( new Inet6( *reinterpret_cast<const struct sockaddr_in6*>(addr) ) );
	default:
		throw(std::invalid_argument("Unknown address family"));
	}
}

std::auto_ptr<SockAddr> translate(std::string const &host, unsigned short const port) throw(std::invalid_argument) {
	bool looks_like_v4 = ( host.find('.') != std::string::npos );
	bool looks_like_v6 = ( host.find(':') != std::string::npos );
	if( ( !looks_like_v4 && !looks_like_v6 ) || ( looks_like_v4 && looks_like_v6 ) ) {
		std::string e("\"");
		e.append(host);
		e.append("\" does not look like an IP address");
		throw std::invalid_argument(e);
	}
	if( looks_like_v4 ) {
		struct sockaddr_in sa;
		bzero(&sa, sizeof(sa));
#ifdef SOCKADDR_HAS_LEN_FIELD
		sa.sin_len = sizeof(sa);
#endif
		sa.sin_family = AF_INET;
		if( inet_pton(AF_INET, host.c_str(), &sa.sin_addr) <= 0 ) {
			std::string e("\"");
			e.append(host);
			e.append("\" is not an IPv4 address");
			throw std::invalid_argument(e);
		}
		sa.sin_port = htons(port);

		return create(&sa);
	} else { // looks_like_v6
		struct sockaddr_in6 sa;
		bzero(&sa, sizeof(sa));
#ifdef SOCKADDR_HAS_LEN_FIELD
		sa.sin6_len = sizeof(sa);
#endif
		sa.sin6_family = AF_INET6;
		if( inet_pton(AF_INET6, host.c_str(), &sa.sin6_addr) <= 0 ) {
			std::string e("\"");
			e.append(host);
			e.append("\" is not an IPv6 address");
			throw std::invalid_argument(e);
		}
		sa.sin6_port = htons(port);

		return create(&sa);
	}
}

std::auto_ptr< boost::ptr_vector< SockAddr > > resolve(std::string const &host, std::string const &port, int const family, int const socktype, int const protocol, bool const v4_mapped) {
	struct addrinfo hints;
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_protocol = protocol;
	hints.ai_flags = AI_ADDRCONFIG;
	if( v4_mapped ) hints.ai_flags = AI_V4MAPPED;

	int hstart = 0, hend = host.length();
	if( host[0] == '[' && host[ host.length()-1 ] == ']' ) {
		hstart = 1;
		hend -= 2;
		hints.ai_flags |= AI_NUMERICHOST;
	}

	int pstart = 0, pend = port.length();
	if( port[0] == '[' && port[ port.length()-1 ] == ']' ) {
		pstart = 1;
		pend -= 2;
		hints.ai_flags |= AI_NUMERICSERV;
	}

	struct addrinfo *res;
	int rv = getaddrinfo(host.substr(hstart,hend).c_str(), port.substr(pstart,pend).c_str(), &hints, &res);
	if( rv != 0 ) {
		std::ostringstream e;
		e << "Could not getaddrinfo(\"" << host << "\", \"" << port << "\"): ";
		e << gai_strerror(rv);
		throw std::runtime_error(e.str());
	}

	struct addrinfo *p = res;
	std::auto_ptr< boost::ptr_vector< SockAddr > > ret( new boost::ptr_vector< SockAddr> );
	while( p != NULL ) {
		std::auto_ptr<SockAddr> a( create( reinterpret_cast<sockaddr_storage*>(p->ai_addr) ) );
		ret->push_back( a.release() );

		p = p->ai_next;
	}

	freeaddrinfo(res);

	return ret;
}

Inet4::Inet4(struct sockaddr_in const &addr) throw() {
	bzero(&m_addr, sizeof(m_addr));
#ifdef SOCKADDR_HAS_LEN_FIELD
	m_addr.sin_len = sizeof(m_addr);
#endif
	m_addr.sin_family = addr.sin_family;
	m_addr.sin_port = addr.sin_port;
	m_addr.sin_addr.s_addr = addr.sin_addr.s_addr;
}

bool Inet4::operator ==(Inet4 const &b) const throw() {
	return (m_addr.sin_port == b.m_addr.sin_port) &&
	       (m_addr.sin_addr.s_addr == b.m_addr.sin_addr.s_addr);
}

std::string Inet4::string() const throw(Errno) {
	std::string a("[");
	{
		char address[INET_ADDRSTRLEN];
		if( inet_ntop(AF_INET, &m_addr.sin_addr, address, sizeof(address)) == NULL ) {
			throw Errno("Could not convert IPv4 address to text", errno);
		}
		a.append(address);
	}

	a.append("]:");

	{
		char port[6];
		int rv = snprintf(port, sizeof(port), "%d", ntohs(m_addr.sin_port));
		assert( rv < (signed)sizeof(port) ); // unsigned shorts are always 5 digits or less
		if( rv == -1 ) {
			throw Errno("Could not convert IPv4 address to text", errno);
		}
		a.append(port);
	}
	return a;
}

Inet6::Inet6(struct sockaddr_in6 const &addr) throw() {
	bzero(&m_addr, sizeof(m_addr));
#ifdef SOCKADDR_HAS_LEN_FIELD
	m_addr.sin6_len = sizeof(m_addr);
#endif
	m_addr.sin6_family = AF_INET6;
	m_addr.sin6_port = addr.sin6_port;
	m_addr.sin6_flowinfo = addr.sin6_flowinfo;
	for( unsigned int i = 0; i < sizeof(addr.sin6_addr.s6_addr); i++) m_addr.sin6_addr.s6_addr[i] = addr.sin6_addr.s6_addr[i];
	m_addr.sin6_scope_id = addr.sin6_scope_id;
}

bool Inet6::operator ==(Inet6 const &b) const throw() {
	if( m_addr.sin6_port != b.m_addr.sin6_port ) return false;
	for( unsigned int i = 0; i < sizeof(m_addr.sin6_addr.s6_addr); i++) {
		if( m_addr.sin6_addr.s6_addr[i] != b.m_addr.sin6_addr.s6_addr[i] ) return false;
	}
	return true;
}

std::string Inet6::string() const throw(std::runtime_error) {
	std::string a("[");
	{
		char address[INET6_ADDRSTRLEN];
		if( inet_ntop(AF_INET6, &m_addr.sin6_addr, address, sizeof(address)) == NULL ) {
			throw Errno("Could not convert IPv6 address to text", errno);
		}
		a.append(address);
	}

	a.append("]:");

	{
		char port[6];
		int rv = snprintf(port, sizeof(port), "%d", ntohs(m_addr.sin6_port));
		assert( rv < (signed)sizeof(port) ); // unsigned shorts are always 5 digits or less
		if( rv == -1 ) {
			throw Errno("Could not convert IPv6 address to text", errno);
		}
		a.append(port);
	}
	return a;
}

} // namespace
