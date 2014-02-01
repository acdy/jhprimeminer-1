#include"global.h"
#include"ticker.h"
#include <iostream>

/*
 * Called when a packet with the opcode XPT_OPC_S_AUTH_ACK is received
 */
bool xptClient_processPacket_authResponse(xptClient_t* xptClient)
{
	xptPacketbuffer_t* cpb = xptClient->recvBuffer;
	// read data from the packet
	xptPacketbuffer_beginReadPacket(cpb);
	// start parsing
	bool readError = false;
	// read error code field
	uint32_t authErrorCode = xptPacketbuffer_readU32(cpb, &readError);
	if( readError )
		return false;
	// read reject reason / motd
	char rejectReason[512];
	xptPacketbuffer_readString(cpb, rejectReason, 512, &readError);
	rejectReason[511] = '\0';
	if( readError )
		return false;
	if( authErrorCode == 0 )
	{
		xptClient->clientState = XPT_CLIENT_STATE_LOGGED_IN;
			std::cout << "xpt: Logged in" << std::endl;
			if( rejectReason[0] != '\0' )
				std::cout << "Message from server: " << rejectReason << std::endl;
	}
	else
	{
		// error logging in -> disconnect
			std::cout << "xpt: Failed to log in" << std::endl;
		if( rejectReason[0] != '\0' )
			std::cout << "Reason: " << rejectReason << std::endl;
		return false;
	}
	return true;
}

/*
 * Called when a packet with the opcode XPT_OPC_S_WORKDATA1 is received
 * This is the first version of xpt 'getwork'
 */
bool xptClient_processPacket_blockData1(xptClient_t* xptClient)
{
	// parse block data
	bool recvError = false;
	xptPacketbuffer_beginReadPacket(xptClient->recvBuffer);
	xptClient->workDataValid = false;
	// add general block info
	xptClient->blockWorkInfo.version = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// version
	xptClient->blockWorkInfo.height = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// block height
	xptClient->blockWorkInfo.nBits = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// nBits
	xptClient->blockWorkInfo.nBitsShare = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);		// nBitsRecommended / nBitsShare
	xptClient->blockWorkInfo.nTime = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// nTimestamp
	xptPacketbuffer_readData(xptClient->recvBuffer, xptClient->blockWorkInfo.prevBlock, 32, &recvError);	// prevBlockHash
	uint32_t payloadNum = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);							// payload num
	if( recvError )
	{
			std::cout << "xptClient_processPacket_blockData1(): Parse error" << std::endl;
		return false;
	}
	if( xptClient->payloadNum != payloadNum )
	{
			std::cout << "xptClient_processPacket_blockData1(): Invalid payloadNum" << std::endl;
		return false;
	}
	for(uint32_t i=0; i<payloadNum; i++)
	{
		// read merkle root for each work data entry
		xptPacketbuffer_readData(xptClient->recvBuffer, xptClient->workData[i].merkleRoot, 32, &recvError);
	}
	if( recvError )
	{
			std::cout << "xptClient_processPacket_blockData1(): Parse error 2" << std::endl;
		return false;
	}
	xptClient->workDataValid = true;
	xptClient->workDataCounter++;
	return true;
}

/*
 * Called when a packet with the opcode XPT_OPC_S_SHARE_ACK is received
 */
bool xptClient_processPacket_shareAck(xptClient_t* xptClient)
{
	xptPacketbuffer_t* cpb = xptClient->recvBuffer;
	// read data from the packet
	xptPacketbuffer_beginReadPacket(cpb);
	// start parsing
	bool readError = false;
	// read error code field
	uint32_t shareErrorCode = xptPacketbuffer_readU32(cpb, &readError);
	if( readError )
		return false;
	// read reject reason
	char rejectReason[512];
	xptPacketbuffer_readString(cpb, rejectReason, 512, &readError);
	rejectReason[511] = '\0';
	float shareValue = xptPacketbuffer_readFloat(cpb, &readError);
	if( readError )
		return false;
	if( shareErrorCode == 0 )
	{
		total_shares++;
		valid_shares++;
		time_t now = time(0);
		char* dt = ctime(&now);
			std::cout << "ACCEPTED [ " << valid_shares << " / " << total_shares << " val: " << shareValue << "] " << dt << std::endl;
		primeStats.fShareValue += shareValue;
		primeStats.fBlockShareValue += shareValue;
		primeStats.fTotalSubmittedShareValue += shareValue;
	}
	else
	{
		// error logging in -> disconnect
		total_shares++;
			std::cout << "Invalid share" << std::endl;
			if( rejectReason[0] != '\0' )
				std::cout << "Reason: " << rejectReason << std::endl;
	}
	return true;
}

/*
 * Called when a packet with the opcode XPT_OPC_S_PING is received
 */
bool xptClient_processPacket_client2ServerPing(xptClient_t* xptClient)
{
	// parse block data
	bool recvError = false;
	xptPacketbuffer_beginReadPacket(xptClient->recvBuffer);
	xptClient->workDataValid = false;
	// add general block info
	uint32_t version = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// version
	uint32_t tsLow = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);				// lower 32 bits of timestamp
	uint32_t tsHigh = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);				// upper 32 bits of timestamp
	if( recvError )
	{
			std::cout << "xptClient_processPacket_client2ServerPing(): Parse error" << std::endl;
		return false;
	}

	uint64_t pingSentTimestamp = ((uint64_t) tsLow) | (((uint64_t) tsHigh) << 32);
	uint64_t now = getTimeMilliseconds();
	uint64_t roundtrip = now - pingSentTimestamp;

//	std::cout << "Reply from server time=" << roundtrip << "ms" << std::endl;
	return true;
}
