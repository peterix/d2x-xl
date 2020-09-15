#ifdef _WIN32
#include <windows.h>
#include <stddef.h>
#include <io.h>
#endif
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>

#include "descent.h"
#include "ogl_defs.h"
#include "ogl_lib.h"
#include "ogl_shader.h"
#include "ogl_color.h"
#include "light.h"
#include "dynlight.h"
#include "lightmap.h"
#include "segmath.h"
#include "wall.h"
#include "loadgamedata.h"
#include "fbuffer.h"
#include "error.h"
#include "u_mem.h"
#include "menu.h"
#include "menu.h"
#include "text.h"
#include "key.h"
#include "netmisc.h"
#include "gamepal.h"
#include "loadgeometry.h"
#include "renderthreads.h"
#include "createmesh.h"

CLightmapManager lightmapManager;

int32_t PrecomputeLightmapsPoll(CMenu &menu, int32_t &key, int32_t nCurItem, int32_t nState);

//------------------------------------------------------------------------------

#define LMAP_REND2TEX 0
#define TEXTURE_CHECK 1

#define LIGHTMAP_DATA_VERSION 50
#define LM_W LIGHTMAP_WIDTH
#define LM_H LIGHTMAP_WIDTH

#define OPTIMIZED_THREADS 1

//------------------------------------------------------------------------------

typedef struct tLightmapDataHeader {
    int32_t nVersion;
    int32_t nCheckSum;
    int32_t nSegments;
    int32_t nVertices;
    int32_t nFaces;
    int32_t nLights;
    int32_t nMaxLightRange;
    int32_t nBuffers;
    int32_t bCompressed;
} tLightmapDataHeader;

//------------------------------------------------------------------------------

GLhandleARB lmShaderProgs[3] = {0, 0, 0};
GLhandleARB lmFS[3] = {0, 0, 0};
GLhandleARB lmVS[3] = {0, 0, 0};

#if DBG
int32_t lightmapWidth[5] = {8, 16, 32, 64, 128};
#else
int32_t lightmapWidth[5] = {16, 32, 64, 128, 256};
#endif

tLightmap dummyLightmap;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

CLightmapProgress::CLightmapProgress()
    : m_pProgressMenu(NULL),
      m_pTotalProgress(NULL),
      m_pLevelProgress(NULL),
      m_pLevelCount(NULL),
      m_pTime(NULL),
      m_pLock(NULL),
      m_nLocalProgress(0),
      m_fTotal(0.0f),
      m_tStart(0),
      m_nSkipped(0),
      m_bActive(false) {
    m_pLock = SDL_CreateSemaphore(1);
}

//------------------------------------------------------------------------------

CLightmapProgress::~CLightmapProgress() {
    m_pProgressMenu = NULL;
    m_pTotalProgress = NULL;
    m_pLevelProgress = NULL;
    m_pLevelCount = NULL;
    m_pTime = NULL;
    m_pLock = NULL;
    m_nLocalProgress = 0;
    m_fTotal = 0.0f;
    m_tStart = 0;
    m_nSkipped = 0;
    m_bActive = false;
    if (m_pLock) {
        SDL_DestroySemaphore(m_pLock);
        m_pLock = NULL;
    }
}

//------------------------------------------------------------------------------

void CLightmapProgress::Setup(void) {
    if (!(m_pProgressMenu = CMenu::Active()))
        return;

    Timeout().Start(33);

    if (!gameStates.app.bPrecomputeLightmaps) {
        m_pTotalProgress = (*m_pProgressMenu)["progress bar 1"];
        return;
    }

    if ((m_pLevelProgress = (*m_pProgressMenu)["subtitle 1"]))
        m_pLevelProgress->SetText("level progress");
    if (!(m_pLevelProgress = (*m_pProgressMenu)["progress bar 2"]))
        return;

    if (!m_bActive) {
        if (!((m_pTotalProgress = (*m_pProgressMenu)["progress bar 1"])))
            return;
        if (!(m_pTime = (*m_pProgressMenu)["time"]))
            return;
        m_fTotal = float(m_pTotalProgress->MaxValue());
        m_tStart = SDL_GetTicks();
        m_pLevelCount = (*m_pProgressMenu)["level count"];
        m_nSkipped = 0;
        m_bActive = true;
    }

    if (m_pLevelCount) {
        char szLabel[50];
        sprintf(szLabel, "%d / %d", m_pTotalProgress->Value() / 100 + 1, m_pTotalProgress->MaxValue() / Scale());
        m_pLevelCount->SetText(szLabel);
    }

    m_pLevelProgress->Value() = 0;
    m_pLevelProgress->MaxValue() = FACES.nFaces;
    m_nLocalProgress = 0;
}

//------------------------------------------------------------------------------

void CLightmapProgress::Render(int32_t nThread) {
    if (m_pProgressMenu /*&& !nThread*/) {
        // Lock ();
        m_pProgressMenu->Render(m_pProgressMenu->Title(), m_pProgressMenu->SubTitle());
        // Unlock ();
    }
}

//------------------------------------------------------------------------------

