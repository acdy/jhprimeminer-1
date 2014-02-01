
typedef struct  
{
	struct  
	{
		uint8_t hash[32];
		uint32_t index;
	}previous_output;
	uint32_t scriptSignatureLength;
	uint8_t* scriptSignatureData;
	uint32_t sequence;
	uint32_t coinbaseMinerExtraNonceSign; // used only in combination with coinbase transaction (points to offset where the miner is allowed to insert his custom exta nonce)
}bitclientVIn_t;

typedef struct  
{
	uint64_t coinValue;
	uint32_t scriptPkLength;
	uint8_t* scriptPkData;
}bitclientVOut_t;

typedef struct  
{
	uint32_t version;
	//uint32 tx_in_count; // 
	simpleList_t* tx_in;
	simpleList_t* tx_out;
	uint32_t lock_time;
}bitclientTransaction_t;

bitclientTransaction_t* bitclient_createTransaction();
bitclientTransaction_t* bitclient_createCoinbaseTransactionFromSeed(uint32_t seed, uint32_t seed_userId, uint32_t blockHeight, uint8_t* pubKeyRipeHash, uint64_t inputValue);
void bitclient_destroyTransaction(bitclientTransaction_t* tx);
void bitclient_writeTransactionToStream(stream_t* dataStream, bitclientTransaction_t* tx);
void bitclient_generateTxHash(bitclientTransaction_t* tx, uint8_t* txHash);
void bitclient_calculateMerkleRoot(uint8_t* txHashes, uint32_t numberOfTxHashes, uint8_t merkleRoot[32]);
// misc
void bitclient_addVarIntFromStream(stream_t* msgStream, uint64_t varInt);