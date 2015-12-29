/*************************************************************************
 ** Copyright (C) 2014 Jan Pedersen <jp@jp-embedded.com>
 ** 
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 ** 
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 ** 
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#ifndef __syslog
#define __syslog

#include <iostream>
#include <streambuf>
#include <string>
namespace csyslog {
#include <syslog.h>
}

namespace syslog
{
	struct level {
		enum pri {
			emerg   = LOG_EMERG, 	// A panic condition
			alert   = LOG_ALERT, 	// A condition that should be corrected
			critical= LOG_CRIT, 	// Critical condition, e.g, hard device error
			error   = LOG_ERR, 	// Errors
			warning = LOG_WARNING, 	// Warning messages
			notice  = LOG_NOTICE, 	// Possibly be handled specially
			info    = LOG_INFO, 	// Informational
			debug   = LOG_DEBUG 	// For debugging program
		};
	};

	class streambuf : public std::streambuf
	{
		std::string _buf;
		int _level;

		public:

		streambuf() : _level(level::debug)	{ }
		void level(int level)			{ _level = level; }

		protected:

		int sync()
		{
			if (_buf.size()) {
				csyslog::syslog(_level, "%s", _buf.c_str());
				_buf.erase();
			}	
			return 0;
		}

		int_type overflow(int_type c)
		{
			if(c == traits_type::eof()) sync();
			else _buf += static_cast<char>(c);
			return c;
		}

	};

	class ostream : public std::ostream
	{
		streambuf _logbuf;
		public:
		ostream() : std::ostream(&_logbuf) {}
		ostream& operator<<(const level::pri lev) { _logbuf.level(lev); return *this; }
	};

	class redirect
	{
		ostream dst;
		std::ostream &src;
		std::streambuf * const sbuf;

		public:
		redirect(std::ostream & src) : src(src), sbuf(src.rdbuf(dst.rdbuf())) { dst << (&src == &std::cout ? level::info : level::error); }
		~redirect() { src.rdbuf(sbuf); }
	};
}

#endif



