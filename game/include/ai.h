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

#ifndef _AI_H
#define _AI_H

#include <stdio.h>

#include "object.h"
#include "collision_math.h"
#include "robot.h"

#define PLAYER_AWARENESS_INITIAL_TIME (I2X(3))
#define MAX_PATH_LENGTH 30 // Maximum length of path in ai path following.
#define MAX_DEPTH_TO_SEARCH_FOR_PLAYER 10
#define BOSS_GATE_PRODUCER_NUM -1
#define BOSS_ECLIP_NUM 53

#define ROBOT_BRAIN 7
#define ROBOT_BOSS1 17

#define ROBOT_FIRE_AGITATION 94

#define BOSS_D2 21 // Minimum D2 boss value.
#define BOSS_COOL 21
#define BOSS_WATER 22
#define BOSS_FIRE 23
#define BOSS_ICE 24
#define BOSS_ALIEN1 25
#define BOSS_ALIEN2 26

fix MoveTowardsSegmentCenter(CObject *pObj);
fix MoveTowardsPoint(CObject *pObj, CFixVector *vGoal, fix xMinDist);
int32_t GateInRobot(int16_t nObject, uint8_t nType, int16_t nSegment);
void DoAIMovement(CObject *pObj);
void AIMoveToNewSegment(CObject *obj, int16_t newseg, int32_t firstTime);
// void AIFollowPath ( CObject * obj, int16_t newseg, int32_t firstTime );
void AIRecoverFromWallHit(CObject *obj, int32_t nSegment);
void AIMoveOne(CObject *pObj);
void DoAIFrame(CObject *pObj);
void DoD1AIFrame(CObject *pObj);
void InitAIObject(int16_t nObject, int16_t initial_mode, int16_t nHideSegment);
void UpdatePlayerAwareness(CObject *pObj, fix new_awareness);
void CreateAwarenessEvent(
    CObject *pObj,
    int32_t nType); // CObject *pObj can create awareness of player, amount based on "nType"
void DoAIFrameAll(void);
void DoD1AIFrameAll(void);
void InitAISystem(void);
void ResetAIStates(CObject *pObj);
int32_t CreatePathPoints(
    CObject *pObj,
    int32_t start_seg,
    int32_t end_seg,
    tPointSeg *tPointSegs,
    int16_t *num_points,
    int32_t max_depth,
    int32_t randomFlag,
    int32_t safetyFlag,
    int32_t avoid_seg);
void CreateAllPaths(void);
void CreatePathToStation(CObject *pObj, int32_t max_length);
void AIFollowPath(CObject *pObj, int32_t player_visibility, int32_t previousVisibility, CFixVector *vec_to_player);
fix AITurnTowardsVector(CFixVector *vec_to_player, CObject *obj, fix rate);
void AITurnTowardsVelVec(CObject *pObj, fix rate);
void InitAIObjects(void);
void DoAIRobotHit(CObject *robot, int32_t nType);
void DoD1AIRobotHit(CObject *pObj, int32_t type);
void CreateNSegmentPath(CObject *pObj, int32_t nPathLength, int16_t avoid_seg);
void CreateNSegmentPathToDoor(CObject *pObj, int32_t nPathLength, int16_t avoid_seg);
void InitRobotsForLevel(void);
int32_t AIBehaviorToMode(int32_t behavior);
void CreatePathToSegment(CObject *pObj, int16_t goalseg, int32_t max_length, int32_t safetyFlag);
int32_t ReadyToFire(tRobotInfo *robptr, tAILocalInfo *ailp);
int32_t SmoothPath(CObject *pObj, tPointSeg *psegs, int32_t num_points);
void MoveTowardsPlayer(CObject *pObj, CFixVector *vec_to_player);

int32_t AICanFireAtTarget(CObject *pObj, CFixVector *vGun, CFixVector *vPlayer);

void DoBossDyingFrame(CObject *pObj);

// max_length is maximum depth of path to create.
// If -1, use default: MAX_DEPTH_TO_SEARCH_FOR_PLAYER
void CreatePathToTarget(CObject *pObj, int32_t max_length, int32_t safetyFlag);
void AttemptToResumePath(CObject *pObj);

// When a robot and a player collide, some robots attack!
void DoAIRobotHitAttack(CObject *robot, CObject *player, CFixVector *collision_point);
void DoD1AIRobotHitAttack(CObject *robot, CObject *player, CFixVector *collision_point);
void AIOpenDoorsInSegment(CObject *robot);
int32_t AIDoorIsOpenable(CObject *pObj, CSegment *segp, int16_t nSide);
int32_t AICanSeeTarget(CObject *pObj, CFixVector *pos, fix fieldOfView, CFixVector *vec_to_player);
void AIResetAllPaths(void); // Reset all paths.  Call at the start of a level.
int32_t AILocalPlayerControlsRobot(CObject *pObj, int32_t awarenessLevel);

