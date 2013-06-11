/*
 * Communication.h
 *
 *  Created on: Jul 19, 2012
 *      Author: pat2
 */

#ifndef MPICOMMUNICATION_H_
#define MPICOMMUNICATION_H_

namespace TraceviewerServer
{

	class MPICommunication
	{
	public:
		static const int SOCKET_SERVER = 0; //The rank of the node that is doing all the socket comm

		typedef struct
		{
			char Path[1024];
		} open_file_command;
		typedef struct
		{
			int processStart;
			int processEnd;
			double timeStart;
			double timeEnd;
			int verticalResolution;
			int horizontalResolution;
		} get_data_command;
		typedef struct
		{
			Long minBegTime;
			Long maxEndTime;
			int headerSize;
		} more_info_command;

		typedef struct
		{
			int Command;
			union
			{

				open_file_command ofile;
				get_data_command gdata;
				more_info_command minfo;
			};
		} CommandMessage;

		typedef struct
		{
			int RankID;
			int TraceLinesSent;
		} DoneMessage;

		typedef struct
		{
			int RankID;

			int Line;
			int Entries;
			double Begtime;
			double Endtime;
			int CompressedSize;//In Bytes
		} DataHeader;

		typedef struct
		{
			int Tag;
			union
			{
				DataHeader Data;
				DoneMessage Done;
			};
		} ResultMessage;

	};

} /* namespace TraceviewerServer */
#endif /* MPICOMMUNICATION_H_ */
