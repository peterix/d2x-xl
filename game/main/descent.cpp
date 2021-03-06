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

char copyright[] = "DESCENT II  COPYRIGHT (C) 1994-1996 PARALLAX SOFTWARE CORPORATION";

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(__unix__) || defined(__macosx__)
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __macosx__
#include "SDL/SDL_main.h"
#include "SDL/SDL_keyboard.h"
#include "SDL_net/SDL_net.h"
#include "FolderDetector.h"
#else
#ifdef _WIN32
#include <WinSock.h>
#endif
#include "SDL_main.h"
#include "SDL_keyboard.h"
#include "SDL_net.h"
#endif
#include "descent.h"
#include "u_mem.h"
#include "strutil.h"
#include "key.h"
#include "timer.h"
#include "error.h"
#include "segpoint.h"
#include "screens.h"
#include "texmap.h"
#include "texmerge.h"
#include "menu.h"
#include "iff.h"
#include "pcx.h"
#include "args.h"
#include "hogfile.h"
#include "ogl_defs.h"
#include "ogl_lib.h"
#include "ogl_render.h"
#include "ogl_shader.h"
#include "sdlgl.h"
#include "text.h"
#include "newdemo.h"
#include "objrender.h"
#include "renderthreads.h"
#include "network.h"
#include "font.h"
#include "gamefont.h"
#include "kconfig.h"
#include "mouse.h"
#include "joy.h"
#include "input.h"
#include "desc_id.h"
#include "joydefs.h"
#include "gamepal.h"
#include "movie.h"
#include "compbit.h"
#include "playerprofile.h"
#include "tracker.h"
#include "rendermine.h"
#include "sphere.h"
#include "endlevel.h"
#include "interp.h"
#include "autodl.h"
#include "hiresmodels.h"
#include "automap.h"
#include "banlist.h"
#include "menubackground.h"
#include "console.h"
#include "IpToCountry.h"

#include "audio/songs.h"
#include "audio/soundthreads.h"

#ifndef DESCENT_EXECUTABLE_VERSION
#ifdef __macosx__
#define DESCENT_EXECUTABLE_VERSION "OS X"
#else
#define DESCENT_EXECUTABLE_VERSION "Linux"
#endif
#endif

extern const char *pszOglExtensions;
extern int32_t glHWHash;

#include <SDL.h>
#include <SDL_mixer.h>

#include "vers_id.h"

CGameOptions gameOptions[2];
CGameStates gameStates;
CGameData gameData;

// static const char desc_id_checksum_str[] = DESC_ID_CHKSUM_TAG "0000"; // 4-byte checksum

void DefaultAllSettings(bool bSetup);

CGameOptions *gameOpts = gameOptions;

void EvalArgs(void);
void GetAppFolders(bool bInit);
#if defined(_WIN32) || defined(__unix__)
int32_t CheckAndFixSetup(void);
#else
#define CheckAndFixSetup()
#endif
void InitGameStates(void);

char szAutoMission[255];
char szAutoHogFile[255];

int32_t nDescentCriticalError = 0;
uint32_t descent_critical_deverror = 0;
uint32_t descent_critical_errcode = 0;

// ----------------------------------------------------------------------------

extern bool bPrintingLog;

// ----------------------------------------------------------------------------

#define SUBVER_XOFFS (gameStates.menus.bHires ? 45 : 25)

