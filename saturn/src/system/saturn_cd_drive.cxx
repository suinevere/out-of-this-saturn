#include <srl.hpp>
#include "cd_drive.h"

#if CD_DRIVE_FAD_ERR != CDC_FAD_ERR
#error CD_DRIVE_FAD_ERR must equal CDC_FAD_ERR
#endif

extern "C" {

void cd_drive_read_toc(uint32_t *toc)
{
    CDC_TgetToc(toc);
}

void cd_drive_start_analysis(void)
{
    SRL::Sound::Cdda::Analysis::Start();
}

void cd_drive_halt(void)
{
    CdcPos pos;

    CDC_POS_PTYPE(&pos) = CDC_PTYPE_DFL;
    CDC_CdSeek(&pos);
}

int cd_drive_status(uint32_t *fad)
{
    CdcStat stat;

    CDC_GetCurStat(&stat);
    *fad = (uint32_t)CDC_STAT_FAD(&stat);
    return (CDC_GET_STC(&stat) == CDC_ST_PLAY) ? 1 : 0;
}

void cd_drive_play_fad(uint32_t startFad, uint32_t count)
{
    CdcPly ply;

    CDC_PLY_STYPE(&ply) = CDC_PTYPE_FAD;
    CDC_PLY_SFAD(&ply) = startFad;
    CDC_PLY_ETYPE(&ply) = CDC_PTYPE_FAD;
    CDC_PLY_EFAS(&ply) = count;
    CDC_PLY_PMODE(&ply) = CDC_PM_DFL;
    CDC_CdPlay(&ply);
}

void cd_drive_play_track(uint16_t track, int loop)
{
    SRL::Sound::Cdda::PlaySingle(track, loop != 0);
}

uint32_t cd_drive_volume(void)
{
    SRL::Sound::Cdda::Analysis::TotalVolume probe =
        SRL::Sound::Cdda::Analysis::GetTotalVolume();

    return (uint32_t)(probe.LeftChannel | probe.RightChannel);
}

}
