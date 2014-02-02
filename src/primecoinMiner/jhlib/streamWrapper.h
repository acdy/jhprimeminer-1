typedef struct _stream_t stream_t;

typedef struct  
{
#ifdef _WIN32
	uint32_t(__fastcall *readData)(void *object, void *buffer, uint32_t len);
	uint32_t(__fastcall *writeData)(void *object, void *buffer, uint32_t len);
	uint32_t(__fastcall *getSize)(void *object);
	void(__fastcall *setSize)(void *object, uint32_t size);
	uint32_t(__fastcall *getSeek)(void *object);
	void(__fastcall *setSeek)(void *object, int32_t seek, bool relative);
	void (__fastcall *initStream)(void *object, stream_t *stream);
	void (__fastcall *destroyStream)(void *object, stream_t *stream);
#else 
#define __fastcall
//#define gcc_fastcall __attribute__((fastcall))
// wont be used on x64 linux, so using default calling convention for now
	uint32_t (*readData)(void *object, void *buffer, uint32_t len);
	uint32_t (*writeData)(void *object, void *buffer, uint32_t len);
	uint32_t (*getSize)(void *object);
	void (*setSize)(void *object, uint32_t size);
	uint32_t (*getSeek)(void *object);
	void (*setSeek)(void *object, int32_t seek, bool relative);
	void (*initStream)(void *object, stream_t *stream);
	void (*destroyStream)(void *object, stream_t *stream);
#endif
	// general settings
	bool allowCaching;
}streamSettings_t;

typedef struct _stream_t
{
	void *object;
	//_stream_t *substream;
	streamSettings_t *settings;
	// bit access ( write )
	uint8_t bitBuffer[8];
	uint8_t bitIndex;
	// bit access ( read )
	uint8_t bitReadBuffer[8];
	uint8_t bitReadBufferState;
	uint8_t bitReadIndex;
}stream_t;


stream_t*	stream_create	(streamSettings_t *settings, void *object);
void		stream_destroy	(stream_t *stream);

// stream reading

int8_t stream_readS8(stream_t *stream);
int16_t stream_readS16(stream_t *stream);
int32_t stream_readS32(stream_t *stream);
uint8_t stream_readU8(stream_t *stream);
uint16_t stream_readU16(stream_t *stream);
uint32_t stream_readU32(stream_t *stream);
uint64_t stream_readU64(stream_t *stream);
float stream_readFloat(stream_t *stream);
uint32_t stream_readData(stream_t *stream, void *data, uint32_t len);
// stream writing
void stream_writeS8(stream_t *stream, int8_t value);
void stream_writeS16(stream_t *stream, int16_t value);
void stream_writeS32(stream_t *stream, int32_t value);
void stream_writeU8(stream_t *stream, uint8_t value);
void stream_writeU16(stream_t *stream, uint16_t value);
void stream_writeU32(stream_t *stream, uint32_t value);
void stream_writeFloat(stream_t *stream, float value);
uint32_t stream_writeData(stream_t *stream, void *data, uint32_t len);
// stream other
void stream_setSeek(stream_t *stream, int32_t seek);
uint32_t stream_getSeek(stream_t *stream);
uint32_t stream_getSize(stream_t *stream);
void stream_setSize(stream_t *stream, uint32_t size);
void stream_skipData(stream_t *stream, uint32_t len);
uint32_t stream_copy(stream_t* dest, stream_t* source, uint32_t length);

// bit operations
void stream_writeBits(stream_t* stream, uint8_t* bitData, uint32_t bitCount);
void stream_readBits(stream_t* stream, uint8_t* bitData, uint32_t bitCount);



/* stream ex */

stream_t* streamEx_fromMemoryRange(void *mem, uint32_t memoryLimit);
stream_t* streamEx_fromDynamicMemoryRange(uint32_t memoryLimit);
stream_t* streamEx_createSubstream(stream_t* mainstream, int32_t startOffset, int32_t size);

// misc
void* streamEx_map(stream_t* stream, int32_t* size);
int32_t streamEx_readStringNT(stream_t* stream, char* str, uint32_t strSize);
