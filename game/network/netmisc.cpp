/* $Id: netmisc.c,v 1.9 2003/10/04 19:13:32 btb Exp $ */
/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <stdio.h>
#include <string.h>

#include "descent.h"
#include "pstypes.h"
#include "mono.h"
#include "byteswap.h"

#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__)

#include "segment.h"
#include "network.h"
#include "network_lib.h"
#include "wall.h"

#include "ipx.h"
#include "multi.h"
#include "object.h"
#include "powerup.h"
#include "netmisc.h"
#include "error.h"

#endif

//------------------------------------------------------------------------------
// routine to calculate the checksum of the segments.  We add these specialized routines
// since the current way is byte order dependent.

uint8_t *CalcCheckSum(uint8_t *bufP, int32_t len, uint32_t &sum1, uint32_t &sum2) {
    while (len--) {
        sum1 += *bufP++;
        if (sum1 >= 255)
            sum1 -= 255;
        sum2 += sum1;
    }
    return bufP;
}

//------------------------------------------------------------------------------

#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__)

static uint8_t nmDataBuf[MAX_PACKET_SIZE]; // used for tmp netGame packets as well as sending CObject data
static uint8_t *nmBufP = NULL;

// if using the following macros in loops, the loop's body *must* be enclosed in curly braces,
// or the macros won't work as intended, as the buffer pointer nmBufI will only be incremented
// after the loop has been fully executed!

#define BE_SET_INT(_src) \
    *(reinterpret_cast<int32_t *>(nmBufP + nmBufI)) = INTEL_INT((int32_t)(_src)); \
    nmBufI += 4
#define BE_SET_SHORT(_src) \
    *(reinterpret_cast<short *>(nmBufP + nmBufI)) = INTEL_SHORT((short)(_src)); \
    nmBufI += 2
#define BE_SET_BYTE(_src) nmBufP[nmBufI++] = (uint8_t)(_src)
#define BE_SET_BYTES(_src, _srcSize) \
    memcpy(nmBufP + nmBufI, _src, _srcSize); \
    nmBufI += (_srcSize)

#define BE_GET_INT(_dest) \
    (_dest) = INTEL_INT(*(reinterpret_cast<int32_t *>(nmBufP + nmBufI))); \
    nmBufI += 4
#define BE_GET_SHORT(_dest) \
    (_dest) = INTEL_SHORT(*(reinterpret_cast<short *>(nmBufP + nmBufI))); \
    nmBufI += 2
#define BE_GET_BYTE(_dest) (_dest) = nmBufP[nmBufI++]
#define BE_GET_BYTES(_dest, _destSize) \
    memcpy(_dest, nmBufP + nmBufI, _destSize); \
    nmBufI += (_destSize)

inline int32_t BEGetInt(int32_t &nmBufI) {
    int32_t i;
    BE_GET_INT(i);
    return i;
}

inline short BEGetShort(int32_t &nmBufI) {
    short i;
    BE_GET_SHORT(i);
    return i;
}

inline uint8_t BEGetByte(int32_t &nmBufI) { return nmBufP[nmBufI++]; }

//------------------------------------------------------------------------------
// following are routine for big endian hardware that will swap the elements of
// structures send through the networking code.  The structures and
// this code must be kept in total sync

void BEReceiveNetPlayerInfo(uint8_t *data, tNetPlayerInfo *info) {
    int32_t nmBufI = 0;

    nmBufP = data;
    BE_GET_BYTES(info->callsign, CALLSIGN_LEN + 1);
    BE_GET_BYTES(info->network.m_info.ipx.server, 4);
    BE_GET_BYTES(&info->network.m_info.ipx.node, 6);
    BE_GET_BYTE(info->versionMajor);
    BE_GET_BYTE(info->versionMinor);
    BE_GET_BYTE(info->computerType);
    BE_GET_BYTE(info->connected);
    // BE_GET_SHORT (info->socket);
    info->socket = *(reinterpret_cast<short *>(data + nmBufI)); // don't swap!
    nmBufI += 2;
    BE_GET_BYTE(info->rank);
}

//------------------------------------------------------------------------------

