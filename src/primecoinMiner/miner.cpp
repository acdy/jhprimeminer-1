#include"global.h"
#include <ctime>
#include "ticker.h"


bool MineProbablePrimeChain(CSieveOfEratosthenes*& psieve, primecoinBlock_t* block, mpz_class& bnFixedMultiplier, bool& fNewBlock, unsigned int& nTriedMultiplier, unsigned int& nProbableChainLength, 
							unsigned int& , unsigned int& nPrimesHit, int32_t threadIndex, mpz_class& mpzHash, unsigned int nPrimorialMultiplier);

std::set<mpz_class> multiplierSet;

bool BitcoinMiner(primecoinBlock_t* primecoinBlock, CSieveOfEratosthenes*& psieve, const int32_t threadIndex, const unsigned int nonceStep)
{
	//JLR DBG
	//printf("PrimecoinMiner started\n");
	//SetThreadPriority(THREAD_PRIORITY_LOWEST);
	//RenameThread("primecoin-miner");
	if( pctx == NULL )
		pctx = BN_CTX_new();
	// Each thread has its own key and counter
	//CReserveKey reservekey(pwallet);
//	unsigned int nExtraNonce = 0;  unused?

   static const unsigned int MAX_NONCE = 0xFFFF0000; // From Primecoind sources.
	static const unsigned int nPrimorialHashFactor = 7;
//	const unsigned int nPrimorialMultiplierStart = 61;   unused?
//	const unsigned int nPrimorialMultiplierMax = 79;  unused?

	unsigned int nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
//	uint64_t nTimeExpected = 0;   // time expected to prime chain (micro-second)   unused?
//	uint64_t nTimeExpectedPrev = 0; // time expected to prime chain last time   unused?
//	bool fIncrementPrimorial = true; // increase or decrease primorial factor   unused?
//	uint64_t nSieveGenTime = 0;   unused?
	

	// Generate a thread specific nonce.
	primecoinBlock->nonce = threadIndex;

	uint64_t nTime = getTimeMilliseconds() + 1000*600;
//	uint64_t nStatTime = getTimeMilliseconds() + 2000;  unused?
	
	// note: originally a wanted to loop as long as (primecoinBlock->workDataHash != jhMiner_getCurrentWorkHash()) did not happen
	//		 but I noticed it might be smarter to just check if the blockHeight has changed, since that is what is really important
	uint32_t loopCount = 0;

	//mpz_class mpzHashFactor;
	//Primorial(nPrimorialHashFactor, mpzHashFactor);
	unsigned int nHashFactor = PrimorialFast(nPrimorialHashFactor);

	time_t unixTimeStart;
	time(&unixTimeStart);
	uint32_t nTimeRollStart = primecoinBlock->timestamp - 5;
	uint32_t nLastRollTime = (uint32_t)getTimeMilliseconds();
	uint32_t nCurrentTick = nLastRollTime;
	while( nCurrentTick < nTime && primecoinBlock->serverData.blockHeight == jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex) )
			{
		nCurrentTick = (uint32_t)getTimeMilliseconds();
      // Roll Time stamp every 10 secs.
		if ((primecoinBlock->xptMode) && (nCurrentTick < nLastRollTime || (nLastRollTime - nCurrentTick >= 10000)))
		{
			// when using x.pushthrough, roll time
			time_t unixTimeCurrent;
			time(&unixTimeCurrent);
			uint32_t timeDif = (uint32_t)unixTimeCurrent - (uint32_t)unixTimeStart;
			uint32_t newTimestamp = nTimeRollStart + timeDif;
			if( newTimestamp != primecoinBlock->timestamp )
			{
				primecoinBlock->timestamp = newTimestamp;
				primecoinBlock->nonce = threadIndex;
				//nPrimorialMultiplierStart = startFactorList[(threadIndex&3)];
		      //nPrimorialMultiplier = nPrimorialMultiplierStart;
			}
			nLastRollTime = nCurrentTick;
		}

		primecoinBlock_generateHeaderHash(primecoinBlock, primecoinBlock->blockHeaderHash.begin());
		//
		// Search
		//
		bool fNewBlock = true;
		unsigned int nTriedMultiplier = 0;
		// Primecoin: try to find hash divisible by primorial
        uint256 phash = primecoinBlock->blockHeaderHash;
        mpz_class mpzHash;
        mpz_set_uint256(mpzHash.get_mpz_t(), phash);
        
		while ((phash < hashBlockHeaderLimit || !mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor)) && primecoinBlock->nonce < MAX_NONCE) {
			primecoinBlock->nonce += nonceStep;
			primecoinBlock_generateHeaderHash(primecoinBlock, primecoinBlock->blockHeaderHash.begin());
            phash = primecoinBlock->blockHeaderHash;
            mpz_set_uint256(mpzHash.get_mpz_t(), phash);
		}
		//JLR DBG
		//printf("Use nonce %d\n", primecoinBlock->nonce);
		if (primecoinBlock->nonce >= MAX_NONCE)
		{
			//JLR DBG
			printf("Nonce overflow\n");
			break;
		}
		// Primecoin: primorial fixed multiplier
		mpz_class mpzPrimorial;
		unsigned int nRoundTests = 0;
		unsigned int nRoundPrimesHit = 0;
//		uint64_t nPrimeTimerStart = getTimeMilliseconds();   unused?
		
		//if( loopCount > 0 )
		//{
		//	//primecoinBlock->nonce++;
		//	if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
		//		error("PrimecoinMiner() : primorial increment overflow");
		//}

		Primorial(nPrimorialMultiplier, mpzPrimorial);

		unsigned int nTests = 0;
		unsigned int nPrimesHit = 0;
		
		mpz_class mpzMultiplierMin = mpzPrimeMin * nHashFactor / mpzHash + 1;
		while (mpzPrimorial < mpzMultiplierMin )
		{
			if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial minimum overflow");
			Primorial(nPrimorialMultiplier, mpzPrimorial);
		}
        mpz_class mpzFixedMultiplier;
        if (mpzPrimorial > nHashFactor) {
            mpzFixedMultiplier = mpzPrimorial / nHashFactor;
        } else {
            mpzFixedMultiplier = 1;
        }		
	//JLR DBG
	//printf("fixedMultiplier: %d nPrimorialMultiplier: %d\n", BN_get_word(&bnFixedMultiplier), nPrimorialMultiplier);
		// Primecoin: mine for prime chain
		unsigned int nProbableChainLength;
		MineProbablePrimeChain(psieve, primecoinBlock, mpzFixedMultiplier, fNewBlock, nTriedMultiplier, nProbableChainLength, nTests, nPrimesHit, threadIndex, mpzHash, nPrimorialMultiplier);
#ifdef _WIN32
		threadHearthBeat[threadIndex] = getTimeMilliseconds();
#endif
		if (appQuitSignal)
		{
			printf( "Shutting down mining thread %d.\n", threadIndex);
			return false;
		}
		//{
		//	// do nothing here, share is already submitted in MineProbablePrimeChain()
		//	//primecoinBlock->nonce += 0x00010000;
		//	primecoinBlock->nonce++;
		//	nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
		//	//break;
		//}
		//psieve = NULL;
		nRoundTests += nTests;
		nRoundPrimesHit += nPrimesHit;
		nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
		// added this
		//if (fNewBlock)
		//{
		//}


		primecoinBlock->nonce += nonceStep;
		//primecoinBlock->timestamp = max(primecoinBlock->timestamp, (unsigned int) time(NULL));
		loopCount++;
	}
	
	return true;
}
