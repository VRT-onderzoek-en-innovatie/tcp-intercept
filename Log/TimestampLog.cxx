#include "../config.h"

#include "TimestampLog.hxx"

#if TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
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
	time_t now_secs = time(NULL);
	struct tm *now = localtime( &now_secs );

	std::vector<char> date(25);
	int length = strftime(&date[0], date.size(), "%Y-%m-%dT%H:%M:%S%z", now);
	std::string date_str( &date[0], length );
	
	m_output << date_str << " " << m_buffer.str() << std::flush;
	m_buffer.str("");

	return 0;
}