void CLightmapProgress::Update(int32_t nThread) {
#if USE_OPENMP
#pragma omp critical(LightmapProgressUpdate)
#endif
    {
        if (m_pProgressMenu) {
// Lock ();
#if OPTIMIZED_THREADS
            if (!gameStates.app.bPrecomputeLightmaps) {
                if (m_pTotalProgress) {
                    m_pTotalProgress->Value()++;
                    static CTimeout to(33);
                    // if (!nThread)
                    if (to.Expired()) {
                        m_pTotalProgress->Rebuild();
                        // Render ();
                    }
                }
            } else
#endif
            {
                if (m_bActive) {
                    m_pLevelProgress->Value()++;
                    static CTimeout to(33);
                    // if (!nThread)
                    int32_t bRebuild = to.Expired();
                    if (bRebuild)
                        m_pLevelProgress->Rebuild();
                    float fLevelProgress = float(m_pLevelProgress->Value()) / float(FACES.nFaces);
                    int32_t nLevelProgress = int32_t(float(Scale()) * fLevelProgress + 0.5f);
                    if (m_nLocalProgress < nLevelProgress) {
                        m_nLocalProgress = nLevelProgress;
                        m_pTotalProgress->Value()++;
                        if (bRebuild)
                            m_pTotalProgress->Rebuild();
                    }
                    // if (!nThread)
                    {
                        if (m_pTime) {
                            static CTimeout to(1000);
                            if (to.Expired()) {
                                int32_t nTotalProgress = m_pTotalProgress->Value();
                                int32_t tLeft, tPassed = SDL_GetTicks() - m_tStart;
                                if (nTotalProgress / Scale() - m_nSkipped > 0)
                                    tLeft = Max(
                                        int32_t(
                                            float(tPassed) * m_fTotal / float(nTotalProgress - m_nSkipped * Scale())) -
                                            tPassed,
                                        0);
                                else { // for the first level, estimate the time needed for the entire level and scale
                                       // with number of levels to be done
                                    int32_t tLevel = int32_t(float(tPassed) / fLevelProgress);
                                    tLeft = int32_t(float(tLevel) * m_fTotal / float(Scale())) - tPassed;
                                }
                                tPassed = (tPassed + 500) / 1000;
                                tLeft = (tLeft + 500) / 1000;
                                char szTime[50];
                                sprintf(
                                    szTime,
                                    "%d:%02d:%02d / %d:%02d:%02d",
                                    tPassed / 3600,
                                    (tPassed / 60) % 60,
                                    tPassed % 60,
                                    tLeft / 3600,
                                    (tLeft / 60) % 60,
                                    tLeft % 60);
                                m_pTime->SetText(szTime);
                            }
                        }
#if !OPTIMIZED_THREADS
                        Render();
#endif
                    }
                }
            }
            // Unlock ();
        }
    }
}

//------------------------------------------------------------------------------

void CLightmapProgress::Skip(int32_t i) {
    ++m_nSkipped;
    if (m_pProgressMenu && m_pTotalProgress) {
        // Lock ();
        m_pTotalProgress->Value() += i * Scale();
        // Unlock ();
        m_fTotal = float(m_pTotalProgress->MaxValue() - m_nSkipped * Scale());
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int32_t CLightmapBuffer::Bind(void) {
    if (m_handle)
        return 1;
    ogl.GenTextures(1, &m_handle);
    if (!m_handle)
        return 0;
    ogl.BindTexture(m_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, LIGHTMAP_BUFWIDTH, LIGHTMAP_BUFWIDTH, 0, GL_RGB, GL_UNSIGNED_BYTE, m_lightmaps);
    return 1;
}

//------------------------------------------------------------------------------

void CLightmapBuffer::Release(void) {
    if (m_handle) {
        ogl.DeleteTextures(1, reinterpret_cast<GLuint *>(&m_handle));
        m_handle = 0;
    }
}

//------------------------------------------------------------------------------

void CLightmapBuffer::ToGrayScale(void) {
    CRGBColor *pColor = &m_lightmaps[0][0];
    for (int32_t j = LIGHTMAP_BUFWIDTH * LIGHTMAP_BUFWIDTH; j; j--, pColor++)
        pColor->ToGrayScale(1);
}

//------------------------------------------------------------------------------

void CLightmapBuffer::Posterize(void) {
    CRGBColor *pColor = &m_lightmaps[0][0];
    for (int32_t j = LIGHTMAP_BUFWIDTH * LIGHTMAP_BUFWIDTH; j; j--, pColor++)
        pColor->Posterize();
}

//------------------------------------------------------------------------------

int32_t CLightmapBuffer::Read(CFile &cf, int32_t bCompressed) {
    return cf.Read(m_lightmaps, sizeof(m_lightmaps), 1, bCompressed) == 1;
}

//------------------------------------------------------------------------------

int32_t CLightmapBuffer::Write(CFile &cf, int32_t bCompressed) {
    return cf.Write(m_lightmaps, sizeof(m_lightmaps), 1, bCompressed) == 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CLightmapList::Create(int32_t nBuffers) {
    if (!m_buffers.Create(nBuffers))
        return false;
    m_buffers.Clear();
    for (int32_t i = 0; i < nBuffers; i++) {
        if (!(m_buffers[i] = new CLightmapBuffer)) {
            for (; i > 0; --i)
                delete m_buffers[i];
            m_buffers.Destroy();
            return false;
        }
    }
    m_nBuffers = nBuffers;
    return true;
}

//------------------------------------------------------------------------------

void CLightmapList::Destroy(void) {
    if (m_buffers.Buffer()) {
        for (int32_t i = 0; i < m_nBuffers; i++) {
            if (m_buffers[i]) {
                delete m_buffers[i];
                m_buffers[i] = NULL;
            }
        }
        m_buffers.Destroy();
        m_nBuffers = 0;
    }
}

//------------------------------------------------------------------------------

int32_t CLightmapList::Realloc(int32_t nBuffers) {
    if (nBuffers == m_nBuffers)
        return 1;

    int32_t nResult = 1;

#if USE_OPENMP
#pragma omp critical(LightmapListRealloc)
#endif
    {
        CArray<CLightmapBuffer *> buffers;

        if (buffers.Create(nBuffers)) {
            buffers.Clear();
            memcpy(buffers.Buffer(), m_buffers.Buffer(), Min(nBuffers, m_nBuffers) * sizeof(CLightmapBuffer *));
            if (nBuffers < m_nBuffers) {
                for (int32_t i = nBuffers; i < m_nBuffers; i++)
                    if (m_buffers[i])
                        delete m_buffers[i];
            } else {
                for (int32_t i = m_nBuffers; i < nBuffers; i++) {
                    if (!(buffers[i] = new CLightmapBuffer)) {
                        for (; i > m_nBuffers; --i)
                            delete buffers[i];
                        buffers.Destroy();
                        nResult = 0;
                    }
                }
            }
            if (nResult) {
                m_buffers.Clear();
                m_buffers.Destroy();
                m_buffers.SetBuffer(buffers.Buffer(), 0, buffers.Length());
                buffers.SetBuffer(NULL);
                m_nBuffers = nBuffers;
            }
        }
    }
    return nResult;
}

//------------------------------------------------------------------------------

int32_t CLightmapList::Bind(int32_t nLightmap) {
    return (m_buffers.Buffer() && m_buffers[nLightmap]) ? m_buffers[nLightmap]->Bind() : 0;
}

//------------------------------------------------------------------------------

int32_t CLightmapList::BindAll(void) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        if (!Bind(i))
            return 0;
    return 1;
}

//------------------------------------------------------------------------------

void CLightmapList::Release(int32_t nLightmap) {
    if (m_buffers.Buffer() && m_buffers[nLightmap])
        m_buffers[nLightmap]->Release();
}

//------------------------------------------------------------------------------

void CLightmapList::ReleaseAll(void) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        Release(i);
}

//------------------------------------------------------------------------------

void CLightmapList::ToGrayScale(int32_t nLightmap) {
    if (m_buffers.Buffer() && m_buffers[nLightmap])
        m_buffers[nLightmap]->ToGrayScale();
}

//------------------------------------------------------------------------------

void CLightmapList::ToGrayScaleAll(void) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        ToGrayScale(i);
}

