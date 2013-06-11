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

#define SIZEOF_LONG 8
#define SIZEOF_INT 4
#define MULTI_PROCESSES  1
#define MULTI_THREADING  2

	#define DEFAULT_PORT 21590
	#define MAX_DB_PATH_LENGTH 1023

	//Message headers:
	#define DATA 0x44415441
	#define OPEN 0x4F50454E
	#define HERE 0x48455245
	#define DONE 0x444F4E45
	#define DBOK 0x44424F4B
	#define INFO 0x494E464F
	#define NODB 0x4E4F4442
	#define EXML 0x45584D4C
	#define SLAVE_REPLY 0x12345678
	#define SLAVE_DONE 0x87654321

	//Error Codes
	#define ERROR_STREAM_OPEN_FAILED -3
	#define ERROR_EXPECTED_OPEN_COMMAND -77
	#define ERROR_DB_OPEN_FAILED -4
	#define ERROR_UNKNOWN_COMMAND -7
	#define ERROR_PATH_TOO_LONG -9
	#define ERROR_INVALID_PARAMETERS -99

}
#endif /* CONSTANTS_H_ */
