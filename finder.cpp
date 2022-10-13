
#include <bur/plctypes.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "Finder.h"
#ifdef __cplusplus
};
#endif
#include <iostream>
#include <string.h>

unsigned long bur_heap_size = 0xffff;

class outbuf : public std::streambuf {
	private:
	char *_front;
	char *_current;
	size_t _sz;
	bool rolled;

	public:
	outbuf(char *data, size_t sz);

	virtual int_type overflow(int_type c = traits_type::eof());
	void reset();
};

outbuf::outbuf(char *data, size_t sz) : _front(data), _current(data), _sz(sz) {
	// no buffering, overflow on every char
	setp(0, 0);
}
int outbuf::overflow(int_type c) {
	// add the char to wherever you want it, for example:

	if (_current - _front > _sz) {
		_current = _front;
		rolled = true;
	}

	switch (c) {
		case '\n':
			if (_current - _front < _sz) {
				(*_current) = 0;
			}
			_current = (char *)((((((UDINT)_current - (UDINT)_front) / 81) + 1) * 81) +
				(UDINT)_front);
			return c;
		default:
			(*_current) = c;
			_current++;
			break;
	}
	return c;
}
void outbuf::reset() {
	if (rolled) {
		rolled = false;
		memset((void *)_front, 0, _sz);
	} else {
		memset((void *)_front, 0, _current - _front);
	}
	_current = _front;
}

unsigned char finder(struct finder_typ *in, unsigned long pBuf,
unsigned long sBuf) {

	// Step through all states faster by looping 5 times
	int index;

	if (in->internal.obj == 0) {
		in->internal.obj = (UDINT*)new outbuf((char*)pBuf, sBuf);
	}
	outbuf * buf = (outbuf *)in->internal.obj;
	std::ostream out( buf );

	for (index = 0; index < 5; index++)

		switch (in->internal.state) {
			case 0:
				in->internal.directoryRead.enable = 0;
				// CurrentFileIndex=0; This variable is never used again.
				in->out.done = 			0;
				if( strcmp( in->in.cwd, in->internal.cwd) != 0 || in->in.refresh ){
					strncpy(in->internal.cwd, in->in.cwd, sizeof(in->internal.cwd));

					std::string s1(in->in.cwd);
					s1 = s1.substr(0, s1.find_last_of("\\/"));
					strncpy(in->internal.path, s1.c_str(), sizeof(in->internal.cwd));					

					in->out.updating = 		1;
					in->internal.state = 	1;					
					buf->reset();					
				}
				break;

			case 1:
				in->out.status = ERR_FUB_BUSY;
				in->internal.directoryOpen.enable = 1;
				in->internal.directoryOpen.pDevice = (UDINT) & (in->in.filedevice);
				in->internal.directoryOpen.pName = (UDINT)&in->internal.path;
				DirOpen(&in->internal.directoryOpen);
				if (in->internal.directoryOpen.status < ERR_FUB_ENABLE_FALSE) {
					if (in->internal.directoryOpen.status == ERR_OK) {
						in->internal.ident = in->internal.directoryOpen.ident;
						in->internal.directoryOpen.enable = 0;
						in->internal.state = in->internal.state + 1;
						in->internal.comma = 0;
						out << "{";
						out << "\"path\":\"" << in->internal.path << "\",";
						out << "\"files\":[";
					} else {
						out << "{";
						out << "\"path\":\"" << in->internal.path << "\",";
						out << "\"files\":[";
						out << "{";
						out << "\"name\":\"./\",";
						out << "\"size\":\"0\",";
						out << "\"date\":\"--\"";
						out << "},";
						out << "{";
						out << "\"name\":\"../\",";
						out << "\"size\":\"0\",";
						out << "\"date\":\"--\"";
						out << "},";
						out << "{";
						out << "\"name\":\"" << "Error: " << in->internal.directoryOpen.status << "\",";
						out << "\"size\":\"" << 0 << "\",";
						out << "\"date\":\"" << "--" << "\"";
						out << "}";
						out << "]}";
						in->out.status = in->internal.directoryOpen.status;
						in->internal.state = 	0;
						in->out.updating = 		0;
						in->out.done = 			1;						
						return 0;

					}
				} else {
					continue;
					};
				break;

			case 2:
				// read directory
				in->internal.directoryRead.enable = 1;
				in->internal.directoryRead.ident = in->internal.ident;
				in->internal.directoryRead.pData = (UDINT) & (in->internal.dirData);
				in->internal.directoryRead.data_len = sizeof(in->internal.dirData);
				DirReadEx(&in->internal.directoryRead);
				if (in->internal.directoryRead.status < ERR_FUB_ENABLE_FALSE) {
					if (in->internal.directoryRead.status == ERR_OK) {
						in->internal.directoryRead.enable = 0;
						in->internal.state = in->internal.state + 1;
					} else if (in->internal.directoryRead.status = fiERR_NO_MORE_ENTRIES) {
						out << "]}";
						in->out.updating = 		0;
						in->out.done = 			1;
						in->out.status = ERR_OK;
						in->internal.state = 5;
					} else {
						if (in->internal.directoryRead.status = 20799) {
							in->out.status = FileIoGetSysError();
						} else {
							in->out.status = in->internal.directoryRead.status;
							};
						in->internal.state = 10;
						};
				} else {
					continue;
					};
				break;

			case 3:
				if( in->internal.comma ){
					out << ",";
				}
				in->internal.comma = 1;
				out << "{";
				out << "\"name\":\"" << in->internal.dirData.Filename;
				if(in->internal.dirData.Mode) out << "/";
				out << "\",";
				out << "\"size\":\"" << in->internal.dirData.Filelength << "\",";
				STRING timeStamp[40];
				GenerateTimestamp( in->internal.dirData.Date, (UDINT)timeStamp, sizeof(timeStamp));
				out << "\"date\":\"" << timeStamp << "\"";
				out << "}";
				// Add the item to the data
				in->internal.state = in->internal.state + 1;
				break;

			case 4:

				//      if (ListFileIndex = MaxValues) {
				//        in->out.status = 		ERR_OK;
				//        NumFiles = ListFileIndex;
				//        in->internal.state = 	5;
				//      } else {
				in->internal.state = 2;
				//      };
				break;

			case 5:
				in->internal.directoryClose.enable = 1;
				in->internal.directoryClose.ident = in->internal.ident;
				DirClose(&in->internal.directoryClose);
				if (in->internal.directoryClose.status == ERR_OK) {
					in->internal.directoryClose.enable = 0;
					in->internal.state = 6;
					};
				break;

			case 10:
				//      UpdateTimer.IN = 1;
				//      if (UpdateTimer.Q || iNameFilter<> NameFilter OR Refresh) {
				//        Refresh = 0;
				in->internal.state = 0;
				return 0;
				//      }
				break;
			default:
				//      Initialized = 1;
				//      Updating = 0;
				//      UpdateTimer.IN = 1;
				//      if (UpdateTimer.Q OR iNameFilter<> NameFilter OR Refresh) {
				//        Refresh = 0;
				in->internal.state = 0;
				return 0;
			//      }
		}
}

//	UpdateTimer.PT = T #60s;
//	UpdateTimer();
//	UpdateTimer.IN = 0;
