typedef struct _xptServer_t xptServer_t;

typedef struct  
{
	uint8_t* buffer;
	uint32_t parserIndex;
	uint32_t bufferLimit; // current maximal size of buffer
	uint32_t bufferSize; // current effective size of buffer
}xptPacketbuffer_t;

typedef struct  
{
	uint8_t merkleRoot[32];
	uint32_t seed;
}xptWorkData_t;

typedef struct  
{
	uint32_t height;
	uint32_t version;
	uint32_t nTime;
	uint32_t nBits;
	uint32_t nBitsShare;
	uint8_t prevBlock[32];
}xptBlockWorkInfo_t;

typedef struct _xptServer_t 
{
#ifdef _WIN32
	SOCKET acceptSocket;
#else
	int acceptSocket;
#endif
	simpleList_t* list_connections;
	xptPacketbuffer_t* sendBuffer; // shared buffer for sending data
	// last known block height (for new block detection)
	uint32_t coinTypeBlockHeight[32];
	// callbacks
	bool(*xptCallback_generateWork)(xptServer_t* xptServer, uint32_t numOfWorkEntries, uint32_t coinTypeIndex, xptBlockWorkInfo_t* xptBlockWorkInfo, xptWorkData_t* xptWorkData);
	void(*xptCallback_getBlockHeight)(xptServer_t* xptServer, uint32_t* coinTypeNum, uint32_t* blockHeightPerCoinType);
}xptServer_t;

typedef struct  
{
	xptServer_t* xptServer;
#ifdef _WIN32
	SOCKET clientSocket;
#else
	int clientSocket;
#endif
	bool disconnected;
	// recv buffer
	xptPacketbuffer_t* packetbuffer;
	uint32_t recvIndex;
	uint32_t recvSize;
	// recv header info
	uint32_t opcode;
	// authentication info
	uint8_t clientState;
	char workerName[128];
	char workerPass[128];
	uint32_t userId;
	uint32_t coinTypeIndex;
	uint32_t payloadNum;

	//uint32_t size;
	//// http auth
	//bool useBasicHTTPAuth;
	//char httpAuthUsername[64];
	//char httpAuthPassword[64];
	//// auto-diconnect
	//uint32_t connectionOpenedTimer;
}xptServerClient_t;

// client states
#define XPT_CLIENT_STATE_NEW		(0)
#define XPT_CLIENT_STATE_LOGGED_IN	(1)


// list of known opcodes

#define XPT_OPC_C_AUTH_REQ		1
#define XPT_OPC_S_AUTH_ACK		2
#define XPT_OPC_S_WORKDATA1		3
#define XPT_OPC_C_SUBMIT_SHARE	4
#define XPT_OPC_S_SHARE_ACK		5

#define XPT_OPC_C_PING			8
#define XPT_OPC_S_PING			8


// list of error codes

#define XPT_ERROR_NONE				(0)
#define XPT_ERROR_INVALID_LOGIN		(1)
#define XPT_ERROR_INVALID_WORKLOAD	(2)
#define XPT_ERROR_INVALID_COINTYPE	(3)

// xpt general
xptServer_t* xptServer_create(uint16_t port);
void xptServer_startProcessing(xptServer_t* xptServer);

// private packet handlers
bool xptServer_processPacket_authRequest(xptServer_t* xptServer, xptServerClient_t* xptServerClient);

// public packet methods
bool xptServer_sendBlockData(xptServer_t* xptServer, xptServerClient_t* xptServerClient);

// packetbuffer
xptPacketbuffer_t* xptPacketbuffer_create(uint32_t initialSize);
void xptPacketbuffer_free(xptPacketbuffer_t* pb);
void xptPacketbuffer_changeSizeLimit(xptPacketbuffer_t* pb, uint32_t sizeLimit);


void xptPacketbuffer_beginReadPacket(xptPacketbuffer_t* pb);
uint32_t xptPacketbuffer_getReadSize(xptPacketbuffer_t* pb);
float xptPacketbuffer_readFloat(xptPacketbuffer_t* pb, bool* error);
uint32_t xptPacketbuffer_readU32(xptPacketbuffer_t* pb, bool* error);
uint16_t xptPacketbuffer_readU16(xptPacketbuffer_t* pb, bool* error);
uint8_t xptPacketbuffer_readU8(xptPacketbuffer_t* pb, bool* error);
void xptPacketbuffer_readData(xptPacketbuffer_t* pb, uint8_t* data, uint32_t length, bool* error);

void xptPacketbuffer_beginWritePacket(xptPacketbuffer_t* pb, uint8_t opcode);
void xptPacketbuffer_writeU32(xptPacketbuffer_t* pb, bool* error, uint32_t v);
void xptPacketbuffer_writeU16(xptPacketbuffer_t* pb, bool* error, uint16_t v);
void xptPacketbuffer_writeU8(xptPacketbuffer_t* pb, bool* error, uint8_t v);
void xptPacketbuffer_writeData(xptPacketbuffer_t* pb, uint8_t* data, uint32_t length, bool* error);
void xptPacketbuffer_finalizeWritePacket(xptPacketbuffer_t* pb);

void xptPacketbuffer_writeString(xptPacketbuffer_t* pb, char* stringData, uint32_t maxStringLength, bool* error);
void xptPacketbuffer_readString(xptPacketbuffer_t* pb, char* stringData, uint32_t maxStringLength, bool* error);