// In escort.c
void DoEscortFrame(CObject *pObj, fix dist_to_player, int32_t player_visibility);
void DoSnipeFrame(CObject *pObj);
void DoThiefFrame(CObject *pObj);

#if DBG
void force_dump_aiObjects_all(char *msg);
#else
#define force_dump_aiObjects_all (msg)
#endif

void StartBossDeathSequence(CObject *pObj);
extern int32_t Boss_been_hit;
extern fix AI_procTime;

// Stuff moved from ai.c by MK on 05/25/95.
#define ANIM_RATE (I2X(1) / 16)
#define DELTA_ANG_SCALE 16

#define OVERALL_AGITATION_MAX 100
#define MAX_AI_CLOAK_INFO 32 // Must be a power of 2!
#define MAX_AI_CLOAK_INFO_D2 8 // Must be a power of 2!

#pragma pack(push, 1)
typedef struct {
    fix lastTime;
    int32_t nLastSeg;
    CFixVector vLastPos;
} tAICloakInfo;
#pragma pack(pop)

#define CHASE_TIME_LENGTH (I2X(8))
#define DEFAULT_ROBOT_SOUND_VOLUME I2X(1)

extern fix xDistToLastTargetPosFiredAt;
extern CFixVector vLastTargetPosFiredAt;

#define MAX_AWARENESS_EVENTS 256
#pragma pack(push, 1)
typedef struct tAwarenessEvent {
    int16_t nSegment; // CSegment the event occurred in
    int16_t nType; // nType of event, defines behavior
    CFixVector pos; // absolute 3 space location of event
} tAwarenessEvent;
#pragma pack(pop)

#define AIS_MAX 8
#define AIE_MAX 5

#define ESCORT_GOAL_UNSPECIFIED -1
#define ESCORT_GOAL_BLUE_KEY 1
#define ESCORT_GOAL_GOLD_KEY 2
#define ESCORT_GOAL_RED_KEY 3
#define ESCORT_GOAL_CONTROLCEN 4
#define ESCORT_GOAL_EXIT 5

// Custom escort goals.
#define ESCORT_GOAL_ENERGY 6
#define ESCORT_GOAL_ENERGYCEN 7
#define ESCORT_GOAL_SHIELD 8
#define ESCORT_GOAL_POWERUP 9
#define ESCORT_GOAL_ROBOT 10
#define ESCORT_GOAL_HOSTAGE 11
#define ESCORT_GOAL_PLAYER_SPEW 12
#define ESCORT_GOAL_SCRAM 13
#define ESCORT_GOAL_EXIT2 14
#define ESCORT_GOAL_BOSS 15
#define ESCORT_GOAL_MARKER1 16
#define ESCORT_GOAL_MARKER2 17
#define ESCORT_GOAL_MARKER3 18
#define ESCORT_GOAL_MARKER4 19
#define ESCORT_GOAL_MARKER5 20
#define ESCORT_GOAL_MARKER6 21
#define ESCORT_GOAL_MARKER7 22
#define ESCORT_GOAL_MARKER8 23
#define ESCORT_GOAL_MARKER9 24

#define MAX_ESCORT_GOALS 25

#define MAX_ESCORT_DISTANCE I2X(80)
#define MIN_ESCORT_DISTANCE I2X(40)

#define PRODUCER_CHECK 1000

extern fix Escort_last_path_created;
extern int32_t Escort_goalObject, Escort_special_goal, Escort_goal_index;

#define GOAL_WIDTH 11

#define SNIPE_RETREAT_TIME I2X(5)
#define SNIPE_ABORT_RETREAT_TIME \
    (SNIPE_RETREAT_TIME / 2) // Can abort a retreat with this amount of time left in retreat
#define SNIPE_ATTACK_TIME I2X(10)
#define SNIPE_WAIT_TIME I2X(5)
#define SNIPE_FIRE_TIME I2X(2)

#define THIEF_PROBABILITY 16384 // 50% chance of stealing an item at each attempt
#define MAX_STOLEN_ITEMS 10 // Maximum number kept track of, will keep stealing, causes stolen weapons to be lost!

extern void CreateBuddyBot(void);

extern void AIMultiSendRobotPos(int16_t nObject, int32_t force);

extern int32_t Flinch_scale;
extern int32_t Attack_scale;
extern int8_t Mike_to_matt_xlate[];

// Amount of time since the current robot was last processed for things such as movement.
// It is not valid to use FrameTime because robots do not get moved every frame.

// -- extern int32_t              Boss_been_hit;
// ------ John: End of variables which must be saved as part of gamesave. -----

extern int32_t ai_evaded;

extern int8_t Super_boss_gate_list[];
#define MAX_GATE_INDEX 25

