#pragma warning(disable:4996)
/**
 *  This class is developed to get the local time or UTC time with precision.
 *  In particular, the offset is returned based on the Network Time Protocol (NTP),
 *  where ntp servers are used to sync the clock and include the timestamp. If accuracy
 *  is not an issue, then use the timestamp.cpp (i.e. locally).
 *
 *  Ioannis Selinis 2019 (5GIC, University of Surrey)
 */

#ifndef UNICODE
#define UNICODE
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN

#include <Ws2tcpip.h>
#include <stdio.h>

 // Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

 /******************************************************************************
  * Project Headers
  *****************************************************************************/
#include "NtpClient.h"
#include <iostream>    // Needed to perform IO operations

  /******************************************************************************
  * System Headers
  *****************************************************************************/
#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <wchar.h>
#include <sstream>
#include <ctime>
#include <Windows.h>

  ////
#include <filesystem>
#include <string>       // std::string
#include <timeapi.h>
#include <bitset>
#include <iomanip>
////

using namespace std;

/******************************************************************************
* Preprocessor Directives and Macros
*****************************************************************************/
#define NTP_PORT (123)
#define SERVICE_NAME ("npt")
#define NTP_SERVER ("pool.ntp.org") //pool.ntp.org time-a-g.nist.gov time.google.com
#define NTP_MSG_SIZE (48) // in bytes
#define NTP_MSG_OFFSET_ROOT_DELAY (4)
#define NTP_MSG_OFFSET_ROOT_DISPERSION (8)
#define NTP_MSG_OFFSET_REFERENCE_IDENTIFIER (12)
#define NTP_MSG_OFFSET_REFERENCE_TIMESTAMP (16)
#define NTP_MSG_OFFSET_ORIGINATE_TIMESTAMP (24)
#define NTP_MSG_OFFSET_RECEIVE_TIMESTAMP (32)
#define NTP_MSG_OFFSET_TRANSMIT_TIMESTAMP (40)

constexpr auto SECONDS_SINCE_FIRST_EPOCH = (2208988800UL); // Seconds from 1/1/1900 00.00 to 1/1/1970 00.00;
//constexpr auto NTP_SCALE_FRAC = (4294967296UL);

/******************************************************************************
* Class Member Function Definitions
*****************************************************************************/

NtpClient::NtpClient()
	: m_clockOffset(0),
	  m_originateTimestamp(0)
{
}

NtpClient::~NtpClient()
{
}

void
NtpClient::dns_lookup(const char* host, sockaddr_in* out)
{
	struct addrinfo* result;
	int ret = getaddrinfo(host, SERVICE_NAME, NULL, &result);
	for (struct addrinfo* p = result; p; p = p->ai_next)
	{
		if (p->ai_family != AF_INET)
			continue;

		memcpy(out, p->ai_addr, sizeof(*out));
	}
}

void
NtpClient::SetClockOffset(int clockOffset)
{
	m_clockOffset = clockOffset;
}

int
NtpClient::GetClockOffset(void)
{
	return m_clockOffset;
}

void 
NtpClient::gettimeofday(struct timeval* tp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
}

void 
NtpClient::convert_unix_to_ntp(struct ntp_timestamp *ntpTs, struct timeval *unixTs)
{
	ntpTs->second = unixTs->tv_sec + SECONDS_SINCE_FIRST_EPOCH;//0x83AA7E80;
	ntpTs->fraction = (uint32_t)((double)(unixTs->tv_usec + 1) * (double)(1LL << 32) * 1.0e-6);
}
void 
NtpClient::convert_ntp_to_unix(struct ntp_timestamp *ntpTs, struct timeval *unixTs)
{
	unixTs->tv_sec = ntpTs->second - SECONDS_SINCE_FIRST_EPOCH;// 0x83AA7E80; // the seconds from Jan 1, 1900 to Jan 1, 1970
	unixTs->tv_usec = (uint32_t)((double)ntpTs->fraction * 1.0e6 / (double)(1LL << 32));
}

