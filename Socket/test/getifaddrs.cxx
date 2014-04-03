#include <stdio.h>
#include "../SockAddr.hxx"

int main() {
	std::auto_ptr< boost::ptr_vector< SockAddr::SockAddr> > local_addrs
		= SockAddr::getifaddrs();
	for( typeof(local_addrs->begin()) i = local_addrs->begin(); i != local_addrs->end(); i++ ) {
		printf("%s\n", i->string().c_str() );
	}

	return 0;
}
