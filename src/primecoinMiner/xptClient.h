typedef struct  
{
	uint8_t merkleRoot[32];
	uint8_t prevBlockHash[32];
	uint32_t version;
	uint32_t nonce;
	uint32_t nTime;
	uint32_t nBits;
	// primecoin specific
	uint32_t sieveSize;
	uint32_t sieveCandidate; // index of sieveCandidate for this share
	uint8_t fixedMultiplierSize;
	uint8_t fixedMultiplier[201];
	uint8_t chainMultiplierSize;
	uint8_t chainMultiplier[201];
}xptShareToSubmit_t;

typedef struct  
{
#ifdef _WIN32
	SOCKET clientSocket;
#else
    int clientSocket;
#endif
	xptPacketbuffer_t* sendBuffer; // buffer for sending data
	xptPacketbuffer_t* recvBuffer; // buffer for receiving data
	// worker info
	char username[128];
	char password[128];
	uint32_t payloadNum;
	uint32_t clientState;
	// recv info
	uint32_t recvSize;
	uint32_t recvIndex;
	uint32_t opcode;
	// disconnect info
	bool disconnected;
	char* disconnectReason;
	// periodic ping/heartbeat info
	time_t lastClient2ServerInteractionTimestamp;
	// work data
	uint32_t workDataCounter; // timestamp of when received the last block of work data
	bool workDataValid;
	xptBlockWorkInfo_t blockWorkInfo;
	xptWorkData_t workData[128]; // size equal to max payload num
	// shares to submit
#ifdef _WIN32
	CRITICAL_SECTION cs_shareSubmit;
#else
  pthread_mutex_t cs_shareSubmit;
#endif
	simpleList_t* list_shareSubmitQueue;
}xptClient_t;

xptClient_t* xptClient_connect(jsonRequestTarget_t* target, uint32_t payloadNum);
void xptClient_free(xptClient_t* xptClient);

bool xptClient_process(xptClient_t* xptClient); // needs to be called in a loop
bool xptClient_isDisconnected(xptClient_t* xptClient, char** reason);
bool xptClient_isAuthenticated(xptClient_t* xptClient);

void xptClient_foundShare(xptClient_t* xptClient, xptShareToSubmit_t* xptShareToSubmit);

// never send this directly
void xptClient_sendWorkerLogin(xptClient_t* xptClient);

// packet handlers
bool xptClient_processPacket_authResponse(xptClient_t* xptClient);
bool xptClient_processPacket_blockData1(xptClient_t* xptClient);
bool xptClient_processPacket_shareAck(xptClient_t* xptClient);
bool xptClient_processPacket_client2ServerPing(xptClient_t* xptClient);

