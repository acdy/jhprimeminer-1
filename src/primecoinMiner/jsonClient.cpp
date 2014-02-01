#include "global.h"
#include "ticker.h"
#include <iostream>

#ifdef _WIN32
SOCKET jsonClient_openConnection(char *IP, int Port)
{
	SOCKET s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	SOCKADDR_IN addr;
	memset(&addr,0,sizeof(SOCKADDR_IN));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(Port);
	addr.sin_addr.s_addr=inet_addr(IP);
	int result = connect(s,(SOCKADDR*)&addr,sizeof(SOCKADDR_IN));
	if( result )
	{
		closesocket(s);
		return SOCKET_ERROR;
	}
#else
int jsonClient_openConnection(char *IP, int Port)
{
  int s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  sockaddr_in addr;
  memset(&addr,0,sizeof(sockaddr_in));
  addr.sin_family=AF_INET;
  addr.sin_port=htons(Port);
  addr.sin_addr.s_addr=inet_addr(IP);
  int result = connect(s,(sockaddr*)&addr,sizeof(sockaddr_in));
	if( result )
	{
		close(s);
		return 0;
	}
#endif
	return s;
}

static const char* base64_chars = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

int base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len, char* output) {
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];
	int32_t outputLength = 0;
	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;
			for(i = 0; (i <4) ; i++)
			{
				output[outputLength] = base64_chars[char_array_4[i]];
				outputLength++;
			}
			i = 0;
		}
	}
	if (i)
	{
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
		{
			output[outputLength] = base64_chars[char_array_4[j]];
			outputLength++;
		}
		while((i++ < 3))
		{
			output[outputLength] = '=';
			outputLength++;
		}
	}
	return outputLength;
}

unsigned char * base64_decode(const unsigned char *src, size_t len, uint8_t* out, int32_t *out_len)
{
	unsigned char dtable[256], *pos, in[4], block[4], tmp;
	size_t i, count, olen;
	memset(dtable, 0x80, 256);
	for (i = 0; i < 64; i++)
		dtable[base64_chars[i]] = (unsigned char)i;
	dtable['='] = 0;
	count = 0;
	for (i = 0; i < len; i++) {
		if (dtable[src[i]] != 0x80)
			count++;
	}
	if (count % 4)
		return NULL;
	olen = count / 4 * 3;
	pos = out;
	if (out == NULL)
		return NULL;

	count = 0;
	for (i = 0; i < len; i++) {
		tmp = dtable[src[i]];
		if (tmp == 0x80)
			continue;

		in[count] = src[i];
		block[count] = tmp;
		count++;
		if (count == 4) {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
		}
	}
	if (pos > out) {
		if (in[2] == '=')
			pos -= 2;
		else if (in[3] == '=')
			pos--;
	}
	*out_len = pos - out;
	return out;
}