//------------------------------------------------------------------------------

void CLightmapList::Posterize(int32_t nLightmap) {
    if (m_buffers.Buffer() && m_buffers[nLightmap])
        m_buffers[nLightmap]->Posterize();
}

//------------------------------------------------------------------------------

void CLightmapList::PosterizeAll(void) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        Posterize(i);
}

//------------------------------------------------------------------------------

int32_t CLightmapList::Read(int32_t nLightmap, CFile &cf, int32_t bCompressed) {
    return (m_buffers.Buffer() && m_buffers[nLightmap]) ? m_buffers[nLightmap]->Read(cf, bCompressed) : 0;
}

//------------------------------------------------------------------------------

int32_t CLightmapList::ReadAll(CFile &cf, int32_t bCompressed) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        if (!Read(i, cf, bCompressed))
            return 0;
    return 1;
}

//------------------------------------------------------------------------------

int32_t CLightmapList::Write(int32_t nLightmap, CFile &cf, int32_t bCompressed) {
    return (m_buffers.Buffer() && m_buffers[nLightmap]) ? m_buffers[nLightmap]->Write(cf, bCompressed) : 0;
}

//------------------------------------------------------------------------------

int32_t CLightmapList::WriteAll(CFile &cf, int32_t bCompressed) {
    for (int32_t i = 0; i < m_nBuffers; i++)
        if (!Write(i, cf, bCompressed))
            return 0;
    return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void CLightmapFaceData::Setup(CSegFace *pFace) {
    CSide *pSide = SEGMENT(pFace->m_info.nSegment)->m_sides + pFace->m_info.nSide;
    m_nType = pSide->m_nType;
    m_vNormal = pSide->m_normals[2];
    m_vCenter = pSide->Center();
    CFixVector::Normalize(m_vNormal);
    m_vcd.vertNorm.Assign(m_vNormal);
    CFloatVector3::Normalize(m_vcd.vertNorm);
    InitVertColorData(m_vcd);
    m_vcd.pVertPos = &m_vcd.vertPos;
    m_vcd.fMatShininess = 4;
    memcpy(m_sideVerts, pSide->m_corners, sizeof(m_sideVerts));
    // memcpy (m_sideVerts, pFace->m_info.index, sizeof (m_sideVerts));
    m_nColor = 0;
    memset(m_texColor, 0, LM_W * LM_H * sizeof(CRGBColor));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int32_t CLightmapManager::Bind(int32_t nLightmap) { return m_list.Bind(nLightmap); }

//------------------------------------------------------------------------------

int32_t CLightmapManager::BindAll(void) {
    for (int32_t i = 0; i < m_list.m_nBuffers; i++)
        if (!Bind(i))
            return 0;
    return 1;
}

//------------------------------------------------------------------------------

void CLightmapManager::ReleaseAll(void) { m_list.ReleaseAll(); }

//------------------------------------------------------------------------------

void CLightmapManager::Destroy(void) {
    if (m_list.m_info.Buffer()) {
        m_list.m_info.Destroy();
        m_list.Destroy();
        m_list.m_nBuffers = 0;
    }
}

//------------------------------------------------------------------------------

inline void CLightmapManager::ComputePixelOffset(CFixVector &vOffs, CFixVector &v1, CFixVector &v2, int32_t nOffset) {
    vOffs = (v2 - v1) * nOffset;
}

//------------------------------------------------------------------------------

void CLightmapManager::RestoreLights(int32_t bVariable) {
    CDynLight *pLight = lightManager.Lights();

    for (int32_t i = lightManager.LightCount(0); i; i--, pLight++)
        if (!(pLight->info.nType || (pLight->info.bVariable && !bVariable)))
            pLight->info.bOn = 1;
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::CountLights(int32_t bVariable) {
    CDynLight *pLight = lightManager.Lights();
    int32_t nLights = 0;

    if (!gameStates.render.bPerPixelLighting)
        return 0;
    for (int32_t i = lightManager.LightCount(0); i; i--, pLight++)
        if (!(pLight->info.nType || (pLight->info.bVariable && !bVariable)))
            nLights++;
    return nLights;
}

//------------------------------------------------------------------------------

double CLightmapManager::SideRad(int32_t nSegment, int32_t nSide) {
    int32_t i;
    double h, xMin, xMax, yMin, yMax, zMin, zMax;
    double dx, dy, dz;
    CFixVector *v;
    CSide *pSide = SEGMENT(nSegment)->Side(nSide);
    uint16_t *sideVerts = pSide->Corners();

    xMin = yMin = zMin = 1e300;
    xMax = yMax = zMax = -1e300;
    for (i = 0; i < pSide->CornerCount(); i++) {
        v = gameData.segData.vertices + sideVerts[i];
        h = (*v).v.coord.x;
        if (xMin > h)
            xMin = h;
        if (xMax < h)
            xMax = h;
        h = (*v).v.coord.y;
        if (yMin > h)
            yMin = h;
        if (yMax < h)
            yMax = h;
        h = (*v).v.coord.z;
        if (zMin > h)
            zMin = h;
        if (zMax < h)
            zMax = h;
    }
    dx = xMax - xMin;
    dy = yMax - yMin;
    dz = zMax - zMin;
    return sqrt(dx * dx + dy * dy + dz * dz) / (double)I2X(2);
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::Init(int32_t bVariable) {
    CDynLight *pLight;
    CSegFace *pFace = NULL;
    int32_t nIndex, i;
    tLightmapInfo *pLightmapInfo; // temporary place to put light data.
    double sideRad;

    // first step find all the lights in the level.  By iterating through every surface in the level.
    if (!(m_list.m_nLights = CountLights(bVariable))) {
        if (!m_list.m_buffers.Create(1))
            return -1;
        m_list.m_buffers.Clear();
        return 0;
    }
    if (!m_list.m_info.Create(m_list.m_nLights)) {
        m_list.m_nLights = 0;
        return -1;
    }
    if (!m_list.Create(
            (FACES.nFaces + LIGHTMAP_BUFSIZE + 1) /
            LIGHTMAP_BUFSIZE)) { // add the black and white lightmap in - there are levels that do not have either or
                                 // both of them
        m_list.m_info.Destroy();
        m_list.m_nLights = 0;
        return -1;
    }
    m_list.m_info.Clear();
    m_list.m_nLights = 0;
    // first lightmap is dummy lightmap for multi pass lighting
    pLightmapInfo = m_list.m_info.Buffer();
    for (pLight = lightManager.Lights(), i = lightManager.LightCount(0); i; i--, pLight++) {
        if (pLight->info.nType || (pLight->info.bVariable && !bVariable))
            continue;
        if (pFace == pLight->info.pFace)
            continue;
        pFace = pLight->info.pFace;
        sideRad = (double)pFace->m_info.fRads[1] / 10.0;
        nIndex = pFace->m_info.nSegment * 6 + pFace->m_info.nSide;
        // Process found light.
        pLightmapInfo->range += sideRad;
        // find where it is in the level.
        pLightmapInfo->vPos = SEGMENT(pFace->m_info.nSegment)->SideCenter(pFace->m_info.nSide);
        pLightmapInfo->nIndex = nIndex;
        // find light direction, currently based on first 3 points of CSide, not always right.
        CFixVector *pNormal = SEGMENT(pFace->m_info.nSegment)->m_sides[pFace->m_info.nSide].m_normals;
        pLightmapInfo->vDir = CFixVector::Avg(pNormal[0], pNormal[1]);
        pLightmapInfo++;
    }
    return m_list.m_nLights = (int32_t)(pLightmapInfo - m_list.m_info.Buffer());
}

//------------------------------------------------------------------------------

#if LMAP_REND2TEX

// create 512 brightness values using inverse square
void InitBrightmap(uint8_t *brightmap) {
    int32_t i;
    double h;

    for (i = 512; i; i--, brightmap++) {
        h = (double)i / 512.0;
        h *= h;
        *brightmap = (uint8_t)FRound((255 * h));
    }
}

//------------------------------------------------------------------------------

// create a color/brightness table
void InitLightmapInfo(uint8_t *lightmap, uint8_t *brightmap, GLfloat *color) {
    int32_t i, j;
    double h;

    for (i = 512; i; i--, brightmap++)
        for (j = 0; j < 3; j++, lightmap++)
            *lightmap = (uint8_t)FRound(*brightmap * color[j]);
}

#endif // LMAP_REND2TEX

//------------------------------------------------------------------------------

bool CLightmapManager::FaceIsInvisible(CSegFace *pFace) {
#if DBG
    if ((pFace->m_info.nSegment == nDbgSeg) && ((nDbgSide < 0) || (pFace->m_info.nSide == nDbgSide)))
        BRP;
#endif
    CSegment *pSeg = SEGMENT(pFace->m_info.nSegment);
    if (pSeg->m_function == SEGMENT_FUNC_SKYBOX) {
        pFace->m_info.nLightmap = 1;
        return true;
    }
    if (pSeg->m_children[pFace->m_info.nSide] < 0)
        return false;

    CWall *pWall = pSeg->m_sides[pFace->m_info.nSide].Wall();
    if (!pWall || (pWall->nType != WALL_OPEN))
        return false;

    int32_t nTrigger = 0;
    for (;;) {
        if (0 > (nTrigger = pWall->IsTriggerTarget(nTrigger)))
            return false;
        CTrigger *pTrigger = GEOTRIGGER(nTrigger);
        if (pTrigger && (pTrigger->m_info.nType == TT_CLOSE_WALL))
            return false;
        nTrigger++;
    }
    pFace->m_info.nLightmap = 0;
    return true;
}

//------------------------------------------------------------------------------

// static float offset [2] = {1.3846153846f, 3.2307692308f};
// static float weight [3] = {0.2270270270f, 0.3162162162f, 0.0702702703f};
static float weight[5] = {0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f};

static inline void Add(CLightmapFaceData &source, int32_t x, int32_t y, int32_t offset, CFloatVector3 &color) {
    if (x < 0)
        x = 0;
    else if (x >= LM_W)
        x = LM_W - 1;
    if (y < 0)
        y = 0;
    else if (y >= LM_H)
        y = LM_H - 1;
    CRGBColor &sourceColor = source.m_texColor[y * LM_W + x];
    float w = weight[offset];
    color.Red() += sourceColor.r * w;
    color.Green() += sourceColor.g * w;
    color.Blue() += sourceColor.b * w;
}

void CLightmapManager::Blur(CSegFace *pFace, CLightmapFaceData &source, CLightmapFaceData &dest, int32_t direction) {
    int32_t w = LM_W, h = LM_H;
    int32_t xScale = 1 - direction, yScale = direction, xo, yo;
    CRGBColor *destColor = dest.m_texColor;
    CFloatVector3 color;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++, destColor++) {
            color.Set(0.0f, 0.0f, 0.0f);
            Add(source, x, y, 0, color);
            for (int32_t offset = 1; offset < 5; offset++) {
                xo = offset * xScale;
                yo = offset * yScale;
                Add(source, x - xo, y - yo, offset, color);
                Add(source, x + xo, y + yo, offset, color);
            }
            destColor->Set((uint8_t)FRound(color.Red()), (uint8_t)FRound(color.Green()), (uint8_t)FRound(color.Blue()));
        }
    }
}

void CLightmapManager::Blur(CSegFace *pFace, CLightmapFaceData &source) {
#if !DBG
    CLightmapFaceData tempData;

    Blur(pFace, source, tempData, 0);
    Blur(pFace, tempData, source, 1);
#endif
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::Copy(CRGBColor *pTexColor, uint16_t nLightmap) {
    int32_t i = nLightmap % LIGHTMAP_BUFSIZE;
    if (!m_list.Realloc(Max(i + 1, m_list.m_nBuffers)))
        return 0;
    CLightmapBuffer *pBuffer = m_list.m_buffers[nLightmap / LIGHTMAP_BUFSIZE];
    int32_t x = (i % LIGHTMAP_ROWSIZE) * LM_W;
    int32_t y = (i / LIGHTMAP_ROWSIZE) * LM_H;
    for (i = 0; i < LM_H; i++, y++, pTexColor += LM_W)
        memcpy(&pBuffer->m_lightmaps[y][x], pTexColor, LM_W * sizeof(CRGBColor));
    return 1;
}

//------------------------------------------------------------------------------

void CLightmapManager::CreateSpecial(CRGBColor *pTexColor, uint16_t nLightmap, uint8_t nColor) {
    memset(pTexColor, nColor, LM_W * LM_H * sizeof(CRGBColor));
    Copy(pTexColor, nLightmap);
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::CompareFaces(const tSegFacePtr *pf, const tSegFacePtr *pm) {
    int32_t k = (*pf)->m_info.nKey;
    int32_t m = (*pm)->m_info.nKey;

    return (k < m) ? -1 : (k > m) ? 1 : 0;
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::MainThreadId(void) {
    for (int32_t i = 0; i < gameStates.app.nThreads; i++)
        if (m_bActiveThreads[i])
            return i;
    return -1;
}

//------------------------------------------------------------------------------

#if OPTIMIZED_THREADS

//------------------------------------------------------------------------------
// build one entire lightmap in single threaded mode or in horizontal stripes when multi threaded

void CLightmapManager::Build(CSegFace *pFace, int32_t nThread) {
    CFixVector *pPixelPos;
    CRGBColor *pTexColor;
    CFloatVector3 color;
    int32_t w, h, x, y, yMin, yMax;
    uint8_t nTriangles = pFace->m_info.nTriangles - 1;
    int16_t nSegment = pFace->m_info.nSegment;
    int8_t nSide = (int8_t)pFace->m_info.nSide;
    bool bBlack, bWhite;
    CLightmapData &data = m_data[nThread];

    if (SEGMENT(pFace->m_info.nSegment)->m_function == SEGMENT_FUNC_SKYBOX) {
        pFace->m_info.nLightmap = 1;
        return;
    }
    if (FaceIsInvisible(pFace))
        return;

    data.Setup(pFace);

    h = LM_H;
    w = LM_W;

    yMin = 0;
    yMax = h;

#if DBG
    if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
        BRP;
#endif

    m_bActiveThreads[nThread] = true;
    data.m_vcd.pVertPos = &data.m_vcd.vertPos;
    pPixelPos = data.m_pixelPos + yMin * w;

    // move edges for lightmap calculation slighty into the face to avoid lighting errors due to numerical errors when
    // computing point to face visibility for light sources and lightmap vertices. Otherwise these may occur at the
    // borders of a face when the face to be lit is adjacent to a light emitting face. The result of such lighting
    // errors are dark spots at the borders of level geometry faces.

    CFloatVector corners[4] = {
        FVERTICES[data.m_sideVerts[0]],
        FVERTICES[data.m_sideVerts[1]],
        FVERTICES[data.m_sideVerts[2]],
        FVERTICES[data.m_sideVerts[nTriangles ? 3 : 0]],
    };

    CFloatVector o0 = corners[0];
    o0 -= corners[1];
    CFloatVector::Normalize(o0);
    o0 *= 0.0015f;

    CFloatVector o1 = corners[1];
    o1 -= corners[2];
    CFloatVector::Normalize(o1);
    o1 *= 0.0015f;

    CFloatVector o2 = corners[2];
    o2 -= corners[3];
    CFloatVector::Normalize(o2);
    o2 *= 0.0015f;

    if (!nTriangles) {
        corners[0] += o2;
        corners[0] -= o0;
        corners[1] += o0;
        corners[1] -= o1;
        corners[2] += o1;
        corners[2] -= o2;
    } else {
        CFloatVector o3 = corners[3];
        o3 -= corners[0];
        CFloatVector::Normalize(o3);
        o3 *= 0.0015f;

        corners[0] += o3;
        corners[0] -= o0;
        corners[1] += o0;
        corners[1] -= o1;
        corners[2] += o1;
        corners[2] -= o2;
        corners[3] += o2;
        corners[3] -= o3;
    }

    CFixVector v0, v1, v2, v3;

    v0.Assign(corners[0]);
    v1.Assign(corners[1]);
    v2.Assign(corners[2]);
    v3.Assign(corners[3]);

#else

CFixVector v0 = VERTICES[data.m_sideVerts[0]];
CFixVector v1 = VERTICES[data.m_sideVerts[1]];
CFixVector v2 = VERTICES[data.m_sideVerts[2]];
CFixVector v3 = VERTICES[data.m_sideVerts[3]];

#endif

    CFixVector dx, dy;

    if (!nTriangles) {
        CFixVector l0 = v2 - v1;
        CFixVector l1 = v1 - v0;
        for (y = yMin; y < yMax; y++) {
            pPixelPos = data.m_pixelPos + y * w;
            for (x = 0; x <= y; x++, pPixelPos++) {
                dx = l0;
                dy = l1;
                dx *= data.nOffset[x];
                dy *= data.nOffset[y];
                *pPixelPos = v0;
                *pPixelPos += dx;
                *pPixelPos += dy;
            }
        }
    } else if (data.m_nType != SIDE_IS_TRI_13) {
        CFixVector l0 = v2 - v1;
        CFixVector l1 = v1 - v0;
        CFixVector l2 = v3 - v0;
        CFixVector l3 = v2 - v3;
        for (y = yMin; y < yMax; y++) {
            for (x = 0; x < w; x++, pPixelPos++) {
                if (x <= y) {
                    dx = l0;
                    dy = l1;
                } else {
                    dx = l2;
                    dy = l3;
                }
                dx *= data.nOffset[x];
                dy *= data.nOffset[y];
                *pPixelPos = v0;
                *pPixelPos += dx;
                *pPixelPos += dy;
            }
        }
    } else {
        --h;
        CFixVector l0 = v3 - v0;
        CFixVector l1 = v1 - v0;
        CFixVector l2 = v1 - v2;
        CFixVector l3 = v3 - v2;
        for (y = yMin; y < yMax; y++) {
            for (x = 0; x < w; x++, pPixelPos++) {
                if (h - y >= x) {
                    dx = l0;
                    dx *= data.nOffset[x];
                    dy = l1;
                    dy *= data.nOffset[y];
                    *pPixelPos = v0;
                } else {
                    dx = l2;
                    dx *= data.nOffset[h - x];
                    dy = l3;
                    dy *= data.nOffset[h - y];
                    *pPixelPos = v2;
                }
                *pPixelPos += dx;
                *pPixelPos += dy;
            }
        }
    }

#if DBG
    if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
        BRP;
#endif
    bBlack = bWhite = true;
    for (y = yMin; y < yMax; y++) {

#if DBG
        if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
            BRP;
#endif
        int32_t h = nTriangles ? w : y + 1;
        pPixelPos = data.m_pixelPos + y * w;
        for (x = 0; x < h; x++, pPixelPos++) {

#if USE_OPENMP
#pragma omp critical(LightmapProgressUpdate)
#endif
            if (nThread == MainThreadId()) {
                if (m_progress.Timeout().Expired()) {
                    int32_t nKey = KeyInKey();
                    if (nKey == KEY_ESC)
                        m_bSuccess = 0;
                    if (nKey == KEY_ALTED + KEY_F4)
                        exit(0);
                    m_progress.Render(nThread);
                }
            }

            if (!m_bSuccess) {
                m_bActiveThreads[nThread] = false;
                return;
            }
#if DBG
            if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide))) {
                BRP;
                if ((x == 0) && (y == 0))
                    BRP;
                if ((x == 0) && (y == w - 1))
                    BRP;
                if ((x == w - 1) && (y == 0))
                    BRP;
                if ((x == w - 1) && (y == w - 1))
                    BRP;
                if (((x == 0) || (x == w - 1)) || ((y == 0) || (y == w - 1)))
                    BRP;
                if (x == 0)
                    BRP;
                if (x == w - 1)
                    BRP;
                if (y == 0)
                    BRP;
                if (y == w - 1)
                    BRP;
            }
            if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
                BRP;
            int32_t nLights = lightManager.SetNearestToPixel(
                nSegment,
                nSide,
                &data.m_vNormal,
                pPixelPos,
                pFace->m_info.fRads[1] / 10.0f,
                nThread);
            if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
                BRP;
            if (0 < nLights) {
#else
        if (0 < lightManager.SetNearestToPixel(
                    nSegment,
                    nSide,
                    &data.m_vNormal,
                    pPixelPos,
                    pFace->m_info.fRads[1] / 10.0f,
                    nThread)) {
#endif
                data.m_vcd.vertPos.Assign(*pPixelPos);
                color.SetZero();
                ComputeVertexColor(nSegment, nSide, -1, &color, &data.m_vcd, nThread);
#if DBG
                if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide))) {
                    BRP;
                    if ((x == 0) && (y == 0))
                        BRP;
                    if ((x == 0) && (y == w - 1))
                        BRP;
                    if ((x == w - 1) && (y == 0))
                        BRP;
                    if ((x == w - 1) && (y == w - 1))
                        BRP;
                    if (((x == 0) || (x == w - 1)) || ((y == 0) || (y == w - 1)))
                        BRP;
                    if (x == 0)
                        BRP;
                    if (x == w - 1)
                        BRP;
                    if (y == 0)
                        BRP;
                    if (y == w - 1)
                        BRP;
                }
#endif
                if ((color.Red() >= 1.0f / 255.0f) || (color.Green() >= 1.0f / 255.0f) ||
                    (color.Blue() >= 1.0f / 255.0f)) {
                    bBlack = false;
                    if (color.Red() >= 254.0f / 255.0f)
                        color.Red() = 1.0f;
                    else
                        bWhite = false;
                    if (color.Green() >= 254.0f / 255.0f)
                        color.Green() = 1.0f;
                    else
                        bWhite = false;
                    if (color.Blue() >= 254.0f / 255.0f)
                        color.Blue() = 1.0f;
                    else
                        bWhite = false;
                }
                pTexColor = data.m_texColor + x * w + y;
                pTexColor->Red() = (uint8_t)(255 * color.Red());
                pTexColor->Green() = (uint8_t)(255 * color.Green());
                pTexColor->Blue() = (uint8_t)(255 * color.Blue());
            }
        }
    }

    if (bBlack)
        data.m_nColor |= 1;
    else if (bWhite)
        data.m_nColor |= 2;
    else
        data.m_nColor |= 4;

    if (data.m_nColor == 1) {
        pFace->m_info.nLightmap = 0;
        data.nBlackLightmaps++;
    } else if (data.m_nColor == 2) {
        pFace->m_info.nLightmap = 1;
        data.nWhiteLightmaps++;
    } else {
        Blur(pFace, data);
#if USE_OPENMP
#pragma omp critical(BuildLightmap)
#endif
        {
            Copy(data.m_texColor, m_list.m_nLightmaps);
            pFace->m_info.nLightmap = m_list.m_nLightmaps++;
        }
    }
#if DBG
    if ((nSegment == nDbgSeg) && ((nDbgSide < 0) || (nSide == nDbgSide)))
        BRP;
#endif
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::GetFace(void) {
    int32_t nFace;
#if USE_OPENMP
#pragma omp critical(GetFace)
#endif
    { nFace = (m_nFaces > 0) ? FACES.nFaces - m_nFaces-- : -1; }
    return nFace;
}

//------------------------------------------------------------------------------

void CLightmapManager::BuildThread(int32_t nThread) {
    m_bActiveThreads[nThread] = true;
    while (m_bSuccess) {
        int32_t nFace = GetFace();
        if (nFace < 0)
            break;
        m_progress.Update(nThread);
        Build(FACES.faces + nFace, nThread);
    }
    m_bActiveThreads[nThread] = false;
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::BuildAll(int32_t nFace) {
    CreateSpecial(m_data[0].m_texColor, 0, 0);
    CreateSpecial(m_data[0].m_texColor, 1, 255);
    m_list.m_nLightmaps = 2;
#if USE_OPENMP
    if (gameStates.app.nThreads > 1) {
        m_nFaces = FACES.nFaces;
        memset(m_bActiveThreads, 0, sizeof(m_bActiveThreads));
#pragma omp parallel for
        for (int32_t nThread = 0; nThread < gameStates.app.nThreads; nThread++) {
            BuildThread(nThread);
        }
    } else
#endif
    {
        uniDacsRouter[0].Create(gameData.segData.nSegments);
        m_bActiveThreads[0] = true;
        for (int32_t nFace = 0; nFace < FACES.nFaces; nFace++) {
            if (m_bSuccess) {
                m_progress.Update();
                Build(FACES.faces + nFace, 0);
            }
        }
        m_bActiveThreads[0] = false;
    }
    return m_bSuccess;
}

//------------------------------------------------------------------------------

static int32_t nFace = 0;

static int32_t CreatePoll(CMenu &menu, int32_t &key, int32_t nCurItem, int32_t nState) {
    if (nState)
        return nCurItem;

    lightmapManager.SetupProgress();
    lightmapManager.BuildAll(-1);
    lightmapManager.ResetProgress();
    key = -2;
    return nCurItem;
}

//------------------------------------------------------------------------------

char *CLightmapManager::Filename(char *pszFilename, int32_t nLevel) {
    return GameDataFilename(
        pszFilename,
        "lmap",
        nLevel,
        (gameOpts->render.nLightmapQuality + 1) * (gameOpts->render.nLightmapPrecision + 1) - 1);
}

//------------------------------------------------------------------------------

void CLightmapManager::Realloc(int32_t nBuffers) { m_list.Realloc(nBuffers); }

//------------------------------------------------------------------------------

void CLightmapManager::ToGrayScale(void) { m_list.ToGrayScaleAll(); }

//------------------------------------------------------------------------------

void CLightmapManager::Posterize(void) { m_list.PosterizeAll(); }

//------------------------------------------------------------------------------

int32_t CLightmapManager::Save(int32_t nLevel) {
    CFile cf;
    tLightmapDataHeader ldh = {
        LIGHTMAP_DATA_VERSION,
        CalcSegmentCheckSum(),
        gameData.segData.nSegments,
        gameData.segData.nVertices,
        FACES.nFaces,
        m_list.m_nLights,
        MAX_LIGHT_RANGE,
        m_list.m_nBuffers,
        gameStates.app.bCompressData};
    int32_t i, bOk;
    char szFilename[FILENAME_LEN];
    CSegFace *pFace;

    if (!(gameStates.app.bCacheLightmaps && m_list.m_nLights && m_list.m_nBuffers))
        return 0;
    if (!cf.Open(Filename(szFilename, nLevel), gameFolders.var.szLightmaps, "wb", 0))
        return 0;
    bOk = (cf.Write(&ldh, sizeof(ldh), 1) == 1);
    if (bOk) {
        for (i = FACES.nFaces, pFace = FACES.faces.Buffer(); i; i--, pFace++) {
            bOk = cf.Write(&pFace->m_info.nLightmap, sizeof(pFace->m_info.nLightmap), 1) == 1;
            if (!bOk)
                break;
        }
    }
    if (bOk)
        bOk = m_list.WriteAll(cf, ldh.bCompressed);
    cf.Close();
    return bOk;
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::Load(int32_t nLevel) {
    CFile cf;
    tLightmapDataHeader ldh;
    int32_t i, bOk;
    char szFilename[FILENAME_LEN];
    CSegFace *pFace;

    if (!gameStates.app.bCacheLightmaps) {
        PrintLog(0, "lightmap caching is disabled\n");
        return 0;
    }
    if ((gameOpts->app.bExpertMode || gameStates.app.bPrecomputeLightmaps) && gameStates.app.bRebuildLightmaps) {
        PrintLog(0, "Rebuilding lightmap due to user request\n");
        return 0;
    }
    if (!cf.Open(Filename(szFilename, nLevel), gameFolders.var.szLightmaps, "rb", 0) &&
        !cf.Open(Filename(szFilename, nLevel), gameFolders.var.szCache, "rb", 0)) {
        PrintLog(0, "Couldn't read lightmap file '%s'\n", Filename(szFilename, nLevel));
        if (*gameFolders.var.szLightmaps)
            PrintLog(0, "   Checked folder '%s'\n", gameFolders.var.szLightmaps);
        if (*gameFolders.var.szCache)
            PrintLog(0, "   Checked folder '%s'\n", gameFolders.var.szCache);
        return 0;
    }
    bOk = (cf.Read(&ldh, sizeof(ldh), 1) == 1);
    if (!bOk)
        PrintLog(0, "Error reading lightmap header data\n");
    else {
        bOk = false;
        if (ldh.nVersion != LIGHTMAP_DATA_VERSION)
            PrintLog(0, "lightmap data outdated (version)\n");
        else if (ldh.nCheckSum != CalcSegmentCheckSum())
            PrintLog(0, "lightmap data outdated (level data)\n");
        else if (ldh.nSegments != gameData.segData.nSegments)
            PrintLog(0, "lightmap data outdated (segment count)\n");
        else if (ldh.nVertices != gameData.segData.nVertices)
            PrintLog(0, "lightmap data outdated (vertex count)\n");
        else if (ldh.nFaces != FACES.nFaces)
            PrintLog(0, "lightmap data outdated (face count)\n");
        else if (ldh.nLights != m_list.m_nLights)
            PrintLog(0, "lightmap data outdated (light count)\n");
        else if (ldh.nBuffers > m_list.m_nBuffers)
            PrintLog(0, "lightmap data outdated (buffer count)\n");
        else if (ldh.nMaxLightRange != MAX_LIGHT_RANGE)
            PrintLog(0, "lightmap data outdated (light range)\n");
        else
            bOk = true;
    }
    if (bOk) {
        for (i = ldh.nFaces, pFace = FACES.faces.Buffer(); i; i--, pFace++) {
            bOk = cf.Read(&pFace->m_info.nLightmap, sizeof(pFace->m_info.nLightmap), 1) == 1;
            if (!bOk) {
                PrintLog(0, "error reading lightmap count\n");
                break;
            }
        }
    }
    if (bOk) {
        Realloc(ldh.nBuffers);
        if (bOk) {
            bOk = m_list.ReadAll(cf, ldh.bCompressed);
            if (!bOk)
                PrintLog(0, "error reading lightmap data\n");
        }
        cf.Close();
        if (gameOpts->render.color.nLevel < 2)
            ToGrayScale();
        if (gameStates.render.CartoonStyle())
            Posterize();
    }
    return bOk;
}

//------------------------------------------------------------------------------

void CLightmapManager::Init(void) {
    m_list.m_nBuffers = m_list.m_nLights = 0;
    m_list.m_nLightmaps = 0;
    memset(&m_data, 0, sizeof(m_data));
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::Create(int32_t nLevel) {
    if (!gameStates.render.bUsePerPixelLighting)
        return 0;
    if ((gameStates.render.bUsePerPixelLighting == 1) && !CreateLightmapShader(0))
        return gameStates.render.bUsePerPixelLighting = 0;
#if !DBG
    if (gameOpts->render.nLightmapQuality > 3)
        gameOpts->render.nLightmapQuality = 3;
    if (gameOpts->render.nLightmapPrecision > 2)
        gameOpts->render.nLightmapPrecision = 2;
#endif
    Destroy();
    int32_t nLights = Init(0);
    if (nLights < 0)
        return 0;

    SetupProgress();

    if (Load(nLevel)) {
        m_progress.Skip(1);
        return 1;
    }

    m_bSuccess = 1;

    if ((gameStates.render.bPerPixelLighting || gameStates.app.bPrecomputeLightmaps) && FACES.nFaces) {
        if (nLights) {
            lightManager.Transform(1, 0);
            int32_t nSaturation = gameOpts->render.color.nSaturation;
            gameOpts->render.color.nSaturation = 1;
            gameStates.render.bHaveLightmaps = 1;
            // gameData.renderData.fAttScale [0] = 2.0f;
            lightManager.Index(0, 0).nFirst = MAX_OGL_LIGHTS;
            lightManager.Index(0, 0).nLast = 0;
            gameStates.render.bSpecularColor = 0;
            gameStates.render.nState = 0;
            float h = 1.0f / float(LM_W - 1);
            for (int32_t n = 0; n < MAX_THREADS; n++) {
                for (int32_t i = 0; i < LM_W; i++)
                    m_data[n].nOffset[i] = i * h;
                m_data[n].nBlackLightmaps = m_data[n].nWhiteLightmaps = 0;
            }
            // PLANE_DIST_TOLERANCE = fix (I2X (1) * 0.001f);
            // SetupSegments (); // set all faces up as triangles
            gameStates.render.bBuildLightmaps = 1;
            if (!gameStates.app.bPrecomputeLightmaps && gameStates.app.bProgressBars && gameOpts->menus.nStyle) {
                nFace = 0;
                ResetProgress();
                SetupProgress();
                ProgressBar(TXT_CALC_LIGHTMAPS, 1, 0, /*PROGRESS_STEPS*/ (FACES.nFaces), CreatePoll);
            } else {
                if (!gameStates.app.bPrecomputeLightmaps)
                    messageBox.Show(TXT_CALC_LIGHTMAPS);
                GrabMouse(0, 0);
                m_bSuccess = BuildAll(-1);
                GrabMouse(1, 0);
                if (!gameStates.app.bPrecomputeLightmaps)
                    messageBox.Clear();
            }
            if (!m_bSuccess)
                return 0;
            gameStates.render.bBuildLightmaps = 0;
            // PLANE_DIST_TOLERANCE = DEFAULT_PLANE_DIST_TOLERANCE;
            // SetupSegments (); // standard face setup (triangles or quads)
            // gameData.renderData.fAttScale [0] = (gameStates.render.bPerPixelLighting == 2) ? 1.0f : 2.0f;
            gameStates.render.bSpecularColor = 1;
            gameStates.render.bHaveLightmaps = 0;
            gameStates.render.nState = 0;
            gameOpts->render.color.nSaturation = nSaturation;
            Realloc((m_list.m_nLightmaps + LIGHTMAP_BUFSIZE - 1) / LIGHTMAP_BUFSIZE);
        } else {
            CreateSpecial(m_data[0].m_texColor, 0, 0);
            m_list.m_nLightmaps = 1;
            for (int32_t i = 0; i < FACES.nFaces; i++)
                FACES.faces[i].m_info.nLightmap = 0;
        }
        BindAll();
        Save(nLevel);
        if (gameOpts->render.color.nLevel < 2)
            ToGrayScale();
    }
    return m_bSuccess;
}

//------------------------------------------------------------------------------

int32_t CLightmapManager::Setup(int32_t nLevel) {
    if (gameStates.render.bPerPixelLighting || gameStates.app.bPrecomputeLightmaps) {
        if (!Create(nLevel))
            return 0;
        if (HaveLightmaps())
            meshBuilder.RebuildLightmapTexCoord(); // rebuild to create proper lightmap texture coordinates
        else
            gameOpts->render.bUseLightmaps = 0;
    }
    return 1;
}

//------------------------------------------------------------------------------

int32_t SetupLightmap(CSegFace *pFace) {
#if DBG
    if ((pFace->m_info.nSegment == nDbgSeg) && ((nDbgSide < 0) || (pFace->m_info.nSide == nDbgSide)))
        BRP;
#endif
    int32_t i = pFace->m_info.nLightmap / LIGHTMAP_BUFSIZE;
    if (!lightmapManager.Bind(i))
        return 0;
    GLuint h = lightmapManager.Buffer(i)->m_handle;

    if (0 <= ogl.IsBound(h))
        return 1;

    ogl.SelectTMU(GL_TEXTURE0, true);
    ogl.SetTexturing(true);
    // glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    ogl.BindTexture(h);
    gameData.renderData.nStateChanges++;
    return 1;
}

//------------------------------------------------------------------------------