void PrintVersionInfo(void) {
    if (!gameStates.app.bShowVersionInfo) {
        return;
    }

    int32_t nInfoType;

    if (!(gameStates.app.bGameRunning || gameStates.app.bBetweenLevels))
        nInfoType = 0;
    else if (gameStates.app.bSaveScreenShot || (gameData.demoData.nState == ND_STATE_PLAYBACK))
        nInfoType = 1;
    else
        return;

    static int32_t bVertigo = -1;

    int32_t y, w, ws, h, hs, aw;
    float grAlpha = gameStates.render.grAlpha;

    gameData.renderData.frame.Activate("PrintVersionInfo (frame)");

    gameStates.render.grAlpha = 1.0f;
    if (gameStates.menus.bHires) {
        if (gameOpts->menus.altBg.bHave > 0)
            y = 8; // 102
        else {
            y = 88 * CCanvas::Current()->Height() / 480;
            if (y < 88)
                y = 88;
        }
    } else
        y = 37;

    gameStates.menus.bDrawCopyright = 0;
    if (!nInfoType) {
        fontManager.SetCurrent(GAME_FONT);
        fontManager.Current()->StringSize("V2.2", w, h, aw);
        fontManager.SetColorRGBi(RGB_PAL(63, 47, 0), 1, 0, 0);
        h += 2;
        GrPrintF(NULL, 0x8000, CCanvas::Current()->Height() - h, "visit www.descent2.de");
        // fontManager.SetColorRGBi (RGB_PAL (51, 34, 0), 1, 0, 0);
        fontManager.SetColorRGBi(D2BLUE_RGBA, 1, 0, 0);
        GrPrintF(NULL, 0x8000, CCanvas::Current()->Height() - 3 * h - 6, "Press F1 for help in menus");
        fontManager.SetColorRGBi(RGB_PAL(31, 31, 31), 1, 0, 0);
        CCanvas::Current()->SetColor(CCanvas::Current()->FontColor(0));
        OglDrawLine(
            CCanvas::Current()->Width() / 2 - 275,
            CCanvas::Current()->Height() - 2 * h - 5,
            CCanvas::Current()->Width() / 2 + 275,
            CCanvas::Current()->Height() - 2 * h - 5,
            NULL);

        GrPrintF(NULL, 0x8000, CCanvas::Current()->Height() - 2 * h - 2, TXT_COPYRIGHT);
        GrPrintF(
            NULL,
            CCanvas::Current()->Width() - w - 2,
            CCanvas::Current()->Height() - 2 * h - 2,
            "V%d.%d",
            D2X_MAJOR,
            D2X_MINOR);
        if (bVertigo < 0)
            bVertigo = CFile::Exist("d2x.hog", gameFolders.missions.szRoot, 0);
        if (bVertigo) {
            fontManager.SetCurrent(MEDIUM2_FONT);
            fontManager.Current()->StringSize(TXT_VERTIGO, w, h, aw);
            GrPrintF(
                NULL,
                CCanvas::Current()->Width() - w - SUBVER_XOFFS,
                y + (gameOpts->menus.altBg.bHave ? h + 2 : 0),
                TXT_VERTIGO);
        }
        fontManager.SetCurrent(SMALL_FONT);
        fontManager.Current()->StringSize(VERSION, ws, hs, aw);
        fontManager.SetColorRGBi(D2BLUE_RGBA, 1, 0, 0);
        GrPrintF(
            NULL,
            CCanvas::Current()->Width() - ws - 1,
            y + ((bVertigo && !gameOpts->menus.altBg.bHave) ? h + 2 : 0) + (h - hs) / 2,
            VERSION);
    }

    if (!nInfoType) {
        fontManager.SetCurrent(MEDIUM2_FONT);
        fontManager.Current()->StringSize(D2X_NAME, w, h, aw);
        GrPrintF(
            NULL,
            CCanvas::Current()->Width() - w - SUBVER_XOFFS,
            y + ((bVertigo && !gameOpts->menus.altBg.bHave) ? h + 2 : 0),
            D2X_NAME);
    } else {
        // gameStates.render.grAlpha = 0.75f;
        int32_t w2, h2;
        fontManager.SetCurrent(GAME_FONT);
        fontManager.Current()->StringSize("www.descent2.de", w2, h2, aw);
        fontManager.SetCurrent(MEDIUM2_FONT);
        fontManager.Current()->StringSize("D2X-XL", w, h, aw);
        int32_t l = CCanvas::Current()->Width() - 70 - w;
        int32_t t = CCanvas::Current()->Height() - 40 - h - h2 - 2;
        GrPrintF(NULL, l, t, "D2X-XL");
        fontManager.SetCurrent(GAME_FONT);

        fontManager.SetColorRGBi(RGB_PAL(63, 47, 0), 1, 0, 0);

        GrPrintF(NULL, l - (w2 - w + 1) / 2, t + h + 2, "www.descent2.de");
    }
    fontManager.SetCurrent(NORMAL_FONT);
    fontManager.SetColorRGBi(RGB_PAL(6, 6, 6), 1, 0, 0);
    gameStates.render.grAlpha = grAlpha;
    gameData.renderData.frame.Deactivate();
}