void BESendNetPlayersPacket(uint8_t *server, uint8_t *node) {
    int32_t i;
    int32_t nmBufI = 0;

    nmBufP = nmDataBuf;
#if DBG
    memset(nmBufP, 0, sizeof(nmDataBuf)); // this takes time and shouldn't be necessary
#endif
    BE_SET_BYTE(netPlayers[0].m_info.nType);
    BE_SET_INT(netPlayers[0].m_info.nSecurity);
    for (i = 0; i < MAX_NUM_NET_PLAYERS + 4; i++) {
        BE_SET_BYTES(netPlayers[0].m_info.players[i].callsign, CALLSIGN_LEN + 1);
        BE_SET_BYTES(netPlayers[0].m_info.players[i].network.m_info.ipx.server, 4);
        BE_SET_BYTES(&netPlayers[0].m_info.players[i].network.m_info.ipx.node, 6);
        BE_SET_BYTE(netPlayers[0].m_info.players[i].versionMajor);
        BE_SET_BYTE(netPlayers[0].m_info.players[i].versionMinor);
        BE_SET_BYTE(netPlayers[0].m_info.players[i].computerType);
        BE_SET_BYTE(netPlayers[0].m_info.players[i].connected);
        BE_SET_SHORT(netPlayers[0].m_info.players[i].socket);
        BE_SET_BYTE(netPlayers[0].m_info.players[i].rank);
    }
    if (!server && !node)
        IPXSendBroadcastData(nmBufP, nmBufI);
    else
        IPXSendInternetPacketData(nmBufP, nmBufI, server, node);
}

//------------------------------------------------------------------------------

void BEReceiveNetPlayersPacket(uint8_t *data, CAllNetPlayersInfo *pinfo) {
    int32_t i, nmBufI = 0;

    nmBufP = data;
    BE_GET_BYTE(pinfo->m_info.nType);
    BE_GET_INT(pinfo->m_info.nSecurity);
    for (i = 0; i < MAX_NUM_NET_PLAYERS + 4; i++)
        BEReceiveNetPlayerInfo(nmBufP, pinfo->m_info.players + i);
}

//------------------------------------------------------------------------------

void BESendSequencePacket(tSequencePacket seq, uint8_t *server, uint8_t *node, uint8_t *netAddress) {
    int32_t nmBufI = 0;

    nmBufP = nmDataBuf;
#if DBG
    memset(nmBufP, 0, sizeof(nmDataBuf)); // this takes time and shouldn't be necessary
#endif
    BE_SET_BYTE(seq.nType);
    BE_SET_INT(seq.nSecurity);
    BE_SET_BYTES(seq.player.callsign, CALLSIGN_LEN + 1);
    BE_SET_BYTES(seq.player.network.m_info.ipx.server, 4);
    BE_SET_BYTES(&seq.player.network.m_info.ipx.node, 6);
    BE_SET_BYTE(seq.player.versionMajor);
    BE_SET_BYTE(seq.player.versionMinor);
    BE_SET_BYTE(seq.player.computerType);
    BE_SET_BYTE(seq.player.connected);
    BE_SET_SHORT(seq.player.socket);
    BE_SET_BYTE(seq.player.rank);
    if (netAddress)
        IPXSendPacketData(nmBufP, nmBufI, server, node, netAddress);
    else if (!server && !node)
        IPXSendBroadcastData(nmBufP, nmBufI);
    else
        IPXSendInternetPacketData(nmBufP, nmBufI, server, node);
}

//------------------------------------------------------------------------------

void BEReceiveSequencePacket(uint8_t *data, tSequencePacket *seq) {
    int32_t nmBufI = 0;

    nmBufP = data;
    BE_GET_BYTE(seq->nType);
    BE_GET_INT(seq->nSecurity);
    BEReceiveNetPlayerInfo(data + nmBufI + 3, &(seq->player)); // +3 for pad bytes
}

//------------------------------------------------------------------------------