uint64_t
NtpClient::convert_ntp_to_date(uint64_t _ntpTs, struct date_structure *_outDataTs)
{
	uint32_t second = (uint32_t)((_ntpTs >> 32) & 0xFFFFFFFF);
	uint32_t fraction = (uint32_t)(_ntpTs & 0xFFFFFFFF);

	struct timeval unix;
	struct ntp_timestamp ntpTs;
	ntpTs.second = second;
	ntpTs.fraction = fraction;

	convert_ntp_to_unix(&ntpTs, &unix);
	_outDataTs->hour = (unix.tv_sec % 86400L) / 3600;
	_outDataTs->minute = (unix.tv_sec % 3600) / 60;
	_outDataTs->second = (unix.tv_sec % 60);
	_outDataTs->millisecond = unix.tv_usec;

	ostringstream _ss;
	_ss << std::internal << std::setfill('0') << std::setw(2) << _outDataTs->hour << std::internal << std::setfill('0') << std::setw(2) << _outDataTs->minute
		<< std::internal << std::setfill('0') << std::setw(2) << _outDataTs->second << std::internal << std::setfill('0') << std::setw(6) << _outDataTs->millisecond;

	std::string _s = _ss.str();
	return (stoull(_s));
}

void
NtpClient::CreateMessage(char* buffer)
{
	struct ntp_timestamp ntp;
	struct timeval unix;

	gettimeofday(&unix); //get time
	convert_unix_to_ntp(&ntp, &unix); // convert unix time to ntp time
	convert_ntp_to_unix(&ntp, &unix);
	uint64_t _ntpTs = ntp.second;
	_ntpTs = (_ntpTs << 32) | ntp.fraction;
	m_originateTimestamp = _ntpTs;

	SNTPMessage _sntpMsg;
	_sntpMsg.clear();  // Important, if you don't set the version/mode, the server will ignore you. 
	_sntpMsg._leapIndicator = 0;
	_sntpMsg._versionNumber = 3;
	_sntpMsg._mode = 3;
	_sntpMsg._originateTimestamp = _ntpTs; //optional (?)

	char value[sizeof(uint64_t)];
	memcpy(value, &_sntpMsg._originateTimestamp, sizeof(uint64_t));
	int jj = sizeof(uint64_t) - 1;
	int ofssetEnd = NTP_MSG_OFFSET_ORIGINATE_TIMESTAMP + sizeof(uint64_t);
	for (int ii = NTP_MSG_OFFSET_ORIGINATE_TIMESTAMP; ii < ofssetEnd; ii++)
	{
		buffer[ii] = value[jj];
		jj--;
	}

	buffer[0] = (_sntpMsg._leapIndicator << 6) | (_sntpMsg._versionNumber << 3) | _sntpMsg._mode; // create the 1-byte info in one go... the result should be 27 :)
}