// ----------------------------------------------------------------------------

void CheckEndian(void) {
    union {
        int16_t s;
        uint8_t b[2];
    } h;

    h.b[0] = 0;
    h.b[1] = 1;
    gameStates.app.bLittleEndian = (h.s == 256);
}

// ----------------------------------------------------------------------------

void DoJoystickInit() {
    if (!FindArg("-nojoystick")) {
        JoyInit();
        if (FindArg("-joyslow"))
            JoySetSlowReading(JOY_SLOW_READINGS);
        if (FindArg("-joypolled"))
            JoySetSlowReading(JOY_POLLED_READINGS);
        if (FindArg("-joybios"))
            JoySetSlowReading(JOY_BIOS_READINGS);
    }
}

// ----------------------------------------------------------------------------
// set this to force game to run in low res
void DoSelectPlayer(void) {
    LOCALPLAYER.callsign[0] = '\0';
    if (!gameData.demoData.bAuto) {
        KeyFlush();
        // now, before we bring up the register player menu, we need to
        // do some stuff to make sure the palette is ok.  First, we need to
        // get our current palette into the 2d's array, so the remapping will
        // work.  Second, we need to remap the fonts.  Third, we need to fill
        // in part of the fade tables so the darkening of the menu edges works
        SelectPlayer(); // get player's name
    }
}

// ----------------------------------------------------------------------------

#define MENU_HIRES_MODE SM(800, 600)

// ----------------------------------------------------------------------------

void PrintVersion(void) {
    FILE *f;
    char fn[FILENAME_LEN];

    sprintf(fn, "%sd2x-xl-version.txt", gameFolders.game.szData[0]);
    if ((f = fopen(fn, "wa"))) {
        fprintf(f, "%s\n", VERSION);
        fclose(f);
    }
    exit(0);
}

// ----------------------------------------------------------------------------

void PrintBanner(void) {
#if (defined(_WIN32) || defined(__unix__))
    console.printf(CON_NORMAL, "\nDESCENT 2 %s v%d.%d.%d\n", VERSION_TYPE, D2X_MAJOR, D2X_MINOR, D2X_MICRO);
#elif defined(__macosx__)
    console.printf(CON_NORMAL, "\nDESCENT 2 %s -- %s\n", VERSION_TYPE, DESCENT_VERSION);
#endif
    if (hogFileManager.D2XFiles().bInitialized) {
        console.printf((int32_t)CON_NORMAL, "  Vertigo Enhanced\n");
    }
    console.printf(CON_NORMAL, "\nBuilt: %s %s\n", __DATE__, __TIME__);
#ifdef __VERSION__
    console.printf(CON_NORMAL, "Compiler: %s\n", __VERSION__);
#endif
    console.printf(CON_NORMAL, "\n");
}

// ----------------------------------------------------------------------------

int32_t ShowTitleScreens(void) {
    int32_t nPlayed = MOVIE_NOT_PLAYED; // default is not nPlayed
    {
        // NOTE LINK TO ABOVE!
        int32_t bSongPlaying = 0;
        if (movieManager.m_bHaveExtras) {
            nPlayed = movieManager.Play("starta.mve", MOVIE_REQUIRED, 0, gameOpts->movies.bResize);
            if (nPlayed == MOVIE_ABORTED) {
                nPlayed = MOVIE_PLAYED_FULL;
            } else {
                nPlayed = movieManager.Play("startb.mve", MOVIE_REQUIRED, 0, gameOpts->movies.bResize);
            }
        } else {
            movieManager.PlayIntro();
        }

        if (!bSongPlaying) {
            songManager.Play(SONG_TITLE, 1);
        }
    }
    return (nPlayed != MOVIE_NOT_PLAYED); // default is not nPlayed
}

// ----------------------------------------------------------------------------

void ShowLoadingScreen(void) {
    GrSetMode(
        gameStates.menus.bHires
            ? (gameStates.gfx.nStartScrMode < 0) ? SM(800, 600) : displayModeInfo[gameStates.gfx.nStartScrMode].dim
            : SM(320, 200));
    SetScreenMode(SCREEN_MENU);
    gameStates.render.fonts.bHires = gameStates.render.fonts.bHiresAvailable && gameStates.menus.bHires;
    backgroundManager.Rebuild();
    backgroundManager.Draw(gameStates.app.bNostalgia ? BG_LOADING : BG_STANDARD);
    ogl.Update(0);
}

