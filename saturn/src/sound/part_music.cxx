#include "part_music.h"
#include "opening.h"
#include "parts.h"

struct PartMusic
{
    unsigned short part;
    unsigned short cue;
    int            loops;
};

static const PartMusic s_parts[] = {
#if !OPENING_USE_MODULE
    { GAME_PART2,  25, 0 },
#endif
    { GAME_PART3,   2, 1 },
    { GAME_PART4,   5, 1 },
    { GAME_PART5,  13, 1 },
    { GAME_PART6,  21, 1 },
    { GAME_PART7,  19, 1 },
    { GAME_PART8,  24, 1 }
};

static const int s_partCount = (int)(sizeof(s_parts) / sizeof(s_parts[0]));

static const PartMusic *partMusicFind(unsigned short partId)
{
    for (int i = 0; i < s_partCount; i++)
    {
        if (s_parts[i].part == partId)
        {
            return &s_parts[i];
        }
    }

    return 0;
}

unsigned short partMusicCue(unsigned short partId)
{
    const PartMusic *row = partMusicFind(partId);

    return (row != 0) ? row->cue : 0;
}

int partMusicLoops(unsigned short partId)
{
    const PartMusic *row = partMusicFind(partId);

    return (row != 0) ? row->loops : 0;
}

struct ModuleCue
{
    unsigned short part;
    unsigned short resNum;
    unsigned short cue;
    int            loops;
};

static const ModuleCue s_modules[] = {
#if !OPENING_USE_MODULE
    { GAME_PART2, 0x07, 25, 0 },
#endif
    { GAME_PART8, 0x8A, 24, 1 }
};

static const int s_moduleCount =
    (int)(sizeof(s_modules) / sizeof(s_modules[0]));

unsigned short partMusicModuleCue(unsigned short partId, unsigned short resNum,
                                  int *loops)
{
    for (int i = 0; i < s_moduleCount; i++)
    {
        if (s_modules[i].part == partId && s_modules[i].resNum == resNum)
        {
            if (loops != 0)
            {
                *loops = s_modules[i].loops;
            }
            return s_modules[i].cue;
        }
    }

    if (loops != 0)
    {
        *loops = 0;
    }

    return 0;
}

struct ResumeSkip
{
    unsigned short cue;
    unsigned int   skipMs;
};

static const ResumeSkip s_resumeSkips[] = {
    { 2, 4000u }
};

static const int s_resumeSkipCount =
    (int)(sizeof(s_resumeSkips) / sizeof(s_resumeSkips[0]));

unsigned int partMusicResumeSkipMs(unsigned short cue)
{
    for (int i = 0; i < s_resumeSkipCount; i++)
    {
        if (s_resumeSkips[i].cue == cue)
        {
            return s_resumeSkips[i].skipMs;
        }
    }

    return 0u;
}

int partMusicCovers(unsigned short partId)
{
    const int index = (int)partId - (int)GAME_PART_FIRST;

    if (index < 0 || index >= GAME_NUM_PARTS)
    {
        return 0;
    }

    return (memListParts[index][MEMLIST_PART_VIDEO2] == MEMLIST_PART_NONE)
        ? 1 : 0;
}
