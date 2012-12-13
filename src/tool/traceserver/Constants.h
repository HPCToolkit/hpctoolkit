/*
 * Constants.h
 *
 *  Created on: Jul 7, 2012
 *      Author: pat2
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_
namespace TraceviewerServer
{
	class Constants
	{
	public:
		static const int MULTI_PROCESSES = 1;
		static const int MULTI_THREADING = 2;

		static const int SIZEOF_LONG = 8;
		static const int SIZEOF_INT = 4;

		static const char* XML_FILENAME()
		{
			return "experiment.xml";
		}
		static const char* TRACE_FILENAME()
		{
			return "experiment.mt";
		}

		static const int DATA = 0x44415441;
		static const int OPEN = 0x4F50454E;
		static const int HERE = 0x48455245;
		static const int DONE = 0x444F4E45;
		static const int DBOK = 0x44424F4B;
		static const int INFO = 0x494E464F;
		static const int NODB = 0x4E4F4442;
		static const int EXML = 0x45584D4C;
		static const int SLAVE_REPLY = 0x12345678;
		static const int SLAVE_DONE = 0x87654321;
	};
}
#endif /* CONSTANTS_H_ */