void BESendNetGamePacket(uint8_t *server, uint8_t *node, uint8_t *netAddress, int32_t bLiteData) {
    int32_t i;
    short h;
    int32_t nmBufI = 0;

    nmBufP = nmDataBuf;
#if DBG
    memset(nmBufP, 0, sizeof(nmDataBuf)); // this takes time and shouldn't be necessary
#endif
    BE_SET_BYTE(netGame.m_info.nType);
    BE_SET_INT(netGame.m_info.nSecurity);
    BE_SET_BYTES(netGame.m_info.szGameName, NETGAME_NAME_LEN + 1);
    BE_SET_BYTES(netGame.m_info.szMissionTitle, MISSION_NAME_LEN + 1);
    BE_SET_BYTES(netGame.m_info.szMissionName, 9);
    BE_SET_INT(netGame.m_info.nLevel);
    BE_SET_BYTE(netGame.m_info.gameMode);
    BE_SET_BYTE(netGame.m_info.bRefusePlayers);
    BE_SET_BYTE(netGame.m_info.difficulty);
    BE_SET_BYTE(netGame.m_info.gameStatus);
    BE_SET_BYTE(netGame.m_info.nNumPlayers);
    BE_SET_BYTE(netGame.m_info.nMaxPlayers);
    BE_SET_BYTE(netGame.m_info.nConnected);
    BE_SET_BYTE(netGame.m_info.gameFlags);
    BE_SET_BYTE(netGame.m_info.protocolVersion);
    BE_SET_BYTE(netGame.m_info.versionMajor);
    BE_SET_BYTE(netGame.m_info.versionMinor);
    BE_SET_BYTE(netGame.m_info.teamVector);
    if (bLiteData)
        goto do_send;
    h = *(reinterpret_cast<uint16_t *>(&netGame.m_info.teamVector + 1));
    BE_SET_SHORT(h); // get the values for the first short bitfield
    h = *(reinterpret_cast<uint16_t *>(&netGame.m_info.teamVector + 3));
    BE_SET_SHORT(h); // get the values for the first short bitfield
    BE_SET_BYTES(netGame.m_info.szTeamName, 2 * (CALLSIGN_LEN + 1));
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_SET_INT(netGame.Locations(i));
    }
    int32_t j;
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        for (j = 0; j < MAX_NUM_NET_PLAYERS; j++) {
            BE_SET_SHORT(*netGame.Kills(i, j));
        }
    }
    BE_SET_SHORT(netGame.GetSegmentCheckSum());
    BE_SET_SHORT(*netGame.TeamKills(0));
    BE_SET_SHORT(*netGame.TeamKills(1));
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_SET_SHORT(*netGame.Killed(i));
    }
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_SET_SHORT(*netGame.PlayerKills(i));
    }
    BE_SET_INT(netGame.GetScoreGoal());
    BE_SET_INT(netGame.GetPlayTimeAllowed());
    BE_SET_INT(netGame.GetLevelTime());
    BE_SET_INT(netGame.GetControlInvulTime());
    BE_SET_INT(netGame.GetMonitorVector());
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_SET_INT(*netGame.PlayerScore(i));
    }
    BE_SET_BYTES(netGame.PlayerFlags(), MAX_NUM_NET_PLAYERS);
    BE_SET_SHORT(PacketsPerSec());
    BE_SET_BYTE(netGame.GetShortPackets());

do_send:

    if (netAddress)
        IPXSendPacketData(nmBufP, nmBufI, server, node, netAddress);
    else if (!server && !node)
        IPXSendBroadcastData(nmBufP, nmBufI);
    else
        IPXSendInternetPacketData(nmBufP, nmBufI, server, node);
}

//------------------------------------------------------------------------------

