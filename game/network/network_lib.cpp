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

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h> /* for htons & co. */
#endif

#include "descent.h"
#include "strutil.h"
#include "ipx.h"
#include "error.h"
#include "network.h"
#include "network_lib.h"
#include "netmisc.h"
#include "segmath.h"
#include "renderlib.h"
#include "automap.h"

//------------------------------------------------------------------------------

CMineSyncData mineSyncData;

CNetworkData networkData;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

char *iptos(char *pszIP, char *addr) {
    sprintf(pszIP, "%d.%d.%d.%d:%d", addr[0], addr[1], addr[2], addr[3], ntohs(*reinterpret_cast<int16_t *>(addr + 4)));
    return pszIP;
}

//------------------------------------------------------------------------------

void ClipRank(char *rank) {
    // This function ensures no crashes when dealing with D2 1.0
    if (*((uint8_t *)rank) > 9)
        *rank = 0;
}

//------------------------------------------------------------------------------
// Who is the master of this game?

int32_t WhoIsGameHost(void) {
    if (!IsMultiGame)
        return N_LOCALPLAYER;
    for (int32_t i = 0; i < N_PLAYERS; i++) {
        if (PLAYER(i).callsign[0] /*PLAYER (i).connected*/) { // if a player gets kicked, his callsign will be cleared
            if (gameStates.multi.nGameType == IPX_GAME)
                return i;
            if (htons(*NETPLAYER(i).network.Port()) == uint16_t(mpParams.udpPorts[0] + networkData.nPortOffset))
                return i;
        }
    }
    return N_LOCALPLAYER;
}

//------------------------------------------------------------------------------

int32_t IAmGameHost(void) {
    if (gameStates.app.bGameRunning) {
        gameStates.multi.bServer[0] = (WhoIsGameHost() == N_LOCALPLAYER);
        // the following code is obsolete; Only a player with a local port identical to the server port can become new
        // game host
    }
    return gameStates.multi.bServer[0];
}

//------------------------------------------------------------------------------

int32_t NetworkHowManyConnected(void) {
    int32_t n = 0, i;

    for (i = 0; i < N_PLAYERS; i++)
        if (PLAYER(i).connected)
            n++;
    return n;
}

//------------------------------------------------------------------------------

int32_t CmpNetPlayers(char *callsign1, char *callsign2, CNetworkInfo *network1, CNetworkInfo *network2) {
    if ((gameStates.multi.nGameType == IPX_GAME) ||
        ((gameStates.multi.nGameType == UDP_GAME) && extraGameInfo[0].bCheckUDPPort)) {
        if (memcmp(network1, network2, sizeof(CNetworkInfo)))
            return 1;
    } else if (gameStates.multi.nGameType == UDP_GAME) {
        if (memcmp(
                network1,
                network2,
                extraGameInfo[1].bCheckUDPPort ? sizeof(CNetworkInfo)
                                               : sizeof(CNetworkInfo) - 2)) // bCheck the port too
            return 1;
    }
#ifdef MACINTOSH
    else {
        if (network1->appletalk.node != network2->appletalk.node)
            return 1;
        if (network1->appletalk.net == network2->appletalk.net)
            return 1;
    }
#endif
    if (callsign1 && callsign2 && stricmp(callsign1, callsign2))
        return 1;
#if DBG
    HUDMessage(0, "'%s' not recognized", callsign1);
#endif
    return 0;
}

//------------------------------------------------------------------------------

#define LOCAL_NODE \
    ((gameStates.multi.bHaveLocalAddress && (gameStates.multi.nGameType == UDP_GAME)) \
         ? networkData.localAddress.Server() \
         : networkData.pThislayer.player.network.Node())

int32_t CmpLocalPlayer(CNetworkInfo *pNetwork, char *pszNetCallSign, char *pszLocalCallSign) {
    if (stricmp(pszNetCallSign, pszLocalCallSign))
        return 1;

    if (gameStates.multi.nGameType >= IPX_GAME) {
        if ((gameStates.multi.nGameType < UDP_GAME) && (LOCAL_NODE[0] == 127))
            return 0;
        return memcmp(
                   pNetwork->Node(),
                   LOCAL_NODE,
                   ((gameStates.multi.nGameType > IPX_GAME) && extraGameInfo[1].bCheckUDPPort) ? 6 : 4)
                   ? 1
                   : 0;
    }
#ifdef MACINTOSH
    if (pNetwork->appletalk.node != networkData.pThislayer.player.network.AppleTalk().node)
        return 1;
#endif
    return 0;
}

//------------------------------------------------------------------------------

