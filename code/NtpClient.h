/**
 *  This class is developed to get the local time or UTC time with precision.
 *  In particular, the offset is returned based on the Network Time Protocol (NTP),
 *  where ntp servers are used to sync the clock and include the timestamp. If accuracy
 *  is not an issue, then use the timestamp.cpp (i.e. locally).
 *
 *  Ioannis Selinis 2019 (5GIC, University of Surrey)
 */

 /**
  *   ///  Structure of the standard NTP header (as described in RFC 2030) ///
  *						   1                   2                   3
  *	   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |  (0)
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                          Root Delay                           |  (4)
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                       Root Dispersion                         |  (8)
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                     Reference Identifier                      |  (12)
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                                                               |
  *	  |                   Reference Timestamp (64)                    |  (16)
  *	  |                                                               |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                                                               |
  *	  |                   Originate Timestamp (64)                    |  (24)
  *	  |                                                               |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                                                               |
  *	  |                    Receive Timestamp (64)                     |  (32)
  *	  |                                                               |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                                                               |
  *	  |                    Transmit Timestamp (64)                    |  (40)
  *	  |                                                               |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                 Key Identifier (optional) (32)                |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *	  |                                                               |
  *	  |                                                               |
  *	  |                 Message Digest (optional) (128)               |
  *	  |                                                               |
  *	  |                                                               |
  *	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  *	/// SNTP Timestamp Format (as described in RFC 2030)
  *                         1                   2                   3
  *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                           Seconds                             |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                  Seconds Fraction (0-padded)                  |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */

#ifndef NTPCLIENT_H
#define NTPCLIENT_H

#include <winsock2.h>
#include <ctime>
#include <ws2def.h>
#include <string>
#include <stdlib.h>

class NtpClient
{
public:
	NtpClient();
	~NtpClient();

	void dns_lookup(const char* host, sockaddr_in* out);
	/**
	 * This function should be called to create a socket/connect/receive NTP message.
	 * Returns true upon success, false otherwise.
	 */
	bool Connect();
	/**
	 * This function returns the clock offset in ms. 
	 * Negative value means the local clock is ahead, positive means the local clock is behind (relative to the NTP server)
	 */
	int GetClockOffset(void);

private:

	struct ntp_timestamp
	{
		uint32_t second;
		uint32_t fraction;
	};

	struct date_structure
	{
		int hour;
		int minute;
		int second;
		int millisecond;
	};

	struct SNTPMessage
	{
		unsigned char _leapIndicator;			/**< Leap seconds warning of an impending leap second to be inserted/deleted in the last minute of the current day. See the [RFC](http://tools.ietf.org/html/rfc5905#section-7.3) */
		unsigned char _versionNumber;			/**< Protocol version. Should be set to 3 (Version number of the protocol (3 or 4)). */
		unsigned char _mode;					/**< Mode of the message sender - Returns mode. 3 = Client, 4 = Server */
		unsigned char _stratum;					/**< Servers between client and physical timekeeper. 1 = Server is Connected to Physical Source. 0 = Unknown. */
		unsigned char _pollInterval;			/**< Max Poll Rate. In log2 seconds. */
		unsigned char _precision;				/**< Precision of the clock. In log2 seconds. */
		unsigned int _rootDelay;				/**< Round-trip to reference clock. NTP Short Format. */
		unsigned int _rootDispersion;			/**< Dispersion to reference clock. NTP Short Format. */
		unsigned int _referenceIdentifier[4];	/**< Reference ID. For Stratum 1 devices, a 4-byte string. For other devices, 4-byte IP address. */
		uint64_t _referenceTimestamp;			/**< Reference Timestamp. This is the time at which the local clock was last set or corrected, in 64-bit timestamp format. */
		uint64_t _originateTimestamp;			/**< Originate Timestamp. This is the time at which the request departed the client for the server, in 64-bit timestamp format. */
		uint64_t _receiveTimestamp;				/**< Receive Timestamp. This is the time at which the request arrived at the server, in 64-bit timestamp format. */
		uint64_t _transmitTimestamp;			/**< Transmit Timestamp. This is the time at which the reply departed the server for the client, in 64-bit timestamp format. */

		/**
		 * Zero all the values.
		 */
		void clear();
	};

	enum _LeapIndicatorValues
	{
		NoWarning,      // 0 - No warning
		LastMinute61,   // 1 - Last minute has 61 seconds
		LastMinute59,   // 2 - Last minute has 59 seconds
		Alarm           // 3 - Alarm condition (clock not synchronized)
	};