void BEReceiveNetGamePacket(uint8_t *data, CNetGameInfo *netGame, int32_t bLiteData) {
    int32_t i;
    short *ps;
    int32_t nmBufI = 0;

    nmBufP = data;
    BE_GET_BYTE(netGame->m_info.nType);
    BE_GET_INT(netGame->m_info.nSecurity);
    BE_GET_BYTES(netGame->m_info.szGameName, NETGAME_NAME_LEN + 1);
    BE_GET_BYTES(netGame->m_info.szMissionTitle, MISSION_NAME_LEN + 1);
    BE_GET_BYTES(netGame->m_info.szMissionName, 9);
    BE_GET_INT(netGame->m_info.nLevel);
    BE_GET_BYTE(netGame->m_info.gameMode);
    BE_GET_BYTE(netGame->m_info.bRefusePlayers);
    BE_GET_BYTE(netGame->m_info.difficulty);
    BE_GET_BYTE(netGame->m_info.gameStatus);
    BE_GET_BYTE(netGame->m_info.nNumPlayers);
    BE_GET_BYTE(netGame->m_info.nMaxPlayers);
    BE_GET_BYTE(netGame->m_info.nConnected);
    BE_GET_BYTE(netGame->m_info.gameFlags);
    BE_GET_BYTE(netGame->m_info.protocolVersion);
    BE_GET_BYTE(netGame->m_info.versionMajor);
    BE_GET_BYTE(netGame->m_info.versionMinor);
    BE_GET_BYTE(netGame->m_info.teamVector);
    if (bLiteData)
        return;
    BE_GET_SHORT(*reinterpret_cast<short *>((reinterpret_cast<uint8_t *>(&netGame->m_info.teamVector) + 1)));
    BE_GET_SHORT(*reinterpret_cast<short *>((reinterpret_cast<uint8_t *>(&netGame->m_info.teamVector) + 3)));
    BE_GET_BYTES(netGame->m_info.szTeamName, CALLSIGN_LEN + 1);
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_GET_INT(*netGame->Locations(i));
    }
    for (i = MAX_NUM_NET_PLAYERS * MAX_NUM_NET_PLAYERS, ps = netGame->Kills(); i; i--, ps++) {
        BE_GET_SHORT(*ps);
    }
    netGame->SetSegmentCheckSum(BEGetShort(nmBufI));
    BE_GET_SHORT(*netGame->TeamKills(0));
    BE_GET_SHORT(*netGame->TeamKills(1));
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_GET_SHORT(*netGame->Killed(i));
    }
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_GET_SHORT(*netGame->PlayerKills(i));
    }
    netGame->SetScoreGoal(BEGetInt(nmBufI));
    netGame->SetPlayTimeAllowed(BEGetInt(nmBufI));
    netGame->SetLevelTime(BEGetInt(nmBufI));
    netGame->SetControlInvulTime(BEGetInt(nmBufI));
    netGame->SetMonitorVector(BEGetInt(nmBufI));
    for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) {
        BE_GET_INT(*netGame->PlayerScore(i));
    }
    BE_GET_BYTES(netGame->PlayerFlags(), MAX_NUM_NET_PLAYERS);
    netGame->SetPacketsPerSec(BEGetShort(nmBufI));
    netGame->SetShortPackets(BEGetByte(nmBufI));
}

//------------------------------------------------------------------------------

#define EGI_INTEL_SHORT_2BUF(_m) \
    *(reinterpret_cast<short *>(nmBufP) + \
      (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))) = \
        INTEL_SHORT(extraGameInfo[1]._m);

#define EGI_INTEL_INT_2BUF(_m) \
    *(reinterpret_cast<int32_t *>(nmBufP) + \
      (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))) = \
        INTEL_INT(extraGameInfo[1]._m);

#define EGI_INTEL_UINT_2BUF(_m) \
    *(reinterpret_cast<uint32_t *>(nmBufP) + \
      (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))) = \
        INTEL_INT(extraGameInfo[1]._m);

#define BUF2_EGI_INTEL_SHORT(_m) \
    extraGameInfo[1]._m = INTEL_SHORT(*reinterpret_cast<short *>( \
        nmBufP + (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))));

#define BUF2_EGI_INTEL_INT(_m) \
    extraGameInfo[1]._m = INTEL_INT(*reinterpret_cast<int32_t *>( \
        nmBufP + (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))));

#define BUF2_EGI_INTEL_UINT(_m) \
    extraGameInfo[1]._m = INTEL_INT(*reinterpret_cast<uint32_t *>( \
        nmBufP + (reinterpret_cast<char *>(&extraGameInfo[1]._m) - reinterpret_cast<char *>(&extraGameInfo[1]))));

//------------------------------------------------------------------------------