void
NtpClient::ReceivedMessage(char* buffer)
{
	struct ntp_timestamp ntp;
	struct timeval unix;

	gettimeofday(&unix);
	convert_unix_to_ntp(&ntp, &unix); // convert unix time to ntp time

	uint64_t _ntpTs = ntp.second;
	_ntpTs = (_ntpTs << 32) | ntp.fraction;

	SNTPMessage _sntpMsg;
	_sntpMsg.clear();  
	_sntpMsg._leapIndicator = buffer[0] >> 6;
	_sntpMsg._versionNumber = (buffer[0] & 0x38) >> 3;
	_sntpMsg._mode = (buffer[0] & 0x7);
	_sntpMsg._stratum = buffer[1];
	_sntpMsg._pollInterval = buffer[2];
	_sntpMsg._precision = buffer[3];
	_sntpMsg._rootDelay = GetNtpField32(NTP_MSG_OFFSET_ROOT_DELAY, buffer);
	_sntpMsg._rootDispersion = GetNtpField32(NTP_MSG_OFFSET_ROOT_DISPERSION, buffer);
	int _refId[4];
	GetReferenceId(NTP_MSG_OFFSET_REFERENCE_IDENTIFIER, buffer, _refId);
	_sntpMsg._referenceIdentifier[0] = _refId[0];
	_sntpMsg._referenceIdentifier[1] = _refId[1];
	_sntpMsg._referenceIdentifier[2] = _refId[2];
	_sntpMsg._referenceIdentifier[3] = _refId[3];
	_sntpMsg._referenceTimestamp = GetNtpTimestamp64(NTP_MSG_OFFSET_REFERENCE_TIMESTAMP, buffer);
	_sntpMsg._originateTimestamp = GetNtpTimestamp64(NTP_MSG_OFFSET_ORIGINATE_TIMESTAMP, buffer);
	_sntpMsg._receiveTimestamp = GetNtpTimestamp64(NTP_MSG_OFFSET_RECEIVE_TIMESTAMP, buffer);
	_sntpMsg._transmitTimestamp = GetNtpTimestamp64(NTP_MSG_OFFSET_TRANSMIT_TIMESTAMP, buffer);

	uint64_t _tempOriginate = m_originateTimestamp;
	if (_sntpMsg._originateTimestamp > 0)
		_tempOriginate = _sntpMsg._originateTimestamp;

	struct date_structure dataTs;
	uint64_t _originClient = convert_ntp_to_date(_tempOriginate, &dataTs);
	std::cout << "Originate Client: " <<dataTs.hour << ":" << dataTs.minute << ":" << dataTs.second << "." << dataTs.millisecond << std::endl;
	uint64_t _receiveServer = convert_ntp_to_date(_sntpMsg._receiveTimestamp, &dataTs);
	std::cout << "Receive Server: " << dataTs.hour << ":" << dataTs.minute << ":" << dataTs.second << "." << dataTs.millisecond << std::endl;
	uint64_t _transmitServer = convert_ntp_to_date(_sntpMsg._transmitTimestamp, &dataTs);
	std::cout << "Transmit Server: " << dataTs.hour << ":" << dataTs.minute << ":" << dataTs.second << "." << dataTs.millisecond << std::endl;
	uint64_t _receiveClient = convert_ntp_to_date(_ntpTs, &dataTs);
	std::cout << "Receive Client: " << dataTs.hour << ":" << dataTs.minute << ":" << dataTs.second << "." << dataTs.millisecond << std::endl;

	int _clockOffset = (((_receiveServer - _originClient) + (_transmitServer - _receiveClient)) / 2); 
	_clockOffset = _clockOffset / 1000; // divided by 1e3 to only keep three decimals (negative means local clock is ahead, positive means local clock is behind)

	int _roundTripDelay = (_receiveClient - _originClient) - (_receiveServer - _transmitServer);
	_roundTripDelay = _roundTripDelay / 1000;

	std::cout << "Leap Second: " << (uint32_t) _sntpMsg._leapIndicator << " " << GetLeapString(_sntpMsg._leapIndicator) << "\n"
			  << "Version Number: " << (uint32_t)_sntpMsg._versionNumber << "\n"
		      << "Mode: " << (uint32_t) _sntpMsg._mode << " " << GetModeString(_sntpMsg._mode) << "\n"
		      << "Stratum: " << (uint32_t) _sntpMsg._stratum << " " << GetStratumString(_sntpMsg._stratum) << "\n"
		      << "Offset [ms]: " << _clockOffset << "\n"
		      << "RountTrip Delay [ms]: " << _roundTripDelay << std::endl;

	SetClockOffset(_clockOffset);
}