char *NetworkGetPlayerName(int32_t nObject) {
    CObject *pObj = OBJECT(nObject);
    if (!pObj)
        return NULL;
    if (pObj->info.nType != OBJ_PLAYER)
        return NULL;
    if (pObj->info.nId >= MAX_PLAYERS)
        return NULL;
    if (pObj->info.nId >= N_PLAYERS)
        return NULL;
    return PLAYER(pObj->info.nId).callsign;
}

//------------------------------------------------------------------------------

void DeleteActiveNetGame(int32_t i) {
    if (i < --networkData.nActiveGames) {
        int32_t h = networkData.nActiveGames - i;
        memcpy(activeNetGames + i, activeNetGames + i + 1, activeNetGames[0].Size() * h);
        memcpy(activeNetPlayers + i, activeNetPlayers + i + 1, activeNetPlayers[0].Size() * h);
        memcpy(nLastNetGameUpdate + i, nLastNetGameUpdate + i + 1, sizeof(int32_t) * h);
    }
    networkData.bGamesChanged = 1;
}

//------------------------------------------------------------------------------

void DeleteTimedOutNetGames(void) {
    int32_t i, t = SDL_GetTicks();
    int32_t bPlaySound = 0;

    for (i = networkData.nActiveGames; i > 0;)
        if (t - nLastNetGameUpdate[--i] > 10000) {
            DeleteActiveNetGame(i);
            bPlaySound = 1;
        }
    if (bPlaySound)
        audio.PlaySound(SOUND_HUD_MESSAGE);
}

//------------------------------------------------------------------------------

int32_t FindActiveNetGame(char *pszGameName, int32_t nSecurity) {
    int32_t i;

    for (i = 0; i < networkData.nActiveGames; i++) {
        if (!stricmp(activeNetGames[i].m_info.szGameName, pszGameName)
#if SECURITY_CHECK
            && (activeNetGames[i].m_info.nSecurity == nSecurity)
#endif
        )
            break;
    }
    return i;
}

//------------------------------------------------------------------------------

int32_t NetworkObjnumIsPast(int32_t nObject, tNetworkSyncInfo *pSyncInfo) {
    // determine whether or not a given CObject number has already been sent
    // to a re-joining player.
    int32_t nPlayer = pSyncInfo->player[1].player.connected;
    int32_t nObjMode =
        !((gameData.multigame.nObjOwner[nObject] == -1) || (gameData.multigame.nObjOwner[nObject] == nPlayer));

    if (!pSyncInfo->nState)
        return 0; // We're not sending OBJECTS to a new CPlayerData
    if (nObjMode > pSyncInfo->objs.nMode)
        return 0;
    else if (nObjMode < pSyncInfo->objs.nMode)
        return 1;
    else if (nObject < pSyncInfo->objs.nCurrent)
        return 1;
    else
        return 0;
}

//------------------------------------------------------------------------------

void NetworkSetGameMode(int32_t gameMode) {
    gameData.multigame.score.bShowList = 1;

    if (gameMode == NETGAME_ANARCHY)
        ;
    else if (gameMode == NETGAME_TEAM_ANARCHY)
        gameData.appData.SetGameMode(GM_TEAM);
    else if (gameMode == NETGAME_ROBOT_ANARCHY)
        gameData.appData.SetGameMode(GM_MULTI_ROBOTS);
    else if (gameMode == NETGAME_COOPERATIVE)
        gameData.appData.SetGameMode(GM_MULTI_COOP | GM_MULTI_ROBOTS);
    else if (gameMode == NETGAME_CAPTURE_FLAG)
        gameData.appData.SetGameMode(GM_TEAM | GM_CAPTURE);
    else if (HoardEquipped()) {
        if (gameMode == NETGAME_HOARD)
            gameData.appData.SetGameMode(GM_HOARD);
        else if (gameMode == NETGAME_TEAM_HOARD)
            gameData.appData.SetGameMode(GM_HOARD | GM_TEAM);
        else if (gameMode == NETGAME_ENTROPY)
            gameData.appData.SetGameMode(GM_ENTROPY | GM_TEAM);
        else if (gameMode == NETGAME_MONSTERBALL)
            gameData.appData.SetGameMode(GM_MONSTERBALL | GM_TEAM);
    } else
        Int3();
    gameData.appData.SetGameMode(gameData.appData.GameMode() | GM_NETWORK);
    if (IsTeamGame)
        gameData.multigame.score.bShowList = 3;
}

//------------------------------------------------------------------------------
// Check whether all spawn positions are assigned to a team
// Team specific spawn positions will only be used if this applies