void BESendExtraGameInfo(uint8_t *server, uint8_t *node, uint8_t *netAddress) {
    nmBufP = nmDataBuf;
    memcpy(nmBufP, &extraGameInfo[1], sizeof(extraGameInfo[0]));
    EGI_INTEL_SHORT_2BUF(entropy.nMaxVirusCapacity);
    EGI_INTEL_SHORT_2BUF(entropy.nEnergyFillRate);
    EGI_INTEL_SHORT_2BUF(entropy.nShieldFillRate);
    EGI_INTEL_SHORT_2BUF(entropy.nShieldDamageRate);
    for (int32_t i = 0; i < MAX_MONSTERBALL_FORCES; i++)
        EGI_INTEL_SHORT_2BUF(monsterball.forces[i].nForce);
    EGI_INTEL_UINT_2BUF(loadout.nGuns);
    EGI_INTEL_UINT_2BUF(loadout.nDevice);
    EGI_INTEL_INT_2BUF(nVersion);
    EGI_INTEL_INT_2BUF(nSpawnDelay);
    EGI_INTEL_INT_2BUF(nLightRange);
    EGI_INTEL_INT_2BUF(nSpeedScale);
    EGI_INTEL_INT_2BUF(nSecurity);
    for (int32_t i = 0; i < MAX_SHIP_TYPES; i++)
        EGI_INTEL_INT_2BUF(shipsAllowed[i]);
    if (netAddress)
        IPXSendPacketData(nmBufP, sizeof(extraGameInfo[0]), server, node, netAddress);
    else if (!server && !node)
        IPXSendBroadcastData(nmBufP, sizeof(extraGameInfo[0]));
    else
        IPXSendInternetPacketData(nmBufP, sizeof(extraGameInfo[0]), server, node);
}

//------------------------------------------------------------------------------

void BEReceiveExtraGameInfo(uint8_t *data, tExtraGameInfo *extraGameInfo) {
    nmBufP = data;
    memcpy(&extraGameInfo[1], nmBufP, sizeof(extraGameInfo[0]));
    BUF2_EGI_INTEL_SHORT(entropy.nMaxVirusCapacity);
    BUF2_EGI_INTEL_SHORT(entropy.nEnergyFillRate);
    BUF2_EGI_INTEL_SHORT(entropy.nShieldFillRate);
    BUF2_EGI_INTEL_SHORT(entropy.nShieldDamageRate);
    for (int32_t i = 0; i < MAX_MONSTERBALL_FORCES; i++)
        BUF2_EGI_INTEL_SHORT(monsterball.forces[i].nForce);
    BUF2_EGI_INTEL_UINT(loadout.nGuns);
    BUF2_EGI_INTEL_UINT(loadout.nDevice);
    BUF2_EGI_INTEL_INT(nVersion);
    BUF2_EGI_INTEL_INT(nSpawnDelay);
    BUF2_EGI_INTEL_INT(nLightRange);
    BUF2_EGI_INTEL_INT(nSpeedScale);
    BUF2_EGI_INTEL_INT(nSecurity);
    for (int32_t i = 0; i < MAX_SHIP_TYPES; i++)
        BUF2_EGI_INTEL_INT(shipsAllowed[i]);
}

//------------------------------------------------------------------------------

void BESendMissingObjFrames(uint8_t *server, uint8_t *node, uint8_t *netAddress) {
    int32_t i;

    memcpy(nmDataBuf, &networkData.sync[0].objs.missingFrames, sizeof(networkData.sync[0].objs.missingFrames));
    reinterpret_cast<tMissingObjFrames *>(&nmDataBuf[0])->nFrame =
        INTEL_SHORT(networkData.sync[0].objs.missingFrames.nFrame);
    i = 2 * sizeof(uint8_t) + sizeof(uint16_t);
    if (netAddress != NULL)
        IPXSendPacketData(nmDataBuf, i, server, node, netAddress);
    else if ((server == NULL) && (node == NULL))
        IPXSendBroadcastData(nmDataBuf, i);
    else
        IPXSendInternetPacketData(nmDataBuf, i, server, node);
}

//------------------------------------------------------------------------------

void BEReceiveMissingObjFrames(uint8_t *data, tMissingObjFrames *missingObjFrames) {
    memcpy(missingObjFrames, nmDataBuf, sizeof(tMissingObjFrames));
    missingObjFrames->nFrame = INTEL_SHORT(missingObjFrames->nFrame);
}

//------------------------------------------------------------------------------

