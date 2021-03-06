#include"global.h"
#include<ctime>
#include<map>

#ifdef _WIN32
#include<intrin.h>
#include<conio.h>
#include<stdio.h>
#include<iostream>
#include<string>
#else
#include <termios.h>    //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>     //STDIN_FILENO
#endif

//used for get_num_cpu
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

primeStats_t primeStats = {0};
volatile int total_shares = 0;
volatile int valid_shares = 0;
unsigned int nMaxSieveSize;
unsigned int nSievePercentage;
bool nPrintDebugMessages;
unsigned long nOverrideTargetValue;
unsigned int nOverrideBTTargetValue;
char* dt;
bool useGetBlockTemplate = true;
uint8_t decodedWalletAddress[32];
size_t decodedWalletAddressLen;

typedef struct  
{
   bool isValidData;
   // block data
   uint32_t version;
   uint32_t height;
   uint32_t nTime;
   uint32_t nBits;
   uint8_t previousBlockHash[32];
   uint8_t target[32]; // sha256 & scrypt
   // coinbase aux
   uint8_t coinbaseauxFlags[128];
   uint32_t coinbaseauxFlagsLength; // in bytes
   // todo: mempool transactions
}getBlockTemplateData_t;

getBlockTemplateData_t getBlockTemplateData = {0};

typedef struct  
{
   char* workername;
   char* workerpass;
   char* host;
   int32_t port;
   bool useXPT;
   int32_t numThreads;
   int32_t sieveSize;
   int32_t sievePercentage;
   int32_t roundSievePercentage;
   int32_t sievePrimeLimit;	// how many primes should be sieved
   unsigned int L1CacheElements;
   unsigned int primorialMultiplier;
   bool enableCacheTunning;
   int32_t targetOverride;
   int32_t targetBTOverride;
   int32_t sieveExtensions;
   bool printDebug;
   // getblocktemplate stuff
   char* xpmAddress; // we will use this XPM address for block payout
}commandlineInput_t;

commandlineInput_t commandlineInput = {0};

bool error(const char *format, ...)
{
	puts(format);
	return false;
}

void hexDump(char *desc, void *addr, int len) {
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char*)addr;

	// Output description if given.
	if (desc != NULL)
		printf("%s:\n", desc);

	// Process every byte in the data.
	for (i = 0; i < len; i++) {
		// Multiple of 16 means new line (with line offset).

		if ((i % 16) == 0) {
			// Just don't print ASCII for the zeroth line.
			if (i != 0)
				printf("  %s\n", buff);

			// Output the offset.
			printf("  %04x ", i);
		}

		// Now the hex code for the specific character.
		printf(" %02x", pc[i]);

		// And store a printable ASCII character for later.
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	// Pad out last line if not exactly 16 characters.
	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	// And print the final ASCII bit.
	printf("  %s\n", buff);
}


bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	bool ret = false;

	while (*hexstr && len) {
		char hex_byte[4];
		unsigned int v;

		if (!hexstr[1]) {
			std::cout << "hex2bin str truncated" << std::endl;
			return ret;
		}

		memset(hex_byte, 0, 4);
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];

		if (sscanf(hex_byte, "%x", &v) != 1) {
			std::cout << "hex2bin sscanf '" << hex_byte << "' failed" << std::endl;
			return ret;
		}

		*p = (unsigned char) v;

		p++;
		hexstr += 2;
		len--;
	}

	if (len == 0 && *hexstr == 0)
		ret = true;
	return ret;
}



uint32_t _swapEndianessU32(uint32_t v)
{
	return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000);
}

uint32_t _getHexDigitValue(uint8_t c)
{
	if( c >= '0' && c <= '9' )
		return c-'0';
	else if( c >= 'a' && c <= 'f' )
		return c-'a'+10;
	else if( c >= 'A' && c <= 'F' )
		return c-'A'+10;
	return 0;
}


static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline bool DecodeBase58(const char* psz, uint8_t* vchRet, size_t* retLength)
{
   CAutoBN_CTX pctx;
   CBigNum bn58 = 58;
   CBigNum bn = 0;
   CBigNum bnChar;
   while (isspace(*psz))
      psz++;
   // Convert big endian string to bignum
   for (const char* p = psz; *p; p++)
   {
      const char* p1 = strchr(pszBase58, *p);
      if (p1 == NULL)
      {
         while (isspace(*p))
            p++;
         if (*p != '\0')
            return false;
         break;
      }
      bnChar.setulong(p1 - pszBase58);
      if (!BN_mul(&bn, &bn, &bn58, pctx))
         throw bignum_error("DecodeBase58 : BN_mul failed");
      bn += bnChar;
   }

   // Get bignum as little endian data
   std::vector<unsigned char> vchTmp = bn.getvch();

   // Trim off sign byte if present
   if (vchTmp.size() >= 2 && vchTmp.end()[-1] == 0 && vchTmp.end()[-2] >= 0x80)
      vchTmp.erase(vchTmp.end()-1);

   // Restore leading zeros
   int nLeadingZeros = 0;
   for (const char* p = psz; *p == pszBase58[0]; p++)
      nLeadingZeros++;
   // Convert little endian data to big endian
   size_t rLen = nLeadingZeros + vchTmp.size();
   for(size_t i=0; i<rLen; i++)
   {
      vchRet[rLen-i-1] = vchTmp[i];
   }
   *retLength = rLen;
   return true;
}

/*
* Parses a hex string
* Length should be a multiple of 2
*/
void jhMiner_parseHexString(char* hexString, uint32_t length, uint8_t* output)
{
	uint32_t lengthBytes = length / 2;
	for (uint32_t i = 0; i<lengthBytes; i++)
   {
      // high digit
		uint32_t d1 = _getHexDigitValue(hexString[i * 2 + 0]);
      // low digit
		uint32_t d2 = _getHexDigitValue(hexString[i * 2 + 1]);
      // build byte
		output[i] = (uint8_t)((d1 << 4) | (d2));
   }
}

/*
* Parses a hex string and converts it to LittleEndian (or just opposite endianness)
* Length should be a multiple of 2
*/
void jhMiner_parseHexStringLE(char* hexString, uint32_t length, uint8_t* output)
{
	uint32_t lengthBytes = length / 2;
	for (uint32_t i = 0; i<lengthBytes; i++)
   {
      // high digit
		uint32_t d1 = _getHexDigitValue(hexString[i * 2 + 0]);
      // low digit
		uint32_t d2 = _getHexDigitValue(hexString[i * 2 + 1]);
      // build byte
		output[lengthBytes - i - 1] = (uint8_t)((d1 << 4) | (d2));
   }
}


void primecoinBlock_generateHeaderHash(primecoinBlock_t* primecoinBlock, uint8_t hashOutput[32])
{
	uint8_t blockHashDataInput[512];
   memcpy(blockHashDataInput, primecoinBlock, 80);
   sha256_context ctx;
   sha256_starts(&ctx);
   sha256_update(&ctx, (uint8_t*)blockHashDataInput, 80);
   sha256_finish(&ctx, hashOutput);
   sha256_starts(&ctx); // is this line needed?
   sha256_update(&ctx, hashOutput, 32);
   sha256_finish(&ctx, hashOutput);
}

void primecoinBlock_generateBlockHash(primecoinBlock_t* primecoinBlock, uint8_t hashOutput[32])
{
	uint8_t blockHashDataInput[512];
   memcpy(blockHashDataInput, primecoinBlock, 80);
   uint32_t writeIndex = 80;
   uint32_t lengthBN = 0;
   CBigNum bnPrimeChainMultiplier;
   bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
   std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
   lengthBN = (uint32_t)bnSerializeData.size();
   *(uint8_t*)(blockHashDataInput + writeIndex) = (uint8_t)lengthBN;
   writeIndex += 1;
   memcpy(blockHashDataInput+writeIndex, &bnSerializeData[0], lengthBN);
   writeIndex += lengthBN;
   sha256_context ctx;
   sha256_starts(&ctx);
   sha256_update(&ctx, (uint8_t*)blockHashDataInput, writeIndex);
   sha256_finish(&ctx, hashOutput);
   sha256_starts(&ctx); // is this line needed?
   sha256_update(&ctx, hashOutput, 32);
   sha256_finish(&ctx, hashOutput);
}

typedef struct  
{
   bool dataIsValid;
   uint8_t data[128];
   uint32_t dataHash; // used to detect work data changes
   uint8_t serverData[32]; // contains data from the server
}workDataEntry_t;

typedef struct  
{
#ifdef _WIN32
	CRITICAL_SECTION cs;
#else
  pthread_mutex_t cs;
#endif
  uint8_t protocolMode;
	// xpm
	workDataEntry_t workEntry[128]; // work data for each thread (up to 128)
	// x.pushthrough
	xptClient_t* xptClient;
}workData_t;

#define MINER_PROTOCOL_GETWORK		(1)
#define MINER_PROTOCOL_STRATUM		(2)
#define MINER_PROTOCOL_XPUSHTHROUGH	(3)
#define MINER_PROTOCOL_GBT			(4)

bool bSoloMining = false;
workData_t workData;
int lastBlockCount = 0;

jsonRequestTarget_t jsonRequestTarget; // rpc login data
//jsonRequestTarget_t jsonRequestTarget = {0}; // rpc login data
//jsonRequestTarget_t jsonLocalPrimeCoin; // rpc login data
bool useLocalPrimecoindForLongpoll;