jsonObject_t* jsonClient_request(jsonRequestTarget_t* server, char* methodName, fStr_t* fStr_parameterData, int32_t* errorCode)
{
  using namespace std;
	*errorCode = JSON_ERROR_NONE;
	// create connection to host
#ifdef _WIN32
	SOCKET serverSocket = jsonClient_openConnection(server->ip, server->port);
#else
  int serverSocket = jsonClient_openConnection(server->ip, server->port);
#endif
	if( serverSocket == 0 )
	{
		*errorCode = JSON_ERROR_HOST_NOT_FOUND;
		return NULL;
	}


#ifdef _WIN32
	// set socket as non-blocking
	unsigned int nonblocking=1;
	unsigned int cbRet;
	WSAIoctl(serverSocket, FIONBIO, &nonblocking, sizeof(nonblocking), NULL, 0, (LPDWORD)&cbRet, NULL, NULL);
#else
  int flags, err;
  flags = fcntl(serverSocket, F_GETFL, 0); 
  flags |= O_NONBLOCK;
  err = fcntl(serverSocket, F_SETFL, flags); //ignore errors for now..
#endif

	//uint32_t startTime = GetTickCount(); // todo: Replace with crossplatform method
//  uint64_t startTime = getTimeMilliseconds();   unused
	// build json request data
	// example: {"method": "getwork", "params": [], "id":0}
	fStr_t* fStr_jsonRequestData = fStr_alloc(1024*512); // 64KB (this is also used as the recv buffer!)
	fStr_appendFormatted(fStr_jsonRequestData, "{\"method\": \"%s\", \"params\": ", methodName);
	//jsonBuilder_buildObjectString(fStr_jsonRequestData, jsonObjectParameter);
	if( fStr_parameterData )
		fStr_append(fStr_jsonRequestData, fStr_parameterData);
	else
		fStr_append(fStr_jsonRequestData, "[]"); // no parameter -> empty array
	fStr_append(fStr_jsonRequestData, ", \"id\":0}");
	// prepare header
	fStr_buffer1kb_t fStrBuffer_header;
	fStr_t* fStr_headerData = fStr_alloc(&fStrBuffer_header, FSTR_FORMAT_UTF8);
	// header fields
	fStr_appendFormatted(fStr_headerData, "POST / HTTP/1.1\r\n");
	if( server->authUser )
	{
		char authString[256];
		char authStringEncoded[512];
		if( server->authPass )
			sprintf(authString, "%s:%s", server->authUser, server->authPass);
		else
			sprintf(authString, "%s", server->authUser); // without password
		int32_t base64EncodedLength = base64_encode((const unsigned char*)authString, fStrLen(authString), authStringEncoded);
		authStringEncoded[base64EncodedLength] = '\0';
		fStr_appendFormatted(fStr_headerData, "Authorization: Basic %s\r\n", authStringEncoded);
	}
	fStr_appendFormatted(fStr_headerData, "Host: %s:%d\r\n", server->ip, (int32_t)server->port);
	fStr_appendFormatted(fStr_headerData, "User-Agent: ypoolbackend 0.1\r\n");
	fStr_appendFormatted(fStr_headerData, "Accept-Encoding: identity\r\n");
	fStr_appendFormatted(fStr_headerData, "Content-Type: application/json\r\n");
	fStr_appendFormatted(fStr_headerData, "Content-Length: %d\r\n", fStr_len(fStr_jsonRequestData));
	fStr_appendFormatted(fStr_headerData, "\r\n"); // empty line concludes the header
	// send header and data
	uint64_t startTime = getTimeMilliseconds();
	send(serverSocket, fStr_get(fStr_headerData), fStr_len(fStr_headerData), 0);
	//std::cout << "Headers: " << fStr_get(fStr_headerData) << std::endl;
	send(serverSocket, fStr_get(fStr_jsonRequestData), fStr_len(fStr_jsonRequestData), 0);
	//std::cout << "Request: " << fStr_get(fStr_jsonRequestData) << std::endl;
	// receive header and request data
	uint8_t* recvBuffer = (uint8_t*)fStr_get(fStr_jsonRequestData);
	uint32_t recvLimit = fStr_getLimit(fStr_jsonRequestData);
	uint32_t recvIndex = 0;
	uint32_t recvDataSizeFull = 0;
	uint32_t recvDataHeaderEnd = 0;
	// recv
	while( true )
	{
		int32_t remainingRecvSize = (int32_t)recvLimit - recvIndex;
		if( remainingRecvSize == 0 )
		{
			printf("JSON-RPC warning: Response is larger than buffer\n");
			// todo: Eventually we should dynamically enlarge the buffer
#ifdef _WIN32
			closesocket(serverSocket);
#else
	      close(serverSocket);
#endif
		fStr_free(fStr_jsonRequestData);
			return NULL;
		}
		// wait for data to receive
		fd_set fds;
		int n;
		struct timeval tv;
		FD_ZERO(&fds) ;
		FD_SET(serverSocket, &fds) ;
		tv.tv_sec = 10; // timeout 10 seconds after the last recv
		tv.tv_usec = 0;
		// wait until timeout or data received.
		n = select(serverSocket, &fds, NULL, NULL, &tv ) ;
/*		if( n == 0)
		{
			//uint32_t passedTime = GetTickCount() - startTime;
      uint64_t passedTime = getTimeMilliseconds() - startTime;
			printf("JSON request timed out after %lums\n", passedTime);
    
			break;
		}
		else if( n == -1 )
		{
			printf("JSON receive error\n");
			break;
		}*/
		int32_t r = recv(serverSocket, (char*)(recvBuffer+recvIndex), remainingRecvSize, 0);
		if( r <= 0 )
		{
			// client closed connection
			// this can also happen when we have received all data
#ifdef _WIN32
			closesocket(serverSocket);
#else
	      close(serverSocket);
#endif
			serverSocket = 0; // set socket to zero so we know the connection is already closed


			break;
		}
		else
		{
			recvIndex += r;
			// process data (if no header end found yet)
			if( recvDataHeaderEnd == 0 )
			{
				// did we receive the end of the header already?
				int32_t scanStart = (int32_t)recvIndex - (int32_t)r;
				scanStart = std::max(scanStart-8, 0);
				int32_t scanEnd = (int32_t)(recvIndex);
				scanEnd = std::max(scanEnd-4, 0);
				for(int32_t s=scanStart; s<=scanEnd; s++)
				{
					// is header end?
					if( recvBuffer[s+0] == '\r' &&
						recvBuffer[s+1] == '\n' &&
						recvBuffer[s+2] == '\r' && 
						recvBuffer[s+3] == '\n' )
					{
						if( s == 0 )
						{
							printf("JSON-RPC warning: Server sent headerless HTTP response\n");
#ifdef _WIN32
							closesocket(serverSocket);
#else
			                close(serverSocket);
#endif
							fStr_free(fStr_jsonRequestData);
							return NULL;
						}
						// iterate header for content length field
						// when parsing the header we can assume two things
						// *) there will be always 4 bytes after endOfHeaderPtr
						// *) the 4 bytes at the end will always be \r\n\r\n
						char* endOfHeaderPtr = (char*)(recvBuffer+s);
						char* parsePtr = (char*)recvBuffer;
						int32_t contentLength = -1;
						while( parsePtr < endOfHeaderPtr )
						{
							// is content-length parameter?
							// Content-Length: 11390
							if( (endOfHeaderPtr-parsePtr) >= 15 && memcmp(parsePtr, "Content-Length:", 15) == 0 )
							{
								// parameter found
								// read value and end header parsing (for now)
								if( parsePtr[15] == ' ' ) // is there a space between the number and the colon?
									contentLength = atoi(parsePtr+16);
								else
									contentLength = atoi(parsePtr+15);
								break;
							}
							// authorization required?
							// 401 Authorization Required
							if( (endOfHeaderPtr-parsePtr) >= 10 && memcmp(parsePtr, "HTTP/1.0 ", 9) == 0 )
							{
								// parameter found
								int32_t httpCode = atoi(parsePtr+9);
								if( httpCode == 401 )
								{
									printf("JSON-RPC: Request failed, authorization required (wrong login data)\n");
#ifdef _WIN32
									closesocket(serverSocket);
#else
							        close(serverSocket);
#endif
									fStr_free(fStr_jsonRequestData);
									*errorCode = httpCode + 1000;
									return NULL;
								}
								break;
							}
							while( parsePtr < endOfHeaderPtr )
							{
								parsePtr++;
								if( parsePtr[0] == '\r' && parsePtr[1] == '\n' )
								{
									// next line found
									parsePtr += 2;
									break;
								}
							}
						}
						if( contentLength <= 0 )
						{
							printf("JSON-RPC warning: Content-Length header field not present\n");
#ifdef _WIN32
							closesocket(serverSocket);
#else
			                close(serverSocket);
#endif
							fStr_free(fStr_jsonRequestData);
							return NULL;
						}
						// mark header and packet size, then finish processing header
						recvDataHeaderEnd = s+4;
						recvDataSizeFull = recvDataHeaderEnd + contentLength;
					}
				}
			}
			// process data (if header found and full packet received)
			if( recvDataHeaderEnd != 0 && recvIndex >= recvDataSizeFull )
			{
				// process request
				break;
				//jsonRpc_processRequest(jrs, client);
				//// wipe processed packet and shift remaining data back
				//client->recvDataHeaderEnd = 0;
				//uint32_t pRecvIndex = client->recvIndex;
				//client->recvIndex -= client->recvDataSizeFull;
				//client->recvDataSizeFull = 0;
				//if( client->recvIndex > 0 )
				//	__debugbreak(); // todo -> The client tried to send us more than one single request?
			}
		}
	}

	if( recvDataHeaderEnd != 0 )
	{
		// close connection (we kind of force this)
		if( serverSocket != 0 )
		{
#ifdef _WIN32
			closesocket(serverSocket);
#else
            close(serverSocket);
#endif
			serverSocket = 0;
		}
		// get request result data
		char* requestData = (char*)(recvBuffer+recvDataHeaderEnd);
		int32_t requestLength = (int32_t)(recvDataSizeFull - recvDataHeaderEnd);
		// parse data
		jsonObject_t* jsonObjectReturn = jsonParser_parse((uint8_t*)requestData, requestLength);
		if( jsonObjectReturn == NULL )
		{
			*errorCode = JSON_ERROR_UNABLE_TO_PARSE;
			fStr_free(fStr_jsonRequestData);
			return NULL;
		}
		// free temporary buffer
		fStr_free(fStr_jsonRequestData);
		// finally test if the return object has a result parameter
		if( jsonObject_getParameter(jsonObjectReturn, "result") == NULL )
		{
			jsonObject_freeObject(jsonObjectReturn);
			*errorCode = JSON_ERROR_NO_RESULT;
			return NULL;
		}
		// return parsed object
		return jsonObjectReturn;
	}


	// close connection
	if( serverSocket != 0 )
	{
#ifdef _WIN32
		closesocket(serverSocket);
#else
        close(serverSocket);
#endif
		serverSocket = 0;
	}


	// free everything again
	fStr_free(fStr_jsonRequestData);
	return NULL;
}
