#include "../config.h"

#include "TimestampLog.hxx"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <vector>
#include <string>

TimestampLogBuffer::TimestampLogBuffer( std::ostream& output ) :
	m_output( output )
{
	m_buffer.clear();
}

int TimestampLogBuffer::overflow(int c) {
	m_buffer << static_cast<char>(c);
	return std::char_traits<char>::not_eof(c);
}

int TimestampLogBuffer::sync() {
	struct timeval tv_now;
	std::string date_str;

	if( -1 == gettimeofday(&tv_now, NULL) ) {
		date_str = "gettimeofday() failed";
	} else {
		struct tm *now = localtime( &tv_now.tv_sec );
		std::vector<char> date(20);
		int length = strftime(&date[0], date.size(), "%Y-%m-%dT%H:%M:%S", now);
		date_str.assign( &date[0], length );

		std::ostringstream millisec;
		millisec << "000000" << tv_now.tv_usec;
		date_str.append(".");
		date_str.append(millisec.str().substr( millisec.str().length()-6 ));

		length = strftime(&date[0], date.size(), "%z", now);
		date_str.append( &date[0], length );
	}

	m_output << date_str << " " << m_buffer.str() << std::flush;
	m_buffer.str("");

	return 0;
}