/*
* Pushes the found block data to the server for giving us the $$$
* Uses getwork to push the block
* Returns true on success
* Note that the primecoin data can be larger due to the multiplier at the end, so we use 512 bytes per default
* 29.sep: switched to 512 bytes per block as default, since Primecoin can use up to 2000 bits (250 bytes) for the multiplier chain + length prefix of 2 bytes
*/
bool jhMiner_pushShare_primecoin(uint8_t data[512], primecoinBlock_t* primecoinBlock)
{
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
   {
      // prepare buffer to send
      fStr_buffer4kb_t fStrBuffer_parameter;
      fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
      fStr_append(fStr_parameter, "[\"");
      fStr_addHexString(fStr_parameter, data, 512);
      fStr_appendFormatted(fStr_parameter, "\",\"");
	  fStr_addHexString(fStr_parameter, (uint8_t*)&primecoinBlock->serverData, 32);
      fStr_append(fStr_parameter, "\"]");
      // send request
	  int32_t rpcErrorCode = 0;
      jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", fStr_parameter, &rpcErrorCode);
      if( jsonReturnValue == NULL )
      {
         printf("PushWorkResult failed :(\n");
         return false;
      }
      else
      {
         // rpc call worked, sooooo.. is the server happy with the result?
         jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
         if( jsonObject_isTrue(jsonReturnValueBool) )
         {
            total_shares++;
            valid_shares++;
            time_t now = time(0);
            dt = ctime(&now);
            //JLR DBG
            printf("Valid share found!");
            printf("[ %d / %d ] %s",valid_shares, total_shares,dt);
            jsonObject_freeObject(jsonReturnValue);
            return true;
         }
         else
         {
            total_shares++;
            // the server says no to this share :(
            printf("Server rejected share (BlockHeight: %d/%d nBits: 0x%08X)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
            jsonObject_freeObject(jsonReturnValue);
            return false;
         }
      }
      jsonObject_freeObject(jsonReturnValue);
      return false;
   }
   else if( workData.protocolMode == MINER_PROTOCOL_GBT )
   {
      // use submitblock
      char* methodName = "submitblock";
      // get multiplier
      CBigNum bnPrimeChainMultiplier;
      bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
      std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
	  uint32_t lengthBN = (uint32_t)bnSerializeData.size();
      //memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
      //xptShareToSubmit->chainMultiplierSize = lengthBN;
      // prepare raw data of block
	  uint8_t dataRaw[512] = { 0 };
      //uint8_t proofOfWorkHash[32];
      bool shareAccepted = false;
      memset(dataRaw, 0x00, sizeof(dataRaw));
	  *(uint32_t*)(dataRaw + 0) = primecoinBlock->version;
      memcpy((dataRaw+4), primecoinBlock->prevBlockHash, 32);
      memcpy((dataRaw+36), primecoinBlock->merkleRoot, 32);
	  *(uint32_t*)(dataRaw + 68) = primecoinBlock->timestamp;
	  *(uint32_t*)(dataRaw + 72) = primecoinBlock->nBits;
	  *(uint32_t*)(dataRaw + 76) = primecoinBlock->nonce;
	  *(uint8_t*)(dataRaw + 80) = (uint8_t)lengthBN;
      if( lengthBN > 0x7F )
         printf("Warning: chainMultiplierSize exceeds 0x7F in jhMiner_pushShare_primecoin()\n");
      memcpy(dataRaw+81, &bnSerializeData[0], lengthBN);
      // create stream to write block data to
      stream_t* blockStream = streamEx_fromDynamicMemoryRange(1024*64);
      // write block data
      stream_writeData(blockStream, dataRaw, 80+1+lengthBN);
      // generate coinbase transaction
      bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock->seed, primecoinBlock->threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock->nBits));
      // write amount of transactions (varInt)
      bitclient_addVarIntFromStream(blockStream, 1);
      bitclient_writeTransactionToStream(blockStream, txCoinbase);
      // map buffer
	  int32_t blockDataLength = 0;
	  uint8_t* blockData = (uint8_t*)streamEx_map(blockStream, &blockDataLength);
      // clean up
      bitclient_destroyTransaction(txCoinbase);
      // prepare buffer to send
      fStr_buffer4kb_t fStrBuffer_parameter;
      fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
      fStr_append(fStr_parameter, "[\""); // \"]
      fStr_addHexString(fStr_parameter, blockData, blockDataLength);
      fStr_append(fStr_parameter, "\"]");
      // send request
	  int32_t rpcErrorCode = 0;
      jsonObject_t* jsonReturnValue = NULL;
      jsonReturnValue = jsonClient_request(&jsonRequestTarget, methodName, fStr_parameter, &rpcErrorCode);		
      // clean up rest
      stream_destroy(blockStream);
      free(blockData);
      // process result
      if( jsonReturnValue == NULL )
      {
         printf("SubmitBlock failed :(\n");
         return false;
      }
      else
      {
         // is the bitcoin client happy with the result?
         jsonObject_t* jsonReturnValueRejectReason = jsonObject_getParameter(jsonReturnValue, "result");
         if( jsonObject_getType(jsonReturnValueRejectReason) == JSON_TYPE_NULL )
         {
            printf("Valid block found!\n");
            jsonObject_freeObject(jsonReturnValue);
            return true;
         }
         else
         {
            // :( the client says no
            printf("Coin daemon rejected block :(\n");
            jsonObject_freeObject(jsonReturnValue);
            return false;
         }
      }

   }
   else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH ) {
	// printf("Queue share\n");
		xptShareToSubmit_t* xptShareToSubmit = (xptShareToSubmit_t*)malloc(sizeof(xptShareToSubmit_t));
		memset(xptShareToSubmit, 0x00, sizeof(xptShareToSubmit_t));
		memcpy(xptShareToSubmit->merkleRoot, primecoinBlock->merkleRoot, 32);
		memcpy(xptShareToSubmit->prevBlockHash, primecoinBlock->prevBlockHash, 32);
		xptShareToSubmit->version = primecoinBlock->version;
		xptShareToSubmit->nBits = primecoinBlock->nBits;
		xptShareToSubmit->nonce = primecoinBlock->nonce;
		xptShareToSubmit->nTime = primecoinBlock->timestamp;
		// set multiplier
		CBigNum bnPrimeChainMultiplier;
		bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
		std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
		size_t lengthBN = bnSerializeData.size();
		memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
		xptShareToSubmit->chainMultiplierSize = (unsigned char)lengthBN;
		// todo: Set stuff like sieve size
		if (workData.xptClient && !workData.xptClient->disconnected) 
		{
			xptClient_foundShare(workData.xptClient, xptShareToSubmit);
			return true;
		} 
		else
		{
			std::cout << "Share submission failed. The client is not connected to the pool." << std::endl;
		}
   }
   return false;
}

