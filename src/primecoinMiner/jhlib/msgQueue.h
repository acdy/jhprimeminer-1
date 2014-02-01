

typedef struct _msgQueue_t msgQueue_t;


typedef struct
{
	int32_t msgId;
	uint32_t paramA;
	uint32_t paramB;
	void* data; // additional dynamic data, automatically freed
}msgInfo_t;

typedef struct _msgInfoLink_t
{
	msgInfo_t msgInfo;
	// next
	_msgInfoLink_t *next;
}msgInfoLink_t;

typedef struct _msgQueue_t
{
#ifdef _WIN32
	CRITICAL_SECTION criticalSection;
#else
  pthread_mutex_t criticalSection;
#endif
	int32_t nameId;
	msgInfoLink_t *first;
	msgInfoLink_t *last; // for fast appending
	// message queue callback
#ifdef _WIN32
	void (JHCALLBACK *messageProc)(msgQueue_t* msgQueue, int32_t msgId, uint32_t param1, uint32_t param2, void* data);
#else
  void *messageProc(msgQueue_t* msgQueue, int32_t msgId, uint32_t param1, uint32_t param2, void* data);
#endif
	void* custom;
}msgQueue_t;


void msgQueue_init();

int32_t msgQueue_generateUniqueNameId();

#ifdef _WIN32
msgQueue_t* msgQueue_create(int32_t nameId, void (JHCALLBACK *messageProc)(msgQueue_t* msgQueue, int32_t msgId, uint32_t param1, uint32_t param2, void* data));
#else
msgQueue_t* msgQueue_create(int32_t nameId, void *messageProc(msgQueue_t* msgQueue, int32_t msgId, uint32_t param1, uint32_t param2, void* data));
#endif
//void msgQueue_activate(msgQueue_t* msgQueue);

bool msgQueue_check(msgQueue_t* msgQueue);
bool msgQueue_postMessage(int32_t destNameId, int32_t msgId, uint32_t param1, uint32_t param2, void* data);

#define msgQueue_setCustom(x, v)	(x)->custom = v
#define msgQueue_getCustom(x)		(x)->custom