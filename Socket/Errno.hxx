#ifndef __ERRNO_HXX_INCLUDED__
#define __ERRNO_HXX_INCLUDED__

#include <stdexcept>
#include <errno.h>

class Errno : public std::runtime_error {
public:
	Errno(std::string what, int errno_value) throw();
	virtual const char* what() const throw();

	virtual int error_number() const throw() { return m_errno; }

protected:
	int m_errno;

};

#endif // __ERRNO_HXX_INCLUDED__