void BESwapObject(CObject *objP) {
    // swap the short and int32_t entries for this CObject
    objP->info.nSignature = INTEL_INT(objP->info.nSignature);
    objP->info.nNextInSeg = INTEL_SHORT(objP->info.nNextInSeg);
    objP->info.nPrevInSeg = INTEL_SHORT(objP->info.nPrevInSeg);
    objP->info.nSegment = INTEL_SHORT(objP->info.nSegment);
    INTEL_VECTOR(objP->info.position.vPos);
    INTEL_MATRIX(objP->info.position.mOrient);
    objP->info.xSize = INTEL_INT(objP->info.xSize);
    objP->info.xShield = INTEL_INT(objP->info.xShield);
    INTEL_VECTOR(objP->info.vLastPos);
    objP->SetLife(INTEL_INT(objP->info.xLifeLeft));
    switch (objP->info.movementType) {
    case MT_PHYSICS:
        INTEL_VECTOR(objP->mType.physInfo.velocity);
        INTEL_VECTOR(objP->mType.physInfo.thrust);
        objP->mType.physInfo.mass = INTEL_INT(objP->mType.physInfo.mass);
        objP->mType.physInfo.drag = INTEL_INT(objP->mType.physInfo.drag);
        objP->mType.physInfo.brakes = INTEL_INT(objP->mType.physInfo.brakes);
        INTEL_VECTOR(objP->mType.physInfo.rotVel);
        INTEL_VECTOR(objP->mType.physInfo.rotThrust);
        objP->mType.physInfo.turnRoll = INTEL_INT(objP->mType.physInfo.turnRoll);
        objP->mType.physInfo.flags = INTEL_SHORT(objP->mType.physInfo.flags);
        break;

    case MT_SPINNING:
        INTEL_VECTOR(objP->mType.spinRate);
        break;
    }

    switch (objP->info.controlType) {
    case CT_WEAPON:
        objP->cType.laserInfo.parent.nType = INTEL_SHORT(objP->cType.laserInfo.parent.nType);
        objP->cType.laserInfo.parent.nObject = INTEL_SHORT(objP->cType.laserInfo.parent.nObject);
        objP->cType.laserInfo.parent.nSignature = INTEL_INT(objP->cType.laserInfo.parent.nSignature);
        objP->cType.laserInfo.xCreationTime = INTEL_INT(objP->cType.laserInfo.xCreationTime);
        objP->cType.laserInfo.nLastHitObj = INTEL_SHORT(objP->cType.laserInfo.nLastHitObj);
        if (objP->cType.laserInfo.nLastHitObj < 0)
            objP->cType.laserInfo.nLastHitObj = 0;
        else {
            gameData.objs.nHitObjects[OBJ_IDX(objP) * MAX_HIT_OBJECTS] = objP->cType.laserInfo.nLastHitObj;
            objP->cType.laserInfo.nLastHitObj = 1;
        }
        objP->cType.laserInfo.nHomingTarget = INTEL_SHORT(objP->cType.laserInfo.nHomingTarget);
        objP->cType.laserInfo.xScale = INTEL_INT(objP->cType.laserInfo.xScale);
        break;

    case CT_EXPLOSION:
        objP->cType.explInfo.nSpawnTime = INTEL_INT(objP->cType.explInfo.nSpawnTime);
        objP->cType.explInfo.nDeleteTime = INTEL_INT(objP->cType.explInfo.nDeleteTime);
        objP->cType.explInfo.nDeleteObj = INTEL_SHORT(objP->cType.explInfo.nDeleteObj);
        objP->cType.explInfo.attached.nParent = INTEL_SHORT(objP->cType.explInfo.attached.nParent);
        objP->cType.explInfo.attached.nPrev = INTEL_SHORT(objP->cType.explInfo.attached.nPrev);
        objP->cType.explInfo.attached.nNext = INTEL_SHORT(objP->cType.explInfo.attached.nNext);
        break;

    case CT_AI:
        objP->cType.aiInfo.nHideSegment = INTEL_SHORT(objP->cType.aiInfo.nHideSegment);
        objP->cType.aiInfo.nHideIndex = INTEL_SHORT(objP->cType.aiInfo.nHideIndex);
        objP->cType.aiInfo.nPathLength = INTEL_SHORT(objP->cType.aiInfo.nPathLength);
        objP->cType.aiInfo.nDangerLaser = INTEL_SHORT(objP->cType.aiInfo.nDangerLaser);
        objP->cType.aiInfo.nDangerLaserSig = INTEL_INT(objP->cType.aiInfo.nDangerLaserSig);
        objP->cType.aiInfo.xDyingStartTime = INTEL_INT(objP->cType.aiInfo.xDyingStartTime);
        break;

    case CT_LIGHT:
        objP->cType.lightInfo.intensity = INTEL_INT(objP->cType.lightInfo.intensity);
        break;

    case CT_POWERUP:
        objP->cType.powerupInfo.nCount = INTEL_INT(objP->cType.powerupInfo.nCount);
        objP->cType.powerupInfo.xCreationTime = INTEL_INT(objP->cType.powerupInfo.xCreationTime);
        break;
    }

    switch (objP->info.renderType) {
    case RT_MORPH:
    case RT_POLYOBJ: {
        int32_t i;

        objP->rType.polyObjInfo.nModel = INTEL_INT(objP->rType.polyObjInfo.nModel);

        for (i = 0; i < MAX_SUBMODELS; i++)
            INTEL_ANGVEC(objP->rType.polyObjInfo.animAngles[i]);
        objP->rType.polyObjInfo.nSubObjFlags = INTEL_INT(objP->rType.polyObjInfo.nSubObjFlags);
        objP->rType.polyObjInfo.nTexOverride = INTEL_INT(objP->rType.polyObjInfo.nTexOverride);
        objP->rType.polyObjInfo.nAltTextures = INTEL_INT(objP->rType.polyObjInfo.nAltTextures);
        break;
    }

    case RT_WEAPON_VCLIP:
    case RT_HOSTAGE:
    case RT_POWERUP:
    case RT_FIREBALL:
    case RT_THRUSTER:
        objP->rType.vClipInfo.nClipIndex = INTEL_INT(objP->rType.vClipInfo.nClipIndex);
        objP->rType.vClipInfo.xFrameTime = INTEL_INT(objP->rType.vClipInfo.xFrameTime);
        break;

    case RT_LASER:
        break;
    }
}