	//Mode field values
	enum _ModeValues
	{
		SymmetricActive,    // 1 - Symmetric active
		SymmetricPassive,   // 2 - Symmetric passive
		Client,             // 3 - Client
		Server,             // 4 - Server
		Broadcast,          // 5 - Broadcast
		Unknown             // 0, 6, 7 - Reserved
	};

	// Stratum field values
	enum _StratumValues
	{
		Unspecified,            // 0 - unspecified or unavailable
		PrimaryReference,       // 1 - primary reference (e.g. radio-clock)
		SecondaryReference,     // 2-15 - secondary reference (via NTP or SNTP)
		Reserved                // 16-255 - reserved
	};

	/**
	 * This function returns the timestamp (64-bit) from the received
	 * buffer, given the offset provided.
	 * 
	 * \param offset the offset of the timestamp in the NTP message
	 * \param buffer the received message
	 *
	 * Returns the ntp timestamp 
	 */
	uint64_t GetNtpTimestamp64(int offset, char* buffer);
	/**
	 * This function returns the 32-bit value from the received
	 * buffer, given the offset provided.
	 *
	 * \param offset the offset of the field in the NTP message
	 * \param buffer the received message
	 *
	 * Returns the ntp 32-bit value (e.g. for Root Delay)
	 */
	unsigned int GetNtpField32(int offset, char* buffer);
	/**
	 * This function returns an array based on the Reference ID 
	 * (converted from NTP message), given the offset provided.
	 *
	 * \param offset the offset of the field in the NTP message
	 * \param buffer the received message
	 *
	 * Returns the array of Reference ID
	 */
	void GetReferenceId(int offset, char* buffer, int* _outArray);
	/**
	 * This function sets the clock offset in ms.
	 * Negative value means the local clock is ahead, 
	 * positive means the local clock is behind (relative to the NTP server)
	 *
	 * \param clockOffset the clock offset in ms
	 */
	void SetClockOffset(int clockOffset);
	/**
	 * This function creates the SNTP message ready for transmission (SNTP Req)
	 * and returns it back.
	 *
	 * \param buffer the message to be sent
	 */
	void CreateMessage(char* buffer);
	/**
	 * This function gets the information received from the SNTP response
	 * and prints the results (e.g. offset, round trip delay etc.)
	 *
	 * \param buffer the message received
	 */
	void ReceivedMessage(char* buffer);
	/**
	 * This function gets the UNIX time
	 *
	 * \param tp the structure where the tv_sec and tv_usec will be stored
	 */
	void gettimeofday(struct timeval* tp);
	/**
	 * This function converts the UNIX time to NTP
	 *
	 * \param ntpTs the structure NTP where the NTP values are stored
	 * \param unixTs the structure UNIX (with the already set tv_sec and tv_usec)
	 */
	void convert_unix_to_ntp(struct ntp_timestamp* ntpTs, struct timeval* unixTs);
	/**
	 * This function converts the NTP time to UNIX
	 *
	 * \param ntpTs the structure NTP where the NTP values are already set
	 * \param unixTs the structure UNIX where the UNIX values are stored
	 */
	void convert_ntp_to_unix(struct ntp_timestamp* ntpTs, struct timeval* unixTs);
	/**
	 * This function converts the NTP time to local time
	 *
	 * \param _ntpTs the NTP timestamp to be converted
	 * \param _outDataTs the structure Date where the [HH, MM, SS, MMMMMM] are stored
	 */
	uint64_t convert_ntp_to_date(uint64_t _ntpTs, struct date_structure* _outDataTs);
	/**
	 * This function returns the LeapIndicator field in a string format (see _LeapIndicatorValues).
     *
	 * \param _leapIndicator the _leapIndicator from structure SNTPMessage (unsigned char)
	 *
	 * Returns the string format of LeapIndicator
	 */
	std::string GetLeapString(unsigned char _leapIndicator);
	/**
	 * This function returns the Mode field in a string format (see _ModeValues).
	 *
	 * \param _mode the _mode from structure SNTPMessage (unsigned char)
	 *
	 * Returns the string format of Mode
	 */
	std::string GetModeString(unsigned char _mode);
	/**
	 * This function returns the Stratum field in a string format (see _StratumValues).
	 *
	 * \param _stratum the _stratum from structure SNTPMessage (unsigned char)
	 *
	 * Returns the string format of Stratum
	 */
	std::string GetStratumString(unsigned char _stratum);


	int m_clockOffset;			   // offset of the local clock	
	uint64_t m_originateTimestamp; // the time that the req is transmitted (in case that the NTP server does not copy this field from the req to the response)
};

#endif  /* NTPCLIENT_H */