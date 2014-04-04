#ifndef __SOCKADDR_HPP__
#define __SOCKADDR_HPP__

#include "../config.h"
#include <netinet/in.h>
#include <memory>
#include <string>
#include <boost/ptr_container/ptr_vector.hpp>

#include "Errno.hxx"

namespace SockAddr {

class SockAddr {
protected:
	SockAddr() throw() {}

public:
	virtual ~SockAddr() throw() {};

	virtual operator struct sockaddr const*() const throw() =0;
	virtual socklen_t const addr_len() const throw() =0;

	virtual bool operator ==(SockAddr const &b) const throw() {
		return this->port_equal(b) && this->address_equal(b);
	}
	virtual bool address_equal(SockAddr const &b) const throw();
	virtual bool port_equal(SockAddr const &b) const throw() {
		return this->port_number() == b.port_number();
	}

	virtual std::string string() const throw(std::runtime_error) = 0;

	virtual int const proto_family() const throw() =0;
	virtual int const addr_family() const throw() =0;

	virtual int const port_number() const throw() =0;

	virtual bool is_any() const throw() =0;
	virtual bool is_loopback() const throw() =0;
};

std::auto_ptr<SockAddr> create(struct sockaddr_storage const *addr) throw(std::invalid_argument);
inline std::auto_ptr<SockAddr> create(struct sockaddr_in const *addr) throw(std::invalid_argument) {
	return create( reinterpret_cast<struct sockaddr_storage const*>(addr) );
}
inline std::auto_ptr<SockAddr> create(struct sockaddr_in6 const *addr) throw(std::invalid_argument) {
	return create( reinterpret_cast<struct sockaddr_storage const*>(addr) );
}

std::auto_ptr<SockAddr> translate(std::string const &host, unsigned short const port) throw(std::invalid_argument);

std::auto_ptr< boost::ptr_vector< SockAddr > > resolve(std::string const &host, std::string const &service, int const family = 0, int const socktype = 0, int const protocol = 0, bool const v4_mapped = false);

std::auto_ptr< boost::ptr_vector< SockAddr > > getifaddrs();


class Inet4 : public SockAddr {
protected:
	struct sockaddr_in m_addr;

public:
	Inet4(struct sockaddr_in const &addr) throw();
	virtual ~Inet4() throw() {};

	socklen_t const addr_len() const throw() { return sizeof(m_addr); }

	virtual operator struct sockaddr const*() const throw() { return reinterpret_cast<struct sockaddr const*>(&m_addr); }
	virtual bool address_equal(Inet4 const &b) const throw() {
		return (m_addr.sin_addr.s_addr == b.m_addr.sin_addr.s_addr);
	}

	virtual std::string string() const throw(Errno);

	virtual int const proto_family() const throw() { return PF_INET; }
	virtual int const addr_family() const throw() { return AF_INET; }

	virtual int const port_number() const throw() { return ntohs(m_addr.sin_port); }

	virtual bool is_any() const throw() { return m_addr.sin_addr.s_addr == INADDR_ANY; }
	virtual bool is_loopback() const throw() { return m_addr.sin_addr.s_addr == INADDR_LOOPBACK; }
};


class Inet6 : public SockAddr {
protected:
	struct sockaddr_in6 m_addr;

public:
	Inet6(struct sockaddr_in6 const &addr) throw();
	virtual ~Inet6() throw() {}

	socklen_t const addr_len() const throw() { return sizeof(m_addr); }

	virtual operator struct sockaddr const*() const throw() { return reinterpret_cast<struct sockaddr const*>(&m_addr); }
	virtual bool address_equal(Inet6 const &b) const throw() {
		for( unsigned int i = 0; i < sizeof(m_addr.sin6_addr.s6_addr); i++) {
			if( m_addr.sin6_addr.s6_addr[i] != b.m_addr.sin6_addr.s6_addr[i] ) return false;
		}
		return true;
	}

	virtual std::string string() const throw(std::runtime_error);

	virtual int const proto_family() const throw() { return PF_INET6; }
	virtual int const addr_family() const throw() { return AF_INET6; }

	virtual int const port_number() const throw() { return ntohs(m_addr.sin6_port); }

	virtual bool is_any() const throw() { return memcmp(&m_addr.sin6_addr, &in6addr_any, 16); }
	virtual bool is_loopback() const throw() { return memcmp(&m_addr.sin6_addr, &in6addr_loopback, 16); }
};

} // namespace

#endif // __SOCKADDR_HPP__