int32_t GotTeamSpawnPos(void) {
    int32_t i, j;

    for (i = 0; i < gameData.multiplayer.nPlayerPositions; i++) {
        j = FindSegByPos(*PlayerSpawnPos(i), -1, 1, 0);
        gameData.multiplayer.playerInit[i].nSegType = (j < 0) ? SEGMENT_FUNC_NONE : SEGMENT(j)->m_function;
        switch (gameData.multiplayer.playerInit[i].nSegType) {
        case SEGMENT_FUNC_GOAL_BLUE:
        case SEGMENT_FUNC_TEAM_BLUE:
        case SEGMENT_FUNC_GOAL_RED:
        case SEGMENT_FUNC_TEAM_RED:
            break;
        default:
            PrintLog(
                0,
                "Warning: Not all spawn positions are assigned to a team. Random spawn positions will be used.\n");
            return 0;
        }
    }
    return 1;
}

//------------------------------------------------------------------------------
// Find the proper initial spawn location for CPlayerData i from team t

int32_t TeamSpawnPos(int32_t i) {
    int32_t h, j, t = GetTeam(i);

    // first find out how many players before CPlayerData i are in the same team
    // result stored in h
    for (h = j = 0; j < i; j++)
        if (GetTeam(j) == t)
            h++;
    // assign the spawn location # (h+1) to CPlayerData i
    for (j = 0; j < gameData.multiplayer.nPlayerPositions; j++) {
        switch (gameData.multiplayer.playerInit[j].nSegType) {
        case SEGMENT_FUNC_GOAL_BLUE:
        case SEGMENT_FUNC_TEAM_BLUE:
            if (!t && (--h < 0))
                return j;
            break;
        case SEGMENT_FUNC_GOAL_RED:
        case SEGMENT_FUNC_TEAM_RED:
            if (t && (--h < 0))
                return j;
            break;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------

void NetworkCountPowerupsInMine(void) {
    CObject *pObj;

    gameData.multiplayer.powerupsInMine.Clear(0);
    FORALL_POWERUP_OBJS(pObj)
    AddPowerupInMine(pObj->info.nId);
}

//------------------------------------------------------------------------------

#ifdef NETPROFILING
void OpenSendLog() {
    int32_t i;

    SendLogFile = reinterpret_cast<FILE *>(fopen("sendlog.net", "w"));
    for (i = 0; i < 100; i++)
        TTSent[i] = 0;
}

//------------------------------------------------------------------------------

void OpenReceiveLog() {
    int32_t i;

    ReceiveLogFile = reinterpret_cast<FILE *>(fopen("recvlog.net", "w"));
    for (i = 0; i < 100; i++)
        TTRecv[i] = 0;
}
#endif

//------------------------------------------------------------------------------

void NetworkRequestPlayerNames(int32_t n) {
    NetworkSendAllInfoRequest(PID_GAME_PLAYERS, activeNetGames[n].m_info.nSecurity);
    networkData.nNamesInfoSecurity = activeNetGames[n].m_info.nSecurity;
}

//------------------------------------------------------------------------------

int32_t HoardEquipped(void) {
    static int32_t bHoard = -1;

    if (bHoard == -1)
        bHoard = CFile::Exist("hoard.ham", gameFolders.game.szData[0], 0) ? 1 : 0;
    return bHoard;
}

//------------------------------------------------------------------------------

int8_t SwitchObservedPlayer(void) {
    if (LOCALOBJECT->Type() != OBJ_GHOST)
        return 0;
    int8_t nPlayer = LOCALPLAYER.ObservedPlayer();
    int16_t nTeam = GetTeam(N_LOCALPLAYER);
    for (uint8_t i = 0; i < N_PLAYERS; i++) {
        nPlayer = (nPlayer + 1) % N_PLAYERS;
        if ((nPlayer != N_LOCALPLAYER) && PLAYER(nPlayer).IsConnected() && (nPlayer != LOCALPLAYER.ObservedPlayer()) &&
            (!IsTeamGame || (nTeam) == GetTeam(nPlayer))) {
            LOCALPLAYER.SetObservedPlayer(nPlayer);
            SetChaseCam(1);
            return nPlayer;
        }
    }
    LOCALPLAYER.SetObservedPlayer(-1);
    return -1;
}

//------------------------------------------------------------------------------

void StopObserverMode(void);

void ObserverFrame(void) {
    if (OBSERVING) {
        if ((LOCALPLAYER.ObservedPlayer() == N_LOCALPLAYER) || gameData.reactorData.bDestroyed) {
            StopObserverMode();
            StartLevel(0x7fffffff, 1);
        } else if ((LOCALPLAYER.ObservedPlayer() >= 0) && !PLAYER(LOCALPLAYER.ObservedPlayer()).IsConnected())
            SwitchObservedPlayer();
    }
}

//------------------------------------------------------------------------------