// ----------------------------------------------------------------------------

void LoadHoardData(void) {}

// ----------------------------------------------------------------------------

void GrabMouse(int32_t bGrab, int32_t bForce) {
/*
    SDL_SetRelativeMouseMode(SDL_FALSE);
*/
    auto grabMouse = (bGrab && gameStates.input.bGrabMouse);
    PrintLog(0, "GrabMouse (%d)\n", grabMouse);
    SDL_SetRelativeMouseMode(grabMouse ? SDL_TRUE : SDL_FALSE);
}

// ----------------------------------------------------------------------------

void MainLoop(void) {
    while (gameStates.app.nFunctionMode != FMODE_EXIT) {
        gameStates.app.bGameRunning = (gameStates.app.nFunctionMode == FMODE_GAME);
        switch (gameStates.app.nFunctionMode) {
        case FMODE_MENU:
            SetScreenMode(SCREEN_MENU);
            if (gameStates.app.bAutoRunMission) {
                if (StartNewGame(1))
                    gameStates.app.nFunctionMode = FMODE_GAME;
                gameStates.app.bAutoRunMission = 0;
                if (gameStates.app.nFunctionMode == FMODE_GAME)
                    break;
            }
            if (gameData.demoData.bAuto && !gameOpts->demo.bRevertFormat) {
                NDStartPlayback(NULL); // Randomly pick a file
                if (gameData.demoData.nState != ND_STATE_PLAYBACK)
                    Error("No demo files were found for autodemo mode!");
            } else {
                // SetRenderQuality (0);
                MainMenu();
            }
            // if ((gameData.multiplayer.autoNG.bValid > 0) && (gameStates.app.nFunctionMode != FMODE_GAME))
            // gameStates.app.nFunctionMode = FMODE_EXIT;
            break;

        case FMODE_GAME:
            GrabMouse(1, 1);
            RunGame();
            GrabMouse(0, 1);

            gameStates.app.bD1Mission = 0;
            if (gameData.multiplayer.autoNG.bValid)
                gameStates.app.nFunctionMode = FMODE_EXIT;
            else if (gameStates.app.nFunctionMode == FMODE_MENU) {
                audio.StopAllChannels();
                songManager.Play(SONG_TITLE, 1);
            }
            RestoreDefaultModels();
            break;

        default:
            Error("Invalid function mode %d", gameStates.app.nFunctionMode);
        }
    }
}

// ----------------------------------------------------------------------------

void InitThreads(void) {
    if (gameStates.app.bMultiThreaded) {
        int32_t i;
        for (i = 0; i < 2; i++) {
#if MULTI_THREADED_LIGHTS
            gameData.threadData.vertColor.info[i].done = SDL_CreateSemaphore(0);
            gameData.threadData.vertColor.info[i].exec = SDL_CreateSemaphore(0);
            gameData.threadData.vertColor.info[i].bDone = gameData.threadData.vertColor.info[i].bExec =
                gameData.threadData.vertColor.info[i].bQuit = 0;
            gameData.threadData.vertColor.info[i].nId = i;
            gameData.threadData.vertColor.info[i].pThread =
                SDL_CreateThread(VertexColorThread, &gameData.threadData.vertColor.info[i].nId);
#endif
#if MULTI_THREADED_SHADOWS
            gameData.threadData.clipDist.info[i].done = SDL_CreateSemaphore(0);
            gameData.threadData.clipDist.info[i].exec = SDL_CreateSemaphore(0);
            gameData.threadData.clipDist.info[i].nId = i;
            gameData.threadData.clipDist.info[i].pThread =
                SDL_CreateThread(ClipDistThread, &gameData.threadData.clipDist.info[i].nId);
#endif
        }
    }
}

// ------------------------------------------------------------------------------------------

tpfnTIRInit pfnTIRInit = NULL;
tpfnTIRExit pfnTIRExit = NULL;
tpfnTIRStart pfnTIRStart = NULL;
tpfnTIRStop pfnTIRStop = NULL;
tpfnTIRCenter pfnTIRCenter = NULL;
tpfnTIRQuery pfnTIRQuery = NULL;

