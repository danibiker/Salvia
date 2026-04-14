#include "gui.h"


	std::string get_string(const std::wstring & s, std::string & d)
	{
		char buffer[256];
		const wchar_t * cs = s.c_str();
		wcstombs ( buffer, cs, sizeof(buffer) );

		d = buffer;

		return d;
	}

	std::wstring get_wstring(const std::string & s, std::wstring & d)
	{
		wchar_t buffer[256];
		const char * cs = s.c_str();
		mbstowcs ( buffer, cs, sizeof(buffer) );

		d = buffer;

		return d;
	}

	std::wstring string2wstring(const std::string& s)
	{
	   int len;
	   int slength = (int)s.length() + 1;
	   len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
	   wchar_t* buf = new wchar_t[len];
	   MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	   std::wstring r(buf);
	   delete[] buf;
	   return r;
	
}