#endif /* WORDS_BIGENDIAN */

//------------------------------------------------------------------------------

void CSide::CheckSum(uint32_t &sum1, uint32_t &sum2) {
    int32_t i, h;
    short s;

    CalcCheckSum(reinterpret_cast<uint8_t *>(&m_nType), 1, sum1, sum2);
    CalcCheckSum(reinterpret_cast<uint8_t *>(&m_nFrame), 1, sum1, sum2);
    s = INTEL_SHORT(m_nWall);
    CalcCheckSum(reinterpret_cast<uint8_t *>(&s), 2, sum1, sum2);
    s = INTEL_SHORT(m_nBaseTex);
    CalcCheckSum(reinterpret_cast<uint8_t *>(&s), 2, sum1, sum2);
    s = INTEL_SHORT((m_nOvlOrient << 14) + m_nOvlTex);
    CalcCheckSum(reinterpret_cast<uint8_t *>(&s), 2, sum1, sum2);
    for (i = 0; i < 4; i++) {
        h = INTEL_INT(int32_t(m_uvls[i].u));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
        h = INTEL_INT(int32_t(m_uvls[i].v));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
        h = INTEL_INT(int32_t(m_uvls[i].l));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
    }
    for (i = 0; i < 2; i++) {
        h = INTEL_INT(int32_t(m_normals[i].v.coord.x));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
        h = INTEL_INT(int32_t(m_normals[i].v.coord.y));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
        h = INTEL_INT(int32_t(m_normals[i].v.coord.z));
        CalcCheckSum(reinterpret_cast<uint8_t *>(&h), 4, sum1, sum2);
    }
}

//------------------------------------------------------------------------------

void CSegment::CheckSum(uint32_t &sum1, uint32_t &sum2) {
    int32_t i;
    short j;

    for (i = 0; i < 6; i++) {
        m_sides[i].CheckSum(sum1, sum2);
    }
    for (i = 0; i < SEGMENT_SIDE_COUNT; i++) {
        j = INTEL_SHORT(m_children[i]);
        CalcCheckSum(reinterpret_cast<uint8_t *>(&j), 2, sum1, sum2);
    }
    for (i = 0; i < SEGMENT_VERTEX_COUNT; i++) {
        j = INTEL_SHORT(m_vertices[i]);
        CalcCheckSum(reinterpret_cast<uint8_t *>(&j), 2, sum1, sum2);
    }
    i = INTEL_INT(m_objects);
    CalcCheckSum(reinterpret_cast<uint8_t *>(&i), 4, sum1, sum2);
}

//------------------------------------------------------------------------------

uint16_t CalcSegmentCheckSum(void) {
    uint32_t sum1, sum2;

    sum1 = sum2 = 0;
    for (int32_t i = 0; i < gameData.segData.nSegments; i++)
        SEGMENTS[i].CheckSum(sum1, sum2);
    return sum1 * 256 + sum2 % 255;
}

//------------------------------------------------------------------------------
// eof
