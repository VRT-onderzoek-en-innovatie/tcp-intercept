#include "output.hxx"
#include <string>
#include <sstream>
#include <iomanip>

std::string dec(int number, int total_digits) {
	std::ostringstream o;
	o << std::dec << std::setw(total_digits) << std::setfill('0') << number;
	return o.str();
}

std::string hex(int number) {
	std::ostringstream o;
	o << std::hex << std::setw(2) << std::setfill('0') << number;
	return o.str();
}
std::string hex(char num) {
	unsigned char temp = num;
	return hex( temp );
}

std::string bin(int bitmap) {
	std::string o;
	for( int i = 7; i>=0; i-- ) {
		o.append( ((bitmap >> i) & 0x01) ? "1" : "0" );
	}
	return o;
}

std::string format_fixed_point(float f, int decimals) {
	std::ostringstream o;
	o << std::setiosflags(std::ios::fixed) << std::setprecision(decimals) << f;
	return o.str();
}

int bitnum(int bitmap) {
	for( unsigned int i = 0; i < sizeof(bitmap)*8; i++ ) {
		if( (bitmap >> i) & 0x01 ) return i;
	}
	return -1;
}
