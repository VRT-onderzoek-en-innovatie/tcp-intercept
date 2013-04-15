#ifndef TIMESTAMPLOG_H__
#define TIMESTAMPLOG_H__

#include <ostream>
#include <sstream>

class TimestampLogBuffer : public std::basic_streambuf<char, std::char_traits<char> > {
public:
	TimestampLogBuffer( std::ostream& output );

protected:
	virtual int overflow(int c);
	virtual int sync();

private:
	std::ostream& m_output;
	std::ostringstream m_buffer;
};

class TimestampLog : public std::basic_ostream<char, std::char_traits<char> > {
private:
	TimestampLogBuffer m_buf;
public:
	TimestampLog( std::ostream& output ) :
		std::basic_ostream<char, std::char_traits<char> >( &m_buf ),
		m_buf( output )
		{}
};

#endif /* TIMESTAMPLOG_H__ */