bool
NtpClient::Connect()
{
	int iResult;
	WSADATA wsaData;

	SOCKET SendSocket = INVALID_SOCKET;
	sockaddr_in RecvAddr;
	unsigned short Port = NTP_PORT;
	int BufLen = NTP_MSG_SIZE;

	//---------------------------------------------------------------------
	// Create the NTP tx timestamp and fill the fields in the msg to be tx
	char SendBuf[NTP_MSG_SIZE] = {0};
	CreateMessage(SendBuf);

	//----------------------
	//----------------------
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		wprintf(L"WSAStartup failed with error: %d\n", iResult);
		return false;
	}

	//---------------------------------------------
	// Create a socket for sending data
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (SendSocket == INVALID_SOCKET) {
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return false;
	}
	const char* host = NTP_SERVER;

	//---------------------------------------------
	struct hostent* hostV;
	if ((hostV = gethostbyname(host)) == nullptr)
	{
		//More descriptive error message?
		perror(host);
		exit(EXIT_FAILURE);
	}

	printf("Hostname: %s\n", hostV->h_name);
	printf("IP Address: %s\n", inet_ntoa(*((struct in_addr*)hostV->h_addr)));

	memset((char*)& RecvAddr, 0, sizeof(RecvAddr));
	RecvAddr.sin_family = AF_INET;

	std::memcpy((char*)& RecvAddr.sin_addr.s_addr, (char*)hostV->h_addr, hostV->h_length);
	RecvAddr.sin_port = htons(Port);
	if (connect(SendSocket, (struct sockaddr*) & RecvAddr, sizeof(RecvAddr)) < 0)
	{
		perror(host);
		exit(EXIT_FAILURE);
	}
	iResult = sendto(SendSocket, SendBuf, BufLen, 0, (SOCKADDR*)& RecvAddr, sizeof(RecvAddr));
	if (iResult == SOCKET_ERROR) {
		wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
		closesocket(SendSocket);
		WSACleanup();
		return false;
	}

	char bufferRx[NTP_MSG_SIZE] = { 0 };
	// Receive until the peer closes the connection
	iResult = recv(SendSocket, bufferRx, NTP_MSG_SIZE, 0);
	if (iResult == SOCKET_ERROR) {
		wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
		closesocket(SendSocket);
		WSACleanup();
		return false;
	}

	ReceivedMessage(bufferRx);
	WSACleanup();

	return true;
}

uint64_t
NtpClient::GetNtpTimestamp64(int offset, char* buffer)
{
	const int _len = sizeof(uint64_t);
	char valueRx[_len];
	memset(valueRx, 0, _len);
	int jj = sizeof(uint64_t) - 1;
	for (int ii = offset; ii < offset + _len; ii++)
	{
		valueRx[jj] = buffer[ii];
		jj--;
	}

	uint64_t milliseconds;
	memcpy(&milliseconds, valueRx, sizeof(uint64_t));

	return milliseconds;
}

unsigned int
NtpClient::GetNtpField32(int offset, char* buffer)
{
	const int _len = sizeof(int);
	char valueRx[_len];
	memset(valueRx, 0, _len);
	int jj = sizeof(int) - 1;
	for (int ii = offset; ii < offset + _len; ii++)
	{
		valueRx[jj] = buffer[ii];
		jj--;
	}

	unsigned int milliseconds;
	memcpy(&milliseconds, valueRx, sizeof(int));

	return milliseconds;
}

void 
NtpClient::GetReferenceId(int offset, char* buffer, int* _outArray)
{
	const int _len = sizeof(int);
	int jj = 0;
	for (int ii = offset; ii < offset + _len; ii++)
	{
		_outArray[jj] = buffer[ii];
		jj++;
	}
}

std::string
NtpClient::GetLeapString(unsigned char _leapIndicator)
{
	string var = "";
	switch (_leapIndicator)
	{
	case 0:
		var = "NoWarning";
		break;
	case 1:
		var = "LastMinute61";
		break;
	case 2:
		var = "LastMinute59";
		break;
	case 3:
		var = "Alarm";
		break;
	}

	return var;
}

std::string
NtpClient::GetModeString(unsigned char _mode)
{
	string var = "";
	switch (_mode)
	{
	case 1:
		var = "SymmetricActive";
		break;
	case 2:
		var = "SymmetricPassive";
		break;
	case 3:
		var = "Client";
		break;
	case 4:
		var = "Server";
		break;
	case 5:
		var = "Broadcast";
		break;
	default:
		var = "Reserved";
		break;
	}

	return var;
}

std::string
NtpClient::GetStratumString(unsigned char _stratum)
{
	string var = "";
	if (_stratum == 0)
	{
		var = "Unspecified";
	}
	else if (_stratum == 1)
	{
		var = "PrimaryReference";
	}
	else if (_stratum > 1 && _stratum < 16)
	{
		var = "SecondaryReference";
	}
	else
		var = "Reserved";

	return var;
}

void
NtpClient::SNTPMessage::clear() {
	memset(this, 0, sizeof(*this));
}