#define LOAD_TIR_FUNC(_t, _f) \
    if (!((pfn##_f) = (_t)GetProcAddress(hTIRDll, #_f))) \
        return TIRUnload();

#ifdef _WIN32
static HINSTANCE hTIRDll = 0;
#endif

// ------------------------------------------------------------------------------------------

int32_t TIRUnload(void) {
#ifdef _WIN32
    if (hTIRDll)
        FreeLibrary(hTIRDll);
    pfnTIRInit = NULL;
    pfnTIRExit = NULL;
    pfnTIRStart = NULL;
    pfnTIRStop = NULL;
    pfnTIRCenter = NULL;
    pfnTIRQuery = NULL;
    hTIRDll = 0;
#endif
    return gameStates.input.bHaveTrackIR = 0;
}

// ------------------------------------------------------------------------------------------

int32_t TIRLoad(void) {
#ifdef _WIN32
    if (hTIRDll)
        return 1;
#ifdef _M_X64
    hTIRDll = LoadLibrary("d2x-trackir-x64.dll");
#else
    hTIRDll = LoadLibrary("d2x-trackir.dll");
#endif
    if ((size_t)hTIRDll < HINSTANCE_ERROR) {
        hTIRDll = NULL;
        return gameStates.input.bHaveTrackIR = 0;
    }
    LOAD_TIR_FUNC(tpfnTIRInit, TIRInit)
    LOAD_TIR_FUNC(tpfnTIRExit, TIRExit)
    LOAD_TIR_FUNC(tpfnTIRStart, TIRStart)
    LOAD_TIR_FUNC(tpfnTIRStop, TIRStop)
    LOAD_TIR_FUNC(tpfnTIRCenter, TIRCenter)
    LOAD_TIR_FUNC(tpfnTIRQuery, TIRQuery)
    if (pfnTIRInit((HWND)SDL_GetWindowHandle()))
        return gameStates.input.bHaveTrackIR = 1;
    TIRUnload();
    return gameStates.input.bHaveTrackIR = 0;
#else
    return gameStates.input.bHaveTrackIR = 0;
#endif
}

// ----------------------------------------------------------------------------

int32_t InitGraphics(bool bFull = true) {
    int32_t t;

    PrintLog(1, "Initializing graphics\n");
    if ((t = GrInit())) { // doesn't do much
        PrintLog(-1, "Cannot initialize graphics\n");
        Error(TXT_CANT_INIT_GFX, t);
        return 0;
    }
    gameData.renderData.rift.Create();
    PrintLog(1, "Initializing render buffers\n");
    PrintLog(-1);
    if (bFull) {
        PrintLog(1, "Loading default palette\n");
        paletteManager.SetDefault(paletteManager.Load(D2_DEFAULT_PALETTE, NULL));
        CCanvas::Current()->SetPalette(paletteManager.Default()); // just need some valid palette here
        PrintLog(-1);
        PrintLog(1, "Initializing game fonts\n");
        fontManager.Setup(); // must load after palette data loaded.
        PrintLog(-1);
        PrintLog(1, "Setting screen mode\n");
        SetScreenMode(SCREEN_MENU);
        PrintLog(-1);
        PrintLog(1, "Showing loading screen\n");
        ShowLoadingScreen();
        PrintLog(-1);
    }
    PrintLog(-1);
    return 1;
}

//------------------------------------------------------------------------------

static inline int32_t InitGaugeSize(void) { return 19; }

//------------------------------------------------------------------------------

static int32_t loadOp = 0;

static int32_t InitializePoll(CMenu &menu, int32_t &key, int32_t nCurItem, int32_t nState) {
    if (nState)
        return nCurItem;

    // paletteManager.ResumeEffect ();
    switch (loadOp) {
    case 0:
        PrintLog(1, "Creating default tracker list\n");
        tracker.CreateList();
        PrintLog(-1);
        break;
    case 1:
        PrintLog(1, "Loading ban list\n");
        banList.Load();
        PrintLog(-1);
        break;
    case 2:
        PrintLog(1, "Initializing control types\n");
        controls.SetType();
        PrintLog(-1);
        break;
    case 3:
        PrintLog(1, "Initializing keyboard\n");
        KeyInit();
        PrintLog(-1);
        break;
    case 4:
        PrintLog(1, "Initializing joystick\n");
        DoJoystickInit();
        PrintLog(-1);
        break;
    case 5:
        int32_t i;
        if ((i = FindArg("-xcontrol")) > 0)
            externalControls.Init(strtol(appConfig[i + 1], NULL, 0), strtol(appConfig[i + 2], NULL, 0));
        break;
    case 6:
        PrintLog(1, "Initializing movies\n");
        if (FindArg("-nohires") || FindArg("-nohighres") || !GrVideoModeOK(MENU_HIRES_MODE))
            gameOpts->movies.bHires = gameStates.menus.bHires = gameStates.menus.bHiresAvailable = 0;
        else
            gameStates.menus.bHires = gameStates.menus.bHiresAvailable = 1;
        movieManager.InitLibs(); // init movie libraries
        PrintLog(-1);
        break;
    case 7:
        BMInit();
        break;
    case 8:
        PrintLog(1, "Initializing sound\n");
        audio.Setup(1);
        PrintLog(-1);
        break;
    case 9:
        PrintLog(1, "Loading hoard data\n");
        LoadHoardData();
        PrintLog(-1);
        break;
    case 10:
        error_init(ShowInGameWarning, NULL);
        break;
    case 11:
        break;
    case 12:
        PrintLog(1, "Initializing texture merge buffer\n");
        TexMergeInit(100); // 100 cache bitmaps
        PrintLog(-1);
        break;
    case 13:
        InitPowerupTables();
        break;
    case 14:
        LoadGameBackground();
        atexit(CloseGame);
        break;
    case 15:
        InitThreads();
        break;
    case 16:
        PiggyInitMemory();
        break;
    case 17:
        if (!FindArg("-nomouse"))
            MouseInit();
        break;
    case 18:
        PrintLog(1, "Enabling TrackIR support\n");
        TIRLoad();
        PrintLog(-1);
        break;
    }
    loadOp++;
    if (gameStates.app.bProgressBars && gameOpts->menus.nStyle) {
        if (loadOp == InitGaugeSize())
            key = -2;
        else {
            menu[0].Value()++;
            menu[0].Rebuild();
            key = 0;
        }
    }
    // paletteManager.ResumeEffect ();
    return nCurItem;
}

//------------------------------------------------------------------------------

void InitializeGauge(void) {
    loadOp = 0;
    ProgressBar(TXT_INITIALIZING, 1, 0, InitGaugeSize(), InitializePoll);
}

// ----------------------------------------------------------------------------

void InitArgs(int32_t argC, char **argV) {
    appConfig.Destroy();
    appConfig.Init();
    PrintLog(1, "Loading program arguments\n");
    appConfig.Load(argC, argV);
    appConfig.Load(appConfig.Filename(DBG != 0));
    appConfig.PrintLog();
    PrintLog(-1);
}

// ----------------------------------------------------------------------------

int32_t Initialize(int32_t argc, char *argv[]) {
    PrintLog(1, "Initializing data\n");
    gameData.timeData.xGameTotal = 0;
    gameData.appData.argC = argc;
    gameData.appData.argV = reinterpret_cast<char **>(argv);

    SDL_setenv("SDL_DISABLE_LOCK_KEYS", "1", 1);
    hogFileManager.Init("", "");
    CObject::InitTables();
    InitGameStates();
    gameData.Init();
    InitExtraGameInfo();
    InitNetworkData();
    gameOptions[0].Init();
    InitArgs(argc, argv);
    EvalArgs();
    GetAppFolders(true);
    CheckAndFixSetup();
    gameStates.app.nLogLevel = appConfig.Int("-printlog", 1);
#ifdef DESCENT_EXECUTABLE_VERSION
    PrintLog(0, "%s (%s)\n", DESCENT_VERSION, DESCENT_EXECUTABLE_VERSION);
#else
    PrintLog(0, "%s\n", DESCENT_VERSION);
#endif
    InitArgs(argc, argv);
    GetAppFolders(false);
#ifdef D2X_MEM_HANDLER
    MemInit();
#endif
    error_init(NULL, NULL);
    *szAutoHogFile = *szAutoMission = '\0';
    EvalArgs();
    gameOptions[1].Init();
    GetNumThreads();
    DefaultAllSettings(true);
    gameOpts->render.nMathFormat = gameOpts->render.nDefMathFormat;
    PrintLog(0, "Loading text resources\n");
    PrintLog(0, "Loading main hog file\n");
    if (!(hogFileManager.Init("descent2.hog", gameFolders.game.szData[0]) ||
          (gameStates.app.bDemoData = hogFileManager.Init("d2demo.hog", gameFolders.game.szData[0])))) {
        PrintLog(1, "Descent 2 data not found\n");
        Error(TXT_NO_HOG2);
    }
    fontManager.SetScale(1.0f);
    LoadGameTexts();
    PrintLog(0, "Reading configuration file\n");
    ReadConfigFile();
    if (!InitGraphics())
        return 1;
    backgroundManager.Rebuild();
    console.Setup(
        SMALL_FONT,
        &gameData.renderData.screen,
        CON_NUM_LINES,
        0,
        0,
        gameData.renderData.screen.Width(),
        gameData.renderData.screen.Height() / 2);
    if (gameStates.app.bProgressBars && gameOpts->menus.nStyle) {
        InitializeGauge();
    } else {
        CMenu m(1);
        int32_t key = 0;
        m.AddGauge("", "", -1, 1000); // dummy for InitializePoll()
        messageBox.Show(TXT_INITIALIZING);
        for (loadOp = 0; loadOp < InitGaugeSize();) {
            InitializePoll(m, key, 0, 0);
        }
    }
    messageBox.Clear();
    PrintBanner();
    if (!gameStates.app.bAutoRunMission) {
        PrintLog(0, "Showing title screens\n");
        if (!ShowTitleScreens()) {
            ShowLoadingScreen();
        }
    }
    if (FindArg("-norun")) {
        PrintLog(-1);
        return 0;
    }
    PrintLog(0, "Loading hires models\n");
    LoadHiresModels(0);
    LoadModelData();
    LoadIpToCountry();
    ogl.InitShaders(); // required for some menus to show all possible choices
    return 0;
}

// ----------------------------------------------------------------------------

int32_t CleanUp(void) {
    if (gameStates.input.bHaveTrackIR) {
        pfnTIRExit();
        TIRUnload();
    }
    songManager.StopAll();
    audio.StopCurrentSong();
    SaveModelData();
    PrintLog(0, "Saving configuration file\n");
    WriteConfigFile(true);
    PrintLog(0, "Saving player profile\n");
    SavePlayerProfile();
    PrintLog(0, "Releasing tracker list\n");
    tracker.DestroyList();
    profile.Destroy();
    ogl.DestroyDrawBuffers();
    return 0;
}

int32_t SDLCALL main(int32_t argc, char *argv[]) {
    gameStates.app.bInitialized = 0;
    gameStates.app.SRand();
    if (Initialize(argc, argv)) {
        return -1;
    }

    // If built with editor, option to auto-load a level and quit game
    // to write certain data.
    PrintLog(1, "Loading player profile\n");
    DoSelectPlayer();
    CreateSoundThread(); // needs to be repeated here due to dependency on data read in DoSelectPlayer()

    // handle automatic launch of a demo playback
    if (gameData.demoData.bAuto && !gameOpts->demo.bRevertFormat) {
        NDStartPlayback(gameData.demoData.fnAuto);
        if (gameData.demoData.nState == ND_STATE_PLAYBACK) {
            SetFunctionMode(FMODE_GAME);
        }
    }
    // do this here because the demo code can do a __asm int32_t 3; longjmp when trying to
    // autostart a demo from the main menu, never having gone into the game
    setjmp(gameExitPoint);
    backgroundManager.Rebuild();
    gameStates.app.bInitialized = 1;
    // handle direct loading and starting of a mission specified via the command line
    if (*szAutoHogFile && *szAutoMission) {
        hogFileManager.UseMission(szAutoHogFile);
        gameStates.app.bAutoRunMission = hogFileManager.AltFiles().bInitialized;
    }
    PrintLog(-1);
    PrintLog(0, "Invoking main menu\n");
    MainLoop();
    CleanUp();
    return 0;
}