int queryLocalPrimecoindBlockCount(bool useLocal)
{
	int32_t rpcErrorCode = 0;
   jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblockcount", NULL, &rpcErrorCode);
   if( jsonReturnValue == NULL )
   {
      printf("getblockcount() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
      return 0;
   }
   else
   {
      jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
      return (int) jsonObject_getNumberValueAsS32(jsonResult);
      jsonObject_freeObject(jsonReturnValue);
   }

   return 0;
}

static double DIFFEXACTONE = 26959946667150639794667015087019630673637144422540572481103610249215.0;
static const uint64_t diffone = 0xFFFF000000000000ull;

static double target_diff(const unsigned char *target)
{
   double targ = 0;
   signed int i;

   for (i = 31; i >= 0; --i)
      targ = (targ * 0x100) + target[i];

   return DIFFEXACTONE / (targ ? targ: 1);
}

double target_diff(const uint32_t  *target)
{
   double targ = 0;
   signed int i;

   for (i = 0; i < 8; i++)
      targ = (targ * 0x100) + target[7 - i];

   return DIFFEXACTONE / ((double)targ ?  targ : 1);
}


std::string HexBits(unsigned int nBits)
{
   union {
      int32_t nBits;
      char cBits[4];
   } uBits;
   uBits.nBits = htonl((int32_t)nBits);
   return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}

static bool IsXptClientConnected()
{
#ifdef _WIN32
	__try
	{
#endif
		if (workData.xptClient == NULL || workData.xptClient->disconnected)
			return false;
#ifdef _WIN32
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
#endif

	return true;
}


int getNumThreads(void) {
  // based on code from ceretullis on SO
  uint32_t numcpu = 1; // in case we fall through;
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  int mib[4];
  size_t len = sizeof(numcpu); 

  /* set the mib for hw.ncpu */
  mib[0] = CTL_HW;
#ifdef HW_AVAILCPU
  mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;
#else
  mib[1] = HW_NCPU;
#endif
  /* get the number of CPUs from the system */
sysctl(mib, 2, &numcpu, &len, NULL, 0);

    if( numcpu < 1 )
    {
      numcpu = 1;
    }

#elif defined(__linux__) || defined(sun) || defined(__APPLE__)
  numcpu = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
#elif defined(_SYSTYPE_SVR4)
  numcpu = sysconf( _SC_NPROC_ONLN );
#elif defined(hpux)
  numcpu = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#elif defined(_WIN32)
  SYSTEM_INFO sysinfo;
  GetSystemInfo( &sysinfo );
  numcpu = sysinfo.dwNumberOfProcessors;
#endif
  
  return numcpu;
}

/*
* Queries the work data from the coin client
* Uses "getblocktemplate"
* Should be called periodically (5-15 seconds) to keep the current block data up-to-date
*/
void jhMiner_queryWork_primecoin_getblocktemplate()
{
	int32_t rpcErrorCode = 0;
   fStr_buffer4kb_t fStrBuffer_parameter;
   fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
   fStr_append(fStr_parameter, "[{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\"]}]");
   jsonObject_t* jsonReturnValue = NULL;
   jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblocktemplate", fStr_parameter, &rpcErrorCode);
   if( jsonReturnValue == NULL )
   {
      printf("UpdateWork(GetBlockTemplate) failed. Error code %d\n", rpcErrorCode);
      getBlockTemplateData.isValidData = false;
      return;
   }
   else
   {
      jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
      // data
      jsonObject_t* jsonResult_version = jsonObject_getParameter(jsonResult, "version");
      jsonObject_t* jsonResult_previousblockhash = jsonObject_getParameter(jsonResult, "previousblockhash");
      jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
      //jsonObject_t* jsonResult_mintime = jsonObject_getParameter(jsonResult, "mintime");
      jsonObject_t* jsonResult_curtime = jsonObject_getParameter(jsonResult, "curtime");
      jsonObject_t* jsonResult_bits = jsonObject_getParameter(jsonResult, "bits");
      jsonObject_t* jsonResult_height = jsonObject_getParameter(jsonResult, "height");
      jsonObject_t* jsonResult_coinbaseaux = jsonObject_getParameter(jsonResult, "coinbaseaux");
      jsonObject_t* jsonResult_coinbaseaux_flags = NULL;
      if( jsonResult_coinbaseaux )
         jsonResult_coinbaseaux_flags = jsonObject_getParameter(jsonResult_coinbaseaux, "flags");
      // are all fields present?
      if( jsonResult_version == NULL || jsonResult_previousblockhash == NULL || jsonResult_curtime == NULL || jsonResult_bits == NULL || jsonResult_height == NULL || jsonResult_coinbaseaux_flags == NULL )
      {
         printf("UpdateWork(GetBlockTemplate) failed due to missing fields in the response.\n");
         jsonObject_freeObject(jsonReturnValue);
      }
      // prepare field lengths
	  uint32_t stringLength_previousblockhash = 0;
	  uint32_t stringLength_target = 0;
	  uint32_t stringLength_bits = 0;
	  uint32_t stringLength_height = 0;
      // get version
	  uint32_t gbtVersion = jsonObject_getNumberValueAsS32(jsonResult_version);
      // get previous block hash
	  uint8_t* stringData_previousBlockHash = jsonObject_getStringData(jsonResult_previousblockhash, &stringLength_previousblockhash);
      memset(getBlockTemplateData.previousBlockHash, 0, 32);
      jhMiner_parseHexStringLE((char*)stringData_previousBlockHash, stringLength_previousblockhash, getBlockTemplateData.previousBlockHash);
      // get target hash (optional)
	  uint8_t* stringData_target = jsonObject_getStringData(jsonResult_target, &stringLength_target);
      ///(getBlockTemplateData.target, 32);
      memset(getBlockTemplateData.target, 0x00, 32);
      if( stringData_target )
         jhMiner_parseHexStringLE((char*)stringData_target, stringLength_target, getBlockTemplateData.target);
      // get timestamp (mintime)
	  uint32_t gbtTime = jsonObject_getNumberValueAsU32(jsonResult_curtime);
      // get bits
      char bitsTmpText[32]; // temporary buffer so we can add NT
	  uint8_t* stringData_bits = jsonObject_getStringData(jsonResult_bits, &stringLength_bits);
      memcpy(bitsTmpText, stringData_bits, stringLength_bits);
      bitsTmpText[stringLength_bits] = '\0'; 
	  uint32_t gbtBits = 0;
      sscanf((const char*)bitsTmpText, "%x", &gbtBits);
      // get height
	  uint32_t gbtHeight = jsonObject_getNumberValueAsS32(jsonResult_height);
      // get coinbase aux flags
	  uint32_t stringLength_coinbaseauxFlags = 0;
	  uint8_t* stringData_coinbaseauxFlags = jsonObject_getStringData(jsonResult_coinbaseaux_flags, &stringLength_coinbaseauxFlags);
      jhMiner_parseHexString((char*)stringData_coinbaseauxFlags, stringLength_coinbaseauxFlags, getBlockTemplateData.coinbaseauxFlags);
      getBlockTemplateData.coinbaseauxFlagsLength = stringLength_coinbaseauxFlags/2;
      // set remaining number parameters
      getBlockTemplateData.version = gbtVersion;
      getBlockTemplateData.nBits = gbtBits;
      getBlockTemplateData.nTime = gbtTime;
      getBlockTemplateData.height = gbtHeight;
      // done
      jsonObject_freeObject(jsonReturnValue);
      getBlockTemplateData.isValidData = true;
   }
}

void jhMiner_queryWork_primecoin_getwork()
{
	int32_t rpcErrorCode = 0;
   uint64_t time1 = getTimeMilliseconds();
   jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", NULL, &rpcErrorCode);
   uint64_t time2 = getTimeMilliseconds() - time1;
   // printf("request time: %dms\n", time2);
   if( jsonReturnValue == NULL )
   {
      printf("Getwork() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
      workData.workEntry[0].dataIsValid = false;
      return;
   }
   else
   {
      jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
      jsonObject_t* jsonResult_data = jsonObject_getParameter(jsonResult, "data");
      //jsonObject_t* jsonResult_hash1 = jsonObject_getParameter(jsonResult, "hash1");
      jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
      jsonObject_t* jsonResult_serverData = jsonObject_getParameter(jsonResult, "serverData");
      //jsonObject_t* jsonResult_algorithm = jsonObject_getParameter(jsonResult, "algorithm");
      if( jsonResult_data == NULL )
      {
         printf("Error :(\n");
         workData.workEntry[0].dataIsValid = false;
         jsonObject_freeObject(jsonReturnValue);
         return;
      }
      // data
	  uint32_t stringData_length = 0;
	  uint8_t* stringData_data = jsonObject_getStringData(jsonResult_data, &stringData_length);
      //JLR DBG
	  printf("data: %.*s...\n", (int32_t)std::min(48, (int)stringData_length), stringData_data);

#ifdef _WIN32
      EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
      jhMiner_parseHexString((char*)stringData_data, std::min(128*2, (int)stringData_length), workData.workEntry[0].data);
      workData.workEntry[0].dataIsValid = true;
      if (jsonResult_serverData == NULL)
      {
         unsigned char binDataReverse[128];
         for (unsigned int i = 0; i < 128 / 4;++i) 
            ((unsigned int *)binDataReverse)[i] = _swapEndianessU32(((unsigned int *)workData.workEntry[0].data)[i]);
         blockHeader_t * blockHeader = (blockHeader_t *)&binDataReverse[0];

         memset(workData.workEntry[0].serverData, 0, 32);
         ((serverData_t*)workData.workEntry[0].serverData)->nBitsForShare = blockHeader->nBits;
         ((serverData_t*)workData.workEntry[0].serverData)->blockHeight = lastBlockCount;
         useLocalPrimecoindForLongpoll = false;
         bSoloMining = true;

      }
      else
      {
         // get server data
		  uint32_t stringServerData_length = 0;
		  uint8_t* stringServerData_data = jsonObject_getStringData(jsonResult_serverData, &stringServerData_length);
         memset(workData.workEntry[0].serverData, 0, 32);
         if( jsonResult_serverData )
            jhMiner_parseHexString((char*)stringServerData_data, std::min(128*2, 32*2), workData.workEntry[0].serverData);
      }
      // generate work hash
	  uint32_t workDataHash = 0x5B7C8AF4;
	  for (uint32_t i = 0; i<stringData_length / 2; i++)
      {
         workDataHash = (workDataHash>>29)|(workDataHash<<3);
		 workDataHash += (uint32_t)workData.workEntry[0].data[i];
      }
      workData.workEntry[0].dataHash = workDataHash;
#ifdef _WIN32
      LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif

      jsonObject_freeObject(jsonReturnValue);
   }
}

bool SubmitBlock(primecoinBlock_t* pcBlock)
{
   blockHeader_t block = {0};
   memcpy(&block, pcBlock, 80);
   CBigNum bnPrimeChainMultiplier;
   bnPrimeChainMultiplier.SetHex(pcBlock->mpzPrimeChainMultiplier.get_str(16));
   std::vector<unsigned char> primemultiplier = bnPrimeChainMultiplier.getvch();

   //JLR DBG
   printf("nBits: %d\n", block.nBits);
   printf("nNonce: %d\n", block.nonce);
   printf("hashPrevBlock: %s\n", block.prevBlockHash.GetHex().c_str());
   printf("block   - hashMerkleRoot: %s\n", block.merkleRoot.GetHex().c_str());
   printf("pcBlock - hashMerkleRoot: %s\n",  HexStr(BEGIN(pcBlock->merkleRoot), END(pcBlock->merkleRoot)).c_str());
   printf("Multip: %s\n", bnPrimeChainMultiplier.GetHex().c_str());

   if (primemultiplier.size() > 47) {
      error("primemultiplier is too big");
      return false;
   }

   block.primeMultiplier[0] = (unsigned char)primemultiplier.size();

   for (size_t i = 0; i < primemultiplier.size(); ++i) 
      block.primeMultiplier[1 + i] = primemultiplier[i];

   for (unsigned int i = 0; i < 128 / 4; ++i) ((unsigned int *)&block)[i] =
      _swapEndianessU32(((unsigned int *)&block)[i]);

   unsigned char pdata[128] = {0};
   memcpy(pdata, &block, 128);
   std::string data_hex = HexStr(BEGIN(pdata), END(pdata));

   fStr_buffer4kb_t fStrBuffer_parameter;
   fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
   fStr_append(fStr_parameter, "[\""); 
   fStr_append(fStr_parameter, (char *) data_hex.c_str());
   fStr_append(fStr_parameter, "\"]");

   // send request
   int32_t rpcErrorCode = 0;
   //jhMiner_pushShare_primecoin((unsigned char *)data_hex.c_str(), pcBlock);
   //jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "submitblock", fStr_parameter, &rpcErrorCode);
   jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", fStr_parameter, &rpcErrorCode);
   if( jsonReturnValue == NULL )
   {
      printf("PushWorkResult failed :(\n");
      return false;
   }
   else
   {
      // rpc call worked, sooooo.. is the server happy with the result?
      jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
      if( jsonObject_isTrue(jsonReturnValueBool) )
      {
         total_shares++;
         valid_shares++;
         printf("Submit block succeeded! :)\n");
         jsonObject_freeObject(jsonReturnValue);
         return true;
      }
      else
      {
         total_shares++;
         // the server says no to this share :(
         printf("Server rejected the Block. :(\n");
         jsonObject_freeObject(jsonReturnValue);
         return false;
      }
   }
   jsonObject_freeObject(jsonReturnValue);
   return false;
}


/*
* Returns the block height of the most recently received workload
*/
uint32_t jhMiner_getCurrentWorkBlockHeight(int32_t threadIndex)
{
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
      return ((serverData_t*)workData.workEntry[0].serverData)->blockHeight;	
   else if( workData.protocolMode == MINER_PROTOCOL_GBT )
      return getBlockTemplateData.height;	
   else
      return ((serverData_t*)workData.workEntry[threadIndex].serverData)->blockHeight;
}

/*
* Worker thread mainloop for getwork() mode
*/
#ifdef _WIN32
int jhMiner_workerThread_getwork(int threadIndex){
#else
void *jhMiner_workerThread_getwork(void *arg){
uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif


	CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
	   uint8_t localBlockData[128];
      // copy block data from global workData
	   uint32_t workDataHash = 0;
	   uint8_t serverData[32];
//JLR DEBUG
//printf ("jhMiner_workerThread_getwork() TOP!\n");
      while( workData.workEntry[0].dataIsValid == false ) Sleep(200);
#ifdef _WIN32
      EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
      memcpy(localBlockData, workData.workEntry[0].data, 128);
      //seed = workData.seed;
      memcpy(serverData, workData.workEntry[0].serverData, 32);
#ifdef _WIN32
      LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
      // swap endianess
	for (uint32_t i = 0; i<128 / 4; i++)
      {
		*(uint32_t*)(localBlockData + i * 4) = _swapEndianessU32(*(uint32_t*)(localBlockData + i * 4));
      }
      // convert raw data into primecoin block
      primecoinBlock_t primecoinBlock = {0};
      memcpy(&primecoinBlock, localBlockData, 80);
      // we abuse the timestamp field to generate an unique hash for each worker thread...
      primecoinBlock.timestamp += threadIndex;
      primecoinBlock.threadIndex = threadIndex;
      primecoinBlock.xptMode = (workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH);
      // ypool uses a special encrypted serverData value to speedup identification of merkleroot and share data
      memcpy(&primecoinBlock.serverData, serverData, 32);
      // start mining
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
      {
//JLR
//         printf ("jhMiner_workerThread_getwork() BREAK!\n");
         break;
      }
      primecoinBlock.mpzPrimeChainMultiplier = 0;
   }
   if( psieve )
   {
      delete psieve;
      psieve = NULL;
   }
   return 0;
}

static const int64_t PRIMECOIN_COIN = 100000000;
static const int64_t PRIMECOIN_CENT = 1000000;
static const unsigned int PRIMECOIN_nFractionalBits = 24;

/*
* Returns value of block
*/
uint64_t jhMiner_primeCoin_targetGetMint(unsigned int nBits)
{
   if( nBits == 0 )
      return 0;
   uint64_t nMint = 0;
   static uint64_t nMintLimit = 999ull * PRIMECOIN_COIN;
   uint64_t bnMint = nMintLimit;
   bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
   bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
   bnMint = (bnMint / PRIMECOIN_CENT) * PRIMECOIN_CENT;  // mint value rounded to cent
   nMint = bnMint;
   return nMint;
}

/*
* Worker thread mainloop for getblocktemplate mode
*/
#ifdef _WIN32
int jhMiner_workerThread_gbt(int threadIndex){
#else
void *jhMiner_workerThread_gbt(void *arg){ 
uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif
   CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
// JLR
//printf("jhMiner_workerThread_gbt() TOP\n");
      //uint8_t localBlockData[128];
      primecoinBlock_t primecoinBlock = {0};
      // copy block data from global workData
      //uint32_t workDataHash = 0;
      //uint8_t serverData[32];
      while( getBlockTemplateData.isValidData == false ) Sleep(200);
#ifdef _WIN32
      EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
      // generate work from getBlockTemplate data
      primecoinBlock.threadIndex = threadIndex;
      primecoinBlock.version = getBlockTemplateData.version;
      primecoinBlock.timestamp = getBlockTemplateData.nTime;
      primecoinBlock.nonce = 0;
      primecoinBlock.seed = rand();
      primecoinBlock.nBits = getBlockTemplateData.nBits;
      memcpy(primecoinBlock.prevBlockHash, getBlockTemplateData.previousBlockHash, 32);
      // setup serverData struct
      primecoinBlock.serverData.blockHeight = getBlockTemplateData.height;
      primecoinBlock.serverData.nBitsForShare = getBlockTemplateData.nBits;
      // generate coinbase transaction and merkleroot
      bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock.seed, threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock.nBits));
      bitclientTransaction_t* txList[64];
      txList[0] = txCoinbase;
	  uint32_t numberOfTx = 1;
      // generate tx hashes (currently we only support coinbase transaction)
	  uint8_t txHashList[64 * 32];
	  for (uint32_t t = 0; t<numberOfTx; t++)
         bitclient_generateTxHash(txList[t], (txHashList+t*32));
      bitclient_calculateMerkleRoot(txHashList, numberOfTx, primecoinBlock.merkleRoot);
      bitclient_destroyTransaction(txCoinbase);
#ifdef _WIN32
      LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
      primecoinBlock.xptMode = false;
      // start mining
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
      {
//JLR
//printf("jhMiner_workerThread_gbt() BREAK\n");
         break;
      }
      primecoinBlock.mpzPrimeChainMultiplier = 0;
   }
   if( psieve )
   {
      delete psieve;
      psieve = NULL;
   }
   return 0;
}

/*
 * Worker thread mainloop for xpt() mode
 */
#ifdef _WIN32
int jhMiner_workerThread_xpt(int threadIndex){
#else
void *jhMiner_workerThread_xpt(void *arg){
uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif

   CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
	   uint8_t localBlockData[128];
      // copy block data from global workData
	   uint32_t workDataHash = 0;
	   uint8_t serverData[32];
//JLR
//printf("jhMiner_workerThread_xpt() TOP\n");
      while( workData.workEntry[threadIndex].dataIsValid == false ) Sleep(50);
#ifdef _WIN32
      EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
      memcpy(localBlockData, workData.workEntry[threadIndex].data, 128);
      memcpy(serverData, workData.workEntry[threadIndex].serverData, 32);
      workDataHash = workData.workEntry[threadIndex].dataHash;
#ifdef _WIN32
      LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
      // convert raw data into primecoin block
      primecoinBlock_t primecoinBlock = {0};
      memcpy(&primecoinBlock, localBlockData, 80);
      // we abuse the timestamp field to generate an unique hash for each worker thread...
      primecoinBlock.timestamp += threadIndex;
      primecoinBlock.threadIndex = threadIndex;
      primecoinBlock.xptMode = (workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH);
      // ypool uses a special encrypted serverData value to speedup identification of merkleroot and share data
      memcpy(&primecoinBlock.serverData, serverData, 32);
      // start mining
      //JLR DBG
      uint64_t time1 = getTimeMilliseconds();
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
      {
//JLR
//printf("jhMiner_workerThread_xpt() BREAK\n");
         break;
       }
      //JLR DBG
      //printf("Mining stopped after %dms\n", getTimeMilliseconds()-time1);
      primecoinBlock.mpzPrimeChainMultiplier = 0;
   }
   if( psieve )
   {
      delete psieve;
      psieve = NULL;
   }
   return 0;
}


void jhMiner_printHelp()
{
	using namespace std;

   puts("Usage: jhPrimeminer.exe [options]");
   puts("Options:");
   puts("   -o, -O                        The miner will connect to this url");
   puts("                                 You can specifiy an port after the url using -o url:port");
   puts("   -xpt                          Use x.pushthrough protocol");
   puts("   -u                            The username (workername) used for login");
   puts("   -p                            The password used for login");
   puts("   -t <num>                      The number of threads for mining (default 1)");
   puts("                                     For most efficient mining, set to number of CPU cores");
   puts("   -s <num>                      Set MaxSieveSize range from 200000 - 10000000");
   puts("                                     Default is 1500000.");
   puts("   -d <num>                      Set SievePercentage - range from 1 - 100");
   puts("                                     Default is 15 and it's not recommended to use lower values than 8.");
   puts("                                     It limits how many base primes are used to filter out candidate multipliers in the sieve.");
   puts("   -r <num>                      Set RoundSievePercentage - range from 3 - 97");
   puts("                                     The parameter determines how much time is spent running the sieve.");
   puts("                                     By default 80% of time is spent in the sieve and 20% is spent on checking the candidates produced by the sieve");
   puts("   -primes <num>                 Sets how many prime factors are used to filter the sieve");
   puts("                                     Default is MaxSieveSize. Valid range: 300 - 200000000");
   puts("   -xpm <wallet address>         When doing solo mining this is the address your mined XPM will be transfered to.");
   puts("Example usage:");



#ifndef _WIN32
	cout << "  jhprimeminer -o http://poolurl.com:10034 -u workername -p workerpass" << endl;
#else
	cout << "  jhPrimeminer.exe -o http://poolurl.com:8332 -u workername.1 -p workerpass -t 4" << endl;
puts("Press any key to continue...");
	getchar();
#endif
}

void jhMiner_parseCommandline(int argc, char **argv)
{
	using namespace std;
	int32_t cIdx = 1;
	while( cIdx < argc )
	{
		char* argument = argv[cIdx];
		cIdx++;
		if( memcmp(argument, "-o", 3)==0 || memcmp(argument, "-O", 3)==0 )
		{
			// -o
			if( cIdx >= argc )
			{
				cout << "Missing URL after -o option" << endl;
				exit(0);
			}
			if( strstr(argv[cIdx], "http://") )
				commandlineInput.host = fStrDup(strstr(argv[cIdx], "http://")+7);
			else
				commandlineInput.host = fStrDup(argv[cIdx]);
			char* portStr = strstr(commandlineInput.host, ":");
			if( portStr )
			{
				*portStr = '\0';
				commandlineInput.port = atoi(portStr+1);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-u", 3)==0 )
		{
			// -u
			if( cIdx >= argc )
			{
				cout << "Missing username/workername after -u option" << endl;
				exit(0);
			}
			commandlineInput.workername = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-p", 3)==0 )
		{
			// -p
			if( cIdx >= argc )
			{
				cout << "Missing password after -p option" << endl;
				exit(0);
			}
			commandlineInput.workerpass = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
      else if( memcmp(argument, "-xpm", 5)==0 )
      {
         // -xpm
         if( cIdx >= argc )
         {
            cout << "Missing wallet address after -xpm option\n";
            exit(0);
         }
         commandlineInput.xpmAddress = fStrDup(argv[cIdx], 64);
         cIdx++;
      }
		else if( memcmp(argument, "-t", 3)==0 )
		{
			// -t
			if( cIdx >= argc )
			{
				cout << "Missing thread number after -t option" << endl;
				exit(0);
			}
			commandlineInput.numThreads = atoi(argv[cIdx]);
			if( commandlineInput.numThreads < 1 || commandlineInput.numThreads > 128 )
			{
				cout << "-t parameter out of range" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-s", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -s option" << endl;
				exit(0);
			}
			commandlineInput.sieveSize = atoi(argv[cIdx]);
         if( commandlineInput.sieveSize < 200000 || commandlineInput.sieveSize > 40000000 )
         {
            printf("-s parameter out of range, must be between 200000 - 10000000");
            exit(0);
         }
			cIdx++;
		}
		else if( memcmp(argument, "-d", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -d option" << endl;
				exit(0);
			}
			commandlineInput.sievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.sievePercentage < 1 || commandlineInput.sievePercentage > 100 )
			{
				cout << "-d parameter out of range, must be between 1 - 100" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-r", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -r option" << endl;
				exit(0);
			}
			commandlineInput.roundSievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.roundSievePercentage < 3 || commandlineInput.roundSievePercentage > 97 )
			{
				cout << "-r parameter out of range, must be between 3 - 97" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-primes", 8)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -primes option" << endl;
				exit(0);
			}
			commandlineInput.sievePrimeLimit = atoi(argv[cIdx]);
			if( commandlineInput.sievePrimeLimit < 300 || commandlineInput.sievePrimeLimit > 200000000 )
			{
				cout << "-primes parameter out of range, must be between 300 - 200000000" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-c", 3)==0 )
		{
			// -c
			if( cIdx >= argc )
			{
				cout << "Missing number after -c option" << endl;
				exit(0);
			}
			commandlineInput.L1CacheElements = atoi(argv[cIdx]);
			if( commandlineInput.L1CacheElements < 300 || commandlineInput.L1CacheElements > 200000000  || commandlineInput.L1CacheElements % 32 != 0) 
			{
				cout << "-c parameter out of range, must be between 64000 - 2000000 and multiply of 32" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-m", 3)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -m option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier = atoi(argv[cIdx]);
			if( commandlineInput.primorialMultiplier < 5 || commandlineInput.primorialMultiplier > 1009) 
			{
				cout << "-m parameter out of range, must be between 5 - 1009 and should be a prime number" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-tune", 6)==0 )
		{
			// -tune
			commandlineInput.enableCacheTunning = true;
			cIdx++;
		}
      else if( memcmp(argument, "-target", 8)==0 )
      {
         // -target
         if( cIdx >= argc )
         {
            cout << "Missing number after -target option" << endl;
            exit(0);
         }
         commandlineInput.targetOverride = atoi(argv[cIdx]);
         if( commandlineInput.targetOverride < 0 || commandlineInput.targetOverride > 100 )
         {
            printf("-target parameter out of range, must be between 0 - 100");
            exit(0);
         }
         cIdx++;
      }
      else if( memcmp(argument, "-bttarget", 10)==0 )
      {
         // -bttarget
         if( cIdx >= argc )
         {
            printf("Missing number after -bttarget option\n");
            exit(0);
         }
         commandlineInput.targetBTOverride = atoi(argv[cIdx]);
         if( commandlineInput.targetBTOverride < 0 || commandlineInput.targetBTOverride > 100 )
         {
            printf("-bttarget parameter out of range, must be between 0 - 100");
            exit(0);
         }
         cIdx++;
      }
      else if( memcmp(argument, "-se", 4)==0 )
      {
         // -target
         if( cIdx >= argc )
         {
            printf("Missing number after -se option\n");
            exit(0);
         }
         commandlineInput.sieveExtensions = atoi(argv[cIdx]);
         if( commandlineInput.sieveExtensions < 1 || commandlineInput.sieveExtensions > 15 )
         {
            printf("-se parameter out of range, must be between 0 - 15\n");
            exit(0);
         }
         cIdx++;
      }
      else if( memcmp(argument, "-debug", 6)==0 )
      {
         // -debug
         if( cIdx >= argc )
         {
            cout << "Missing flag after -debug option" << endl;
            exit(0);
         }
         if (memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0)
            commandlineInput.printDebug = true;
         cIdx++;
      }
      else if( memcmp(argument, "-xpt", 5)==0 )
      {
         commandlineInput.useXPT = true;
      }
      else if( memcmp(argument, "-help", 6)==0 || memcmp(argument, "--help", 7)==0 )
      {
         jhMiner_printHelp();
         exit(0);
      }
      else
      {
			cout << "'" << argument << "' is an unknown option." << endl;
			#ifdef _WIN32
				cout << "Type jhPrimeminer.exe -help for more info" << endl;
			#else
				cout << "Type jhPrimeminer -help for more info" << endl; 
			#endif
         exit(-1);
      }
   }
   if( argc <= 1 )
   {
      jhMiner_printHelp();
      exit(0);
   }
}

bool fIncrementPrimorial = true;

//void MultiplierAutoAdjust()
//{
//   //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
//   //printf( "ChainHit:  %f - PrevChainHit: %f - PrimorialMultiplier: %u\n", primeStats.nChainHit, primeStats.nPrevChainHit, primeStats.nPrimorialMultiplier);
//   //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
//
//   //bool fIncrementPrimorial = true;
//   if (primeStats.nChainHit == 0)
//      return;
//
//   if ( primeStats.nChainHit < primeStats.nPrevChainHit)
//      fIncrementPrimorial = !fIncrementPrimorial;
//
//   primeStats.nPrevChainHit = primeStats.nChainHit;
//   primeStats.nChainHit = 0;
//   // Primecoin: dynamic adjustment of primorial multiplier
//   if (fIncrementPrimorial)
//   {
//      if (!PrimeTableGetNextPrime((unsigned int)  primeStats.nPrimorialMultiplier))
//         error("PrimecoinMiner() : primorial increment overflow");
//   }
//   else if (primeStats.nPrimorialMultiplier > 7)
//   {
//      if (!PrimeTableGetPreviousPrime((unsigned int) primeStats.nPrimorialMultiplier))
//         error("PrimecoinMiner() : primorial decrement overflow");
//   }
//}

unsigned int nRoundSievePercentage;
bool bOptimalL1SearchInProgress = false;

#ifdef _WIN32
static void CacheAutoTuningWorkerThread(bool bEnabled)
{
   if (bOptimalL1SearchInProgress || !bEnabled)
      return;

   printf("L1Cache autotunning in progress...\n");

   bOptimalL1SearchInProgress = true;

   DWORD startTime = GetTickCount();	
   unsigned int nL1CacheElementsStart = 64000;
   unsigned int nL1CacheElementsMax   = 2560000;
   unsigned int nL1CacheElementsIncrement = 64000;
   BYTE nSampleSeconds = 20;

   unsigned int nL1CacheElements = primeStats.nL1CacheElements;
   std::map <unsigned int, unsigned int> mL1Stat;
   std::map <unsigned int, unsigned int>::iterator mL1StatIter;
   typedef std::pair <unsigned int, unsigned int> KeyVal;

   primeStats.nL1CacheElements = nL1CacheElementsStart;

   long nCounter = 0;
   while (true && bEnabled && !appQuitSignal)
   {		
      primeStats.nWaveTime = 0;
      primeStats.nWaveRound = 0;
      primeStats.nTestTime = 0;
      primeStats.nTestRound = 0;
      Sleep(nSampleSeconds*1000);
      DWORD waveTime = primeStats.nWaveTime;
      if (bEnabled)
         nCounter ++;
      if (nCounter <=1) 
         continue;// wait a litle at the beginning

      nL1CacheElements = primeStats.nL1CacheElements;
      mL1Stat.insert( KeyVal((unsigned int)primeStats.nL1CacheElements, (unsigned int)primeStats.nWaveRound == 0 ? 0xFFFF : primeStats.nWaveTime / primeStats.nWaveRound));
      if (nL1CacheElements < nL1CacheElementsMax)
         primeStats.nL1CacheElements += nL1CacheElementsIncrement;
      else
      {
         // set the best value
         DWORD minWeveTime = mL1Stat.begin()->second;
         unsigned int nOptimalSize = nL1CacheElementsStart;
         for (  mL1StatIter = mL1Stat.begin(); mL1StatIter != mL1Stat.end(); mL1StatIter++ )
         {
            if (mL1StatIter->second < minWeveTime)
            {
               minWeveTime = mL1StatIter->second;
               nOptimalSize = mL1StatIter->first;
            }
         }
         printf("The optimal L1CacheElement size is: %u\n", nOptimalSize);
         primeStats.nL1CacheElements = nOptimalSize;
         nL1CacheElements = nOptimalSize;
         bOptimalL1SearchInProgress = false;
         break;
      }			
      printf("Auto Tuning in progress: %u %%\n", ((primeStats.nL1CacheElements  - nL1CacheElementsStart)*100) / (nL1CacheElementsMax - nL1CacheElementsStart));

      float ratio = (float)(primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0);
      printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
         primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
         primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);

   }
}
#endif

bool bEnablenPrimorialMultiplierTuning = true;

#ifdef _WIN32
static void RoundSieveAutoTuningWorkerThread(bool bEnabled)
{
   __try
   {


      // Auto Tuning for nPrimorialMultiplier
      int nSampleSeconds = 15;

      while (true)
      {
         if (bOptimalL1SearchInProgress || !bEnablenPrimorialMultiplierTuning || !IsXptClientConnected())
         {
            Sleep(10);
            continue;
         }
         primeStats.nWaveTime = 0;
         primeStats.nWaveRound = 0;
         primeStats.nTestTime = 0;
         primeStats.nTestRound = 0;
         Sleep(nSampleSeconds*1000);

         if (appQuitSignal)
            return;

         if (bOptimalL1SearchInProgress || !bEnablenPrimorialMultiplierTuning || !IsXptClientConnected())
            continue;

         float ratio = (float)(primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0);
         //JLR DBG
         printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
         printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
          primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, primeStats.nL1CacheElements,
          primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);
         printf( "PrimorialMultiplier: %u\n",  primeStats.nPrimorialMultiplier);
         printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
         //JLR DBG END

         if (ratio == 0) continue; // No weaves occurred, don't change anything.

         if (ratio > nRoundSievePercentage + 5)
         {
            if (!PrimeTableGetNextPrime((unsigned int)  primeStats.nPrimorialMultiplier))
               error("PrimecoinMiner() : primorial increment overflow");
            printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
         }
         else if (ratio < nRoundSievePercentage - 5)
         {
            if ( primeStats.nPrimorialMultiplier > 2)
            {
               if (!PrimeTableGetPreviousPrime((unsigned int) primeStats.nPrimorialMultiplier))
                  error("PrimecoinMiner() : primorial decrement overflow");
               printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
            }
         }
      }
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }

}
#endif


void PrintCurrentSettings()
{
	using namespace std;
	uint64_t uptime = (getTimeMilliseconds() - primeStats.startTime);

	uint64_t days = uptime / (24 * 60 * 60 * 1000);
   uptime %= (24 * 60 * 60 * 1000);
   uint64_t hours = uptime / (60 * 60 * 1000);
   uptime %= (60 * 60 * 1000);
   uint64_t minutes = uptime / (60 * 1000);
   uptime %= (60 * 1000);
   uint64_t seconds = uptime / (1000);

	cout << endl << "--------------------------------------------------------------------------------"<< endl;
	cout << "Worker name (-u): " << commandlineInput.workername << endl;
	cout << "Number of mining threads (-t): " << commandlineInput.numThreads << endl;
	cout << "Sieve Size (-s): " << nMaxSieveSize << endl;
	cout << "Sieve Percentage (-d): " << nSievePercentage << endl;
	cout << "Round Sieve Percentage (-r): " << nRoundSievePercentage << endl;
	cout << "Prime Limit (-primes): " << commandlineInput.sievePrimeLimit << endl;
	cout << "Primorial Multiplier (-m): " << primeStats.nPrimorialMultiplier << endl;
	cout << "L1CacheElements (-c): " << primeStats.nL1CacheElements << endl;
	cout << "Chain Length Target (-target): " << nOverrideTargetValue << endl;
	cout << "BiTwin Length Target (-bttarget): " << nOverrideBTTargetValue << endl;
	cout << "Sieve Extensions (-se): " << nSieveExtensions << endl;
	cout << "Total Runtime: " << days << " Days, " << hours << " Hours, " << minutes << " minutes, " << seconds << " seconds" << endl;
	cout << "Total Share Value submitted to the Pool: " << primeStats.fTotalSubmittedShareValue << endl;	
	cout << "--------------------------------------------------------------------------------" << endl;
}



bool appQuitSignal = false;

#ifdef _WIN32
static void input_thread(){
#else
void *input_thread(void *){
static struct termios oldt, newt;
    /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
    tcgetattr( STDIN_FILENO, &oldt);
    /*now the settings will be copied*/
    newt = oldt;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    newt.c_lflag &= ~(ICANON);          

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
#endif



	while (true) {

		int input = getchar();
		switch (input) {
		case 'q': case 'Q': case 3: //case 27:
			appQuitSignal = true;
			Sleep(3200);
			std::exit(0);
#ifdef _WIN32
			return;
#else
			/*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	return 0;
#endif
         break;
      case '[':
         if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
            error("PrimecoinMiner() : primorial decrement overflow");	
         printf("Primorial Multiplier: %u\n", primeStats.nPrimorialMultiplier);
         break;
      case ']':
         if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
            error("PrimecoinMiner() : primorial increment overflow");
         printf("Primorial Multiplier: %u\n", primeStats.nPrimorialMultiplier);
         break;
		case 'h': case 'H':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial decrement overflow");	
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
		case 'y': case 'Y':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial increment overflow");
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
      case 'p': case 'P':
         bEnablenPrimorialMultiplierTuning = !bEnablenPrimorialMultiplierTuning;
         std::cout << "Primorial Multiplier Auto Tuning was " << (bEnablenPrimorialMultiplierTuning ? "Enabled": "Disabled") << std::endl;
         break;
		case 's': case 'S':			
			PrintCurrentSettings();
			break;
		case 'u': case 'U':
			if (!bOptimalL1SearchInProgress && nMaxSieveSize < 10000000)
				nMaxSieveSize += 100000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 'j': case 'J':
			if (!bOptimalL1SearchInProgress && nMaxSieveSize > 100000)
				nMaxSieveSize -= 100000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 't': case 'T':
			if( nRoundSievePercentage < 98)
				nRoundSievePercentage++;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;
		case 'g': case 'G':
			if( nRoundSievePercentage > 2)
				nRoundSievePercentage--;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;

		case 0: case 224:
		{
			input = getchar();	
			switch (input)
			{
				case 72: // key up
				if (!bOptimalL1SearchInProgress && nSievePercentage < 100)
				nSievePercentage ++;
				printf("Sieve Percentage: %u%%\n", nSievePercentage);
				break;

			case 80: // key down
				if (!bOptimalL1SearchInProgress && nSievePercentage > 3)
				nSievePercentage --;
				printf("Sieve Percentage: %u%%\n", nSievePercentage);
				break;

			case 77:    // key right
				if( nRoundSievePercentage < 98)
				nRoundSievePercentage++;
				printf("Round Sieve Percentage: %u%%\n", nRoundSievePercentage);
				break;
			case 75:    // key left
				if( nRoundSievePercentage > 2)
				nRoundSievePercentage--;
				printf("Round Sieve Percentage: %u%%\n", nRoundSievePercentage);
				break;
			}
		}
	}
	}
#ifdef _WIN32
	return;
#else
	/*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
    return 0;
#endif
}

#ifdef _WIN32
typedef std::pair <time_t, HANDLE> thMapKeyVal;
time_t* threadHearthBeat;

static void watchdog_thread(std::map<time_t, HANDLE> threadMap)
#else
static void *watchdog_thread(void *)
#endif
{
#ifdef _WIN32
	uint32_t maxIdelTime = 10 * 1000;
		std::map <time_t, HANDLE> :: const_iterator thMap_Iter;
#endif



   while(true)
   {
      if ((workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH) && (!IsXptClientConnected()))
      {
         // Miner is not connected, wait 5 secs before trying again.
         Sleep(5000);
         {
            Sleep(10);
            continue;
         }

#ifdef _WIN32

         time_t currentTick = getTimeMilliseconds();
         for (size_t i = 0; i < threadMap.size(); i++)
         {
			 time_t heartBeatTick = threadHearthBeat[i];
            if (currentTick - heartBeatTick > maxIdelTime)
            {
               //restart the thread
               printf("Restarting thread %d\n", i);
               //__try
               //{
               //HANDLE h = threadMap.at(i);
               thMap_Iter = threadMap.find(i);
               if (thMap_Iter != threadMap.end())
               {
                  HANDLE h = thMap_Iter->second;
                  TerminateThread( h, 0);
                  Sleep(1000);
                  CloseHandle(h);
                  Sleep(1000);
				  threadHearthBeat[i] = getTimeMilliseconds();
                  threadMap.erase(thMap_Iter);
                  h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)i, 0, 0);
                  SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
                  threadMap.insert(thMapKeyVal(i,h));
               }
               /*}
               __except(EXCEPTION_EXECUTE_HANDLER)
               {
               }*/
            }
         }
#else
	//on linux just exit
	exit(-2000);
#endif
         Sleep( 1*1000);
      }
   }
}

void OnNewBlock(double nBitsShare, double nBits, unsigned long blockHeight)
{
   double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
   double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
   if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
   if( totalRunTime < 1.0 ) totalRunTime = 1.0; // avoid division by zero
   double poolDiff = GetPrimeDifficulty(nBitsShare);
   double blockDiff = GetPrimeDifficulty(nBits);
   float bestChainSinceLaunch = GetChainDifficulty(primeStats.bestPrimeChainDifficultySinceLaunch);
   printf("\n\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
   printf("New Block: %u - Diff: %.06f / %.06f\n", blockHeight, blockDiff, poolDiff);
   printf("Total/Valid shares: [ %d / %d ]  -  Max diff: %.06f\n", total_shares, valid_shares, bestChainSinceLaunch);
   for (int i = 6; i <= std::max(6,(int)bestChainSinceLaunch); i++)
   {
      double sharePerHour = ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0;
      printf("%2dch/h: %8.02f - %u [ %u / %u / %u ]\n", // - Val: %0.06f\n", 
         i, sharePerHour, 
         primeStats.chainCounter[0][i],
         primeStats.chainCounter[1][i],
         primeStats.chainCounter[2][i],
         primeStats.chainCounter[3][i]//, 
      );
   }
   if (!bSoloMining)
      printf("Share Value submitted - Last Block/Total: %0.6f / %0.6f\n", primeStats.fBlockShareValue, primeStats.fTotalSubmittedShareValue);
   printf("Current Primorial Value: %u\n", primeStats.nPrimorialMultiplier);
   printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");

   primeStats.fBlockShareValue = 0;
   multiplierSet.clear();
}

void PrintStat()
{
   if( workData.workEntry[0].dataIsValid )
   {
      double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
      double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
      if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
      if( totalRunTime < 1.0 ) totalRunTime = 1.0; // avoid division by zero
      double primesPerSecond = (double)(primeStats.primeChainsFound / (statsPassedTime / 1000.0));
      float avgCandidatesPerRound = primeStats.nCandidateCount / primeStats.nSieveRounds;
      float sievesPerSecond = primeStats.nSieveRounds / (statsPassedTime / 1000.0);

	  uint32_t bestDifficulty = primeStats.bestPrimeChainDifficulty;
      primeStats.bestPrimeChainDifficulty = 0;
      float primeDifficulty = GetChainDifficulty(bestDifficulty);
      float bestChainSinceLaunch = GetChainDifficulty(primeStats.bestPrimeChainDifficultySinceLaunch);
      float shareValuePerHour = primeStats.fShareValue / totalRunTime * 3600000.0;
	  printf("\nVal/h:%8f - PPS:%d - SPS:%.03f - ACC:%d\n", shareValuePerHour, (int32_t)primesPerSecond, sievesPerSecond, (int32_t)avgCandidatesPerRound);
      printf(" Chain/Hr: ");

      for(int i=6; i<=10; i++)
      {
         printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0);
      }
      if (bestChainSinceLaunch >= 11)
      {
         printf("\n           ");
         for(int i=11; i<=15; i++)
         {
            printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0);
         }
      }
      printf("\n\n");
   }
}

/*
* Mainloop when using getblocktemplate mode
*/
int jhMiner_main_gbtMode()
{
   // main thread, query work every x seconds
	int32_t loopCounter = 0;
   while( true )
   {
      // query new work
      jhMiner_queryWork_primecoin_getblocktemplate();
      // calculate stats every second tick
      if( loopCounter&1 )
      {
         PrintStat();
         primeStats.primeLastUpdate = getTimeMilliseconds();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }		
      // wait and check some stats
	  uint64_t time_updateWork = getTimeMilliseconds();
      while( true )
      {
         if (appQuitSignal)
            return 0;
		 uint64_t passedTime = getTimeMilliseconds() - time_updateWork;
         if( passedTime >= 4000 )
            break;
         int currentBlockCount = getBlockTemplateData.height;
         if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
         {	
            serverData_t* serverData = (serverData_t*)workData.workEntry[0].serverData; 				
            // update serverData
            serverData->nBitsForShare = getBlockTemplateData.nBits;
            serverData->blockHeight = getBlockTemplateData.height;
            OnNewBlock(serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
            lastBlockCount = currentBlockCount;
            break;
         }
         lastBlockCount = currentBlockCount;
         Sleep(200);
      }
      loopCounter++;
   }
   return 0;
}

/*
* Mainloop when using getwork() mode
*/
int jhMiner_main_getworkMode()
{
   // main thread, query work every x seconds
   int32_t loopCounter = 0;
   while( true )
   {
      // query new work
      jhMiner_queryWork_primecoin_getwork();
      // calculate stats every second tick
      if( loopCounter&1 )
      {		
         PrintStat();
         primeStats.primeLastUpdate = getTimeMilliseconds();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }		
      // wait and check some stats
	  uint64_t time_updateWork = getTimeMilliseconds();
      while( true )
      {
         if (appQuitSignal)
            return 0;
		 uint64_t passedTime = getTimeMilliseconds() - time_updateWork;
         if( passedTime >= 4000 )
            break;

         int currentBlockCount = queryLocalPrimecoindBlockCount(useLocalPrimecoindForLongpoll);

         if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
         {	
            serverData_t * serverData = (serverData_t*)workData.workEntry[0].serverData; 				
            OnNewBlock( serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
            lastBlockCount = currentBlockCount;
            break;
         }
         lastBlockCount = currentBlockCount;

         Sleep(200);
      }
      loopCounter++;
   }
   return 0;
}



/*
* Mainloop when using xpt mode
*/
int jhMiner_main_xptMode()
{
   // main thread, don't query work, just wait and process
   int32_t loopCounter = 0;
   uint32_t xptWorkIdentifier = 0xFFFFFFFF;
   //unsigned long lastFiveChainCount = 0;
   //unsigned long lastFourChainCount = 0;
   while( true )
   {
      // calculate stats every ~30 seconds
      if( loopCounter % 10 == 0 )
      {
         PrintStat();
         primeStats.primeLastUpdate = getTimeMilliseconds();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }
      // wait and check some stats
	  uint64_t time_updateWork = getTimeMilliseconds();
      while( true )
      {
         if (appQuitSignal)
            return 0;
		 uint64_t tickCount = getTimeMilliseconds();
		 uint64_t passedTime = tickCount - time_updateWork;

         if( passedTime >= 4000 )
            break;
         xptClient_process(workData.xptClient);
         char* disconnectReason = false;
         if( xptClient_isDisconnected(workData.xptClient, &disconnectReason) )
         {
            // disconnected, mark all data entries as invalid
            for(uint32_t i=0; i<128; i++)
               workData.workEntry[i].dataIsValid = false;

			printf("xpt: Disconnected, auto reconnect in 30 seconds. Reason: %s\n", disconnectReason);

            Sleep(30*1000);

            if( workData.xptClient )
               xptClient_free(workData.xptClient);

            xptWorkIdentifier = 0xFFFFFFFF;
            while( true )
            {
               workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
               if( workData.xptClient )
                  break;
            }
         }
         // has the block data changed?
         if( workData.xptClient && xptWorkIdentifier != workData.xptClient->workDataCounter )
         {
            // printf("New work\n");
            xptWorkIdentifier = workData.xptClient->workDataCounter;
            for(uint32_t i=0; i<workData.xptClient->payloadNum; i++)
            {
               uint8_t blockData[256];
               memset(blockData, 0x00, sizeof(blockData));
               *(uint32_t*)(blockData+0) = workData.xptClient->blockWorkInfo.version;
               memcpy(blockData+4, workData.xptClient->blockWorkInfo.prevBlock, 32);
               memcpy(blockData+36, workData.xptClient->workData[i].merkleRoot, 32);
               *(uint32_t*)(blockData+68) = workData.xptClient->blockWorkInfo.nTime;
               *(uint32_t*)(blockData+72) = workData.xptClient->blockWorkInfo.nBits;
               *(uint32_t*)(blockData+76) = 0; // nonce
               memcpy(workData.workEntry[i].data, blockData, 80);
               ((serverData_t*)workData.workEntry[i].serverData)->blockHeight = workData.xptClient->blockWorkInfo.height;
               ((serverData_t*)workData.workEntry[i].serverData)->nBitsForShare = workData.xptClient->blockWorkInfo.nBitsShare;

               // is the data really valid?
               if( workData.xptClient->blockWorkInfo.nTime > 0 )
                  workData.workEntry[i].dataIsValid = true;
               else
                  workData.workEntry[i].dataIsValid = false;
            }
            if (workData.xptClient->blockWorkInfo.height > 0)
            {
               OnNewBlock( workData.xptClient->blockWorkInfo.nBitsShare, workData.xptClient->blockWorkInfo.nBits, workData.xptClient->blockWorkInfo.height);
            }
         }
         Sleep(10);
      }
      loopCounter++;
   }

   return 0;
}

int main(int argc, char **argv)
{
   // setup some default values
   commandlineInput.port = 10034;
   commandlineInput.numThreads = std::max(getNumThreads(), 1);
   commandlineInput.sieveSize = 1024000; // default maxSieveSize
   commandlineInput.sievePercentage = 10; // default 
   commandlineInput.roundSievePercentage = 70; // default 
   commandlineInput.enableCacheTunning = false;
   commandlineInput.L1CacheElements = 256000;
   commandlineInput.primorialMultiplier = 0; // for default 0 we will swithc aouto tune on
   commandlineInput.targetOverride = 9;
   commandlineInput.targetBTOverride = 10;
   commandlineInput.sieveExtensions = 7;
   commandlineInput.printDebug = 0;
   commandlineInput.sievePrimeLimit = 0;
   // parse command lines
   jhMiner_parseCommandline(argc, argv);
   // Sets max sieve size
   nMaxSieveSize = commandlineInput.sieveSize;
   nSievePercentage = commandlineInput.sievePercentage;
   nRoundSievePercentage = commandlineInput.roundSievePercentage;
   nOverrideTargetValue = commandlineInput.targetOverride;
   nOverrideBTTargetValue = commandlineInput.targetBTOverride;
   nSieveExtensions = commandlineInput.sieveExtensions;
   if (commandlineInput.sievePrimeLimit == 0) //default before parsing 
      commandlineInput.sievePrimeLimit = commandlineInput.sieveSize;  //default is sieveSize 
   primeStats.nL1CacheElements = commandlineInput.L1CacheElements;

   if(commandlineInput.primorialMultiplier == 0)
   {
      primeStats.nPrimorialMultiplier = 37;
      bEnablenPrimorialMultiplierTuning = true;
   }
   else
   {
      primeStats.nPrimorialMultiplier = commandlineInput.primorialMultiplier;
      bEnablenPrimorialMultiplierTuning = false;
   }

   if( commandlineInput.host == NULL )
   {
      printf("Missing -o option\n");
      exit(-1);	
   }
   // if set, validate xpm address
   if( commandlineInput.xpmAddress )
   {
      DecodeBase58(commandlineInput.xpmAddress, decodedWalletAddress, &decodedWalletAddressLen);
      sha256_context ctx;
      uint8_t addressValidationHash[32];
      sha256_starts(&ctx);
      sha256_update(&ctx, (uint8_t*)decodedWalletAddress, 20+1);
      sha256_finish(&ctx, addressValidationHash);
      sha256_starts(&ctx); // is this line needed?
      sha256_update(&ctx, addressValidationHash, 32);
      sha256_finish(&ctx, addressValidationHash);
      if( *(uint32_t*)addressValidationHash != *(uint32_t*)(decodedWalletAddress+21) )
      {
         printf("Address '%s' is not a valid wallet address.\n", decodedWalletAddress);
         exit(-2);
      }
	  useGetBlockTemplate = true;
   }
   // print header
   printf("\n");
   printf("\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n");
   printf("\xBA  jhPrimeMiner - mod by rdebourbon -v3.4beta                   \xBA\n");
   printf("\xBA     merged with hg5fm (mumus) v8.0 build                      \xBA\n");
   printf("\xBA  author: JH (http://ypool.net)                                \xBA\n");
   printf("\xBA  contributors: x3maniac                                       \xBA\n");
   printf("\xBA  Credits: Sunny King for the original Primecoin client&miner  \xBA\n");
   printf("\xBA  Credits: mikaelh for the performance optimizations           \xBA\n");
   printf("\xBA                                                               \xBA\n");
   printf("\xBA  Donations:                                                   \xBA\n");
   printf("\xBA        XPM: AUwKMCYCacE6Jq1rsLcSEHSNiohHVVSiWv                \xBA\n");
   printf("\xBA        LTC: LV7VHT3oGWQzG9EKjvSXd3eokgNXj6ciFE                \xBA\n");
   printf("\xBA        BTC: 1Fph7y622HJ5Cwq4bkzfeZXWep2Jyi5kp7                \xBA\n");
   printf("\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\n");
   printf("Launching miner...\n");
   // set priority lower so the user still can do other things
#ifdef _WIN32
   SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
   // init memory speedup (if not already done in preMain)
   //mallocSpeedupInit();
   if( pctx == NULL )
      pctx = BN_CTX_new();
   // init prime table
   GeneratePrimeTable(commandlineInput.sievePrimeLimit);
   printf("Sieve Percentage: %u %%\n", nSievePercentage);
   // init winsock
#ifdef WIN32
   WSADATA wsa;
   WSAStartup(MAKEWORD(2,2),&wsa);
   // init critical section
   InitializeCriticalSection(&workData.cs);
#else
  pthread_mutex_init(&workData.cs, NULL);
#endif
   // connect to host
#ifndef _WIN32
   hostent* hostInfo = gethostbyname(commandlineInput.host);
   if( hostInfo == NULL )
   {
      printf("Cannot resolve '%s'. Is it a valid URL?\n", commandlineInput.host);
      exit(-1);
   }
   void** ipListPtr = (void**)hostInfo->h_addr_list;
   uint32_t ip = 0xFFFFFFFF;
   if( ipListPtr[0] )
   {
      ip = *(uint32_t*)ipListPtr[0];
   }
   char ipText[32];
   esprintf(ipText, "%d.%d.%d.%d", ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
   if( ((ip>>0)&0xFF) != 255 )
   {
      printf("Connecting to '%s' (%d.%d.%d.%d)\n", commandlineInput.host, ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
   }
#else
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  getaddrinfo(commandlineInput.host, 0, &hints, &res);
  char ipText[32];
  inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipText, INET_ADDRSTRLEN);
#endif
   // setup RPC connection data (todo: Read from command line)
   jsonRequestTarget.ip = ipText;
   jsonRequestTarget.port = commandlineInput.port;
   jsonRequestTarget.authUser = commandlineInput.workername;
   jsonRequestTarget.authPass = commandlineInput.workerpass;
	// init stats
   primeStats.primeLastUpdate = primeStats.blockStartTime = primeStats.startTime = getTimeMilliseconds();
	primeStats.shareFound = false;
	primeStats.shareRejected = false;
	primeStats.primeChainsFound = 0;
	primeStats.foundShareCount = 0;
   for(uint32_t i = 0; i < sizeof(primeStats.chainCounter[0])/sizeof(uint32_t);  i++)
   {
      primeStats.chainCounter[0][i] = 0;
      primeStats.chainCounter[1][i] = 0;
      primeStats.chainCounter[2][i] = 0;
      primeStats.chainCounter[3][i] = 0;
   }
   primeStats.fShareValue = 0;
   primeStats.fBlockShareValue = 0;
   primeStats.fTotalSubmittedShareValue = 0;
   primeStats.nWaveTime = 0;
   primeStats.nWaveRound = 0;
   //primeStats.nL1CacheElements = 256000;

   // setup thread count and print info
   printf("Using %d threads\n", commandlineInput.numThreads);
   printf("Username: %s\n", jsonRequestTarget.authUser);
   printf("Password: %s\n", jsonRequestTarget.authPass);
   // decide protocol
   if( commandlineInput.port == 10034 || commandlineInput.useXPT )
   {
      // port 10034 indicates xpt protocol (in future we will also add a -o URL prefix)
      workData.protocolMode = MINER_PROTOCOL_XPUSHTHROUGH;
      printf("Using x.pushthrough protocol\n");
   }
   else
   {
      if( useGetBlockTemplate )
      {
         workData.protocolMode = MINER_PROTOCOL_GBT;
         // getblocktemplate requires a valid xpm address to be set
         if( commandlineInput.xpmAddress == NULL )
         {
            printf("GetBlockTemplate mode requires -xpm parameter\n");
            exit(-3);
         }
      }
      else
      {
         workData.protocolMode = MINER_PROTOCOL_GETWORK;
         printf("Using GetWork() protocol\n");
         printf("Warning: \n");
         printf("   GetWork() is outdated and inefficient. You are losing mining performance\n");
         printf("   by using it. If the pool supports it, consider switching to x.pushthrough.\n");
         printf("   Just add the port :10034 to the -o parameter.\n");
         printf("   Example: jhPrimeminer.exe -o http://poolurl.net:10034 ...\n");
      }
   }
   // initial query new work / create new connection
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
   {
      jhMiner_queryWork_primecoin_getwork();
   }
   else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
   {
      workData.xptClient = NULL;
      // x.pushthrough initial connect & login sequence
      while( true )
      {
         // repeat connect & login until it is successful (with 30 seconds delay)
         while ( true )
         {
            workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
			if (workData.xptClient != NULL) {
				printf("Connected to pool, attempting authentication...\n");
				break;
			}
            printf("Failed to connect, retry in 30 seconds\n");
            Sleep(1000*30);
         }
         // make sure we are successfully authenticated
         while( xptClient_isDisconnected(workData.xptClient, NULL) == false && xptClient_isAuthenticated(workData.xptClient) == false )
         {
            xptClient_process(workData.xptClient);
            Sleep(1);
         }
         char* disconnectReason = NULL;
         // everything went alright?
         if( xptClient_isDisconnected(workData.xptClient, &disconnectReason) == true )
         {
			printf("Disconnected from pool. Reason: %s\n", disconnectReason);
            xptClient_free(workData.xptClient);
            workData.xptClient = NULL;
            break;
         }
         if( xptClient_isAuthenticated(workData.xptClient) == true )
         {
			 break;
         }
		 if (disconnectReason)
            printf("xpt error: %s\n", disconnectReason);
         // delete client
         xptClient_free(workData.xptClient);
         // try again in 30 seconds
         printf("x.pushthrough authentication sequence failed, retry in 30 seconds\n");
         Sleep(30*1000);
      }
   }

   printf("\nVal/h = 'Share Value per Hour', PPS = 'Primes per Second', \n");
   printf("SPS = 'Sieves per Second', ACC = 'Avg. Candidate Count / Sieve' \n");
   printf("===============================================================\n");
   printf("Keyboard shortcuts:\n");
   printf("   <Ctrl-C>, <Q>     - Quit\n");
   printf("   <Up arrow key>    - Increment Sieve Percentage\n");
   printf("   <Down arrow key>  - Decrement Sieve Percentage\n");
   printf("   <Left arrow key>  - Decrement Round Sieve Percentage\n");
   printf("   <Right arrow key> - Increment Round Sieve Percentage\n");
   printf("   <P> - Enable/Disable Round Sieve Percentage Auto Tuning\n");
   printf("   <S> - Print current settings\n");
   printf("   <[> - Decrement Primorial Multiplier\n");
   printf("   <]> - Increment Primorial Multiplier\n");
   printf("   <-> - Decrement Sive size\n");
   printf("   <+> - Increment Sieve size\n");
   printf("Note: While the initial auto tuning is in progress several values cannot be changed.\n");

#ifdef _WIN32
   // start the Auto Tuning thread
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CacheAutoTuningWorkerThread, (LPVOID)commandlineInput.enableCacheTunning, 0, 0);
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoundSieveAutoTuningWorkerThread, NULL, 0, 0);
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)input_thread, NULL, 0, 0);

   // start threads
   // Although we create all the required heartbeat structures, there is no heartbeat watchdog in GetWork mode. 
   std::map<DWORD, HANDLE> threadMap;
   threadHearthBeat = (time_t *)malloc(commandlineInput.numThreads * sizeof(time_t));
   // start threads
   for(int32_t threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
   {
      HANDLE hThread;
      if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_getwork, (LPVOID)threadIdx, 0, 0);
      else if( workData.protocolMode == MINER_PROTOCOL_GBT )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_gbt, (LPVOID)threadIdx, 0, 0);
      else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)threadIdx, 0, 0);

      SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
      threadMap.insert(thMapKeyVal(threadIdx,hThread));
	  threadHearthBeat[threadIdx] = getTimeMilliseconds();
   }

#else
	uint32_t totalThreads = commandlineInput.numThreads;
			totalThreads = commandlineInput.numThreads + 1;

  pthread_t threads[totalThreads];
  // start the Auto Tuning thread
  
  pthread_create(&threads[commandlineInput.numThreads], NULL, input_thread, NULL);
	pthread_attr_t threadAttr;
  pthread_attr_init(&threadAttr);
  // Set the stack size of the thread
  pthread_attr_setstacksize(&threadAttr, 120*1024);
  // free resources of thread upon return
  pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
  
  // start threads
	for(uint32_t threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
  {
      if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
	  pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_getwork, (void *)threadIdx);
      else if( workData.protocolMode == MINER_PROTOCOL_GBT )
         pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_gbt, (void *)threadIdx);
       else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
          pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_xpt, (void *)threadIdx);
  }
  pthread_attr_destroy(&threadAttr);
#endif


   // enter different mainloops depending on protocol mode
   if( workData.protocolMode == MINER_PROTOCOL_GBT )
      return jhMiner_main_gbtMode();
   else if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
      return jhMiner_main_getworkMode();
   else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
      return jhMiner_main_xptMode();

   return 0;
}
