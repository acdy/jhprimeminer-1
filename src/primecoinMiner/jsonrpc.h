extern uint8_t* json_emptyString;

typedef struct  
{
	uint8_t type;
}jsonObject_t; // the base object struct, should never be allocated directly

typedef struct  
{
	uint8_t* stringNameData;
	uint32_t stringNameLength;
	jsonObject_t* jsonObjectValue;
}jsonObjectRawObjectParameter_t;

typedef struct  
{
	jsonObject_t baseObject;
	customBuffer_t* list_paramPairs; // array of jsonObjectRawObjectParameter_t
}jsonObjectRawObject_t;

typedef struct  
{
	jsonObject_t baseObject;
	uint8_t* stringData;
	uint32_t stringLength;
}jsonObjectString_t;

typedef struct  
{
	jsonObject_t baseObject;
	int64_t number;
	uint64_t divider; // divide number by this factor to get the real result (usually divider will be 1)
}jsonObjectNumber_t;

typedef struct  
{
	jsonObject_t baseObject;
	bool isTrue;
}jsonObjectBool_t;

typedef struct  
{
	jsonObject_t baseObject;
	simpleList_t* list_values; // array of jsonObject_t ptrs
}jsonObjectArray_t;

#define JSON_TYPE_OBJECT	(1)
#define JSON_TYPE_STRING	(2)
#define JSON_TYPE_ARRAY		(3)
#define JSON_TYPE_NUMBER	(4)
#define JSON_TYPE_NULL		(5)
#define JSON_TYPE_BOOL		(6)

typedef struct  
{
	uint8_t* dataBuffer;
	uint32_t dataLength;
	uint8_t* dataCurrentPtr;
	uint8_t* dataEnd;
}jsonParser_t;

typedef struct  
{
	char* ip;
	uint16_t port;
	char* authUser;
	char* authPass;
}jsonRequestTarget_t;

#define JSON_ERROR_NONE					(0)		// no error (success)
#define JSON_ERROR_HOST_NOT_FOUND		(1)		// unable to connect to ip/port
#define JSON_ERROR_UNABLE_TO_PARSE		(2)		// error parsing the returned JSON data
#define JSON_ERROR_NO_RESULT			(3)		// missing result parameter


jsonObject_t* jsonParser_parse(uint8_t* stream, uint32_t dataLength);

#define JSON_INITIAL_RECV_BUFFER_SIZE	(1024*4) // 4KB
#define JSON_MAX_RECV_BUFFER_SIZE	(1024*1024*4) // 4MB

typedef struct jsonRpcServer_t
{
#ifdef _WIN32
	SOCKET acceptSocket;
#else
	int acceptSocket;
#endif
	simpleList_t* list_connections;
}jsonRpcServer_t;


typedef struct  
{
	jsonRpcServer_t* jsonRpcServer;
#ifdef _WIN32
	SOCKET clientSocket;
#else
	int clientSocket;
#endif
	bool disconnected;
	// recv buffer
	uint32_t recvIndex;
	uint32_t recvSize;
	uint8_t* recvBuffer;
	// recv header info
	uint32_t recvDataSizeFull;
	uint32_t recvDataHeaderEnd;
	// http auth
	bool useBasicHTTPAuth;
	char httpAuthUsername[64];
	char httpAuthPassword[64];
}jsonRpcClient_t;

// server
jsonRpcServer_t* jsonRpc_createServer(uint16_t port);
int jsonRpc_run(jsonRpcServer_t* jrs);
void jsonRpc_sendResponse(jsonRpcServer_t* jrs, jsonRpcClient_t* client, jsonObject_t* jsonObjectReturnValue);
void jsonRpc_sendResponseRaw(jsonRpcServer_t* jrs, jsonRpcClient_t* client, fStr_t* fStr_responseRawData, char* additionalHeaderData);
void jsonRpc_sendFailedToAuthorize(jsonRpcServer_t* jrs, jsonRpcClient_t* client);

// object
jsonObject_t* jsonObject_getParameter(jsonObject_t* jsonObject, char* name);
uint32_t jsonObject_getType(jsonObject_t* jsonObject);
uint8_t* jsonObject_getStringData(jsonObject_t* jsonObject, uint32_t* length);
uint32_t jsonObject_getArraySize(jsonObject_t* jsonObject);
jsonObject_t* jsonObject_getArrayElement(jsonObject_t* jsonObject, uint32_t index);
bool jsonObject_isTrue(jsonObject_t* jsonObject);
double jsonObject_getNumberValueAsDouble(jsonObject_t* jsonObject);
int32_t jsonObject_getNumberValueAsS32(jsonObject_t* jsonObject);
uint32_t jsonObject_getNumberValueAsU32(jsonObject_t* jsonObject);

// object deallocation
void jsonObject_freeStringData(uint8_t* stringBuffer);
void jsonObject_freeObject(jsonObject_t* jsonObject);

// builder
void jsonBuilder_buildObjectString(fStr_t* fStr_output, jsonObject_t* jsonObject);

// client
jsonObject_t* jsonClient_request(jsonRequestTarget_t* server, char* methodName, fStr_t* fStr_parameterData, int32_t* errorCode);

// base64
int base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len, char* output);
unsigned char * base64_decode(const unsigned char *src, size_t len, uint8_t* out, int32_t *out_len);