// These globals are set by a call to FindHitpoint, which is a slow routine,
// so we don't want to call it again (for this CObject) unless we have to.
// extern CFixVector   Hit_pos;
// extern int32_t          HitType, Hit_seg;

#if DBG
// Index into this array with ailp->mode
// Index into this array with aip->behavior
extern char behavior_text[6][9];

// Index into this array with aip->GOAL_STATE or aip->CURRENT_STATE
extern char state_text[8][5];

extern int32_t bDoAIFlag, nBreakOnObject;

extern void mprintf_animation_info(CObject *pObj);

#endif // if DBG

void AIFrameAnimation(CObject *pObj);
void AIDoRandomPatrol(CObject *pObj, tAILocalInfo *pLocalInfo);
int32_t DoSillyAnimation(CObject *pObj);
void ComputeVisAndVec(
    CObject *pObj,
    CFixVector *pos,
    tAILocalInfo *pLocalInfo,
    tRobotInfo *robptr,
    int32_t *flag,
    fix xMaxVisibleDist);
void DoFiringStuff(CObject *obj, int32_t player_visibility, CFixVector *vec_to_player);
int32_t AIMaybeDoActualFiringStuff(CObject *obj, tAIStaticInfo *aip);
void AIDoActualFiringStuff(CObject *obj, tAIStaticInfo *aip, tAILocalInfo *ailp, tRobotInfo *robptr, int32_t gun_num);
void DoSuperBossStuff(CObject *pObj, fix dist_to_player, int32_t player_visibility);
void DoBossStuff(CObject *pObj, int32_t player_visibility);
// -- unused, 08/07/95 -- void ai_turn_randomly (CFixVector *vec_to_player, CObject *obj, fix rate, int32_t
// previousVisibility);
void AIMoveRelativeToTarget(
    CObject *pObj,
    tAILocalInfo *ailp,
    fix dist_to_player,
    CFixVector *vec_to_player,
    fix circleDistance,
    int32_t evade_only,
    int32_t player_visibility);
void MoveAwayFromTarget(CObject *pObj, CFixVector *vec_to_player, int32_t attackType);
void MoveTowardsVector(CObject *pObj, CFixVector *vec_goal, int32_t dot_based);
void InitAIFrame(void);
void MakeNearbyRobotSnipe(void);

void CreateBfsList(int32_t start_seg, int16_t bfs_list[], int32_t *length, int32_t max_segs);
void InitThiefForLevel();

int32_t AISaveBinState(CFile &cf);
int32_t AISaveUniState(CFile &cf);
int32_t AIRestoreBinState(CFile &cf, int32_t version);
int32_t AIRestoreUniState(CFile &cf, int32_t version);

int32_t CheckObjectObjectIntersection(CFixVector *pos, fix size, CSegment *pSeg);
int32_t DoRobotDyingFrame(
    CObject *pObj,
    fix StartTime,
    fix xRollDuration,
    int8_t *bDyingSoundPlaying,
    int16_t deathSound,
    fix xExplScale,
    fix xSoundScale);

void TeleportBoss(CObject *pObj);
int32_t BossFitsInSeg(CObject *pBossObj, int32_t nSegment);

void StartRobotDeathSequence(CObject *pObj);
int32_t DoAnyRobotDyingFrame(CObject *pObj);

#define SPECIAL_REACTOR_ROBOT 65
extern void SpecialReactorStuff(void);

// --------------------------------------------------------------------------------------------------------------------

static inline fix AIMaxDist(int32_t i, tRobotInfo *pRobotInfo) {
    if (gameOpts->gameplay.nAIAwareness) {
        switch (i) {
        case 0:
            return I2X(100 + gameStates.app.nDifficultyLevel * 100);
        case 1:
            return I2X(300);
        case 2:
            return I2X(500);
        case 3:
            return I2X(750);
        case 4:
            return I2X(50 * (2 * gameStates.app.nDifficultyLevel + pRobotInfo->pursuit));
        }
    } else {
        switch (i) {
        case 0:
            return I2X(120 + gameStates.app.nDifficultyLevel * 20);
        case 1:
            return I2X(80);
        case 2:
            return I2X(200);
        case 3:
            return I2X(200);
        case 4:
            return I2X(20 * (2 * gameStates.app.nDifficultyLevel + pRobotInfo->pursuit));
        }
    }
    return 0;
}

#define MAX_WAKEUP_DIST AIMaxDist(0, NULL)
#define MAX_CHASE_DIST AIMaxDist(1, NULL)
#define MAX_SNIPE_DIST AIMaxDist(2, NULL)
#define MAX_REACTION_DIST AIMaxDist(3, NULL)
#define MAX_PURSUIT_DIST(_botInfoP) AIMaxDist(4, _botInfoP)

#define USE_D1_AI (gameStates.app.bD1Mission && gameOpts->gameplay.bUseD1AI)

// --------------------------------------------------------------------------------------------------------------------

#endif /* _AI_H */
