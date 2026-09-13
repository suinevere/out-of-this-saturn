#ifndef PART_MUSIC_H
#define PART_MUSIC_H

unsigned short partMusicCue(unsigned short partId);

int partMusicLoops(unsigned short partId);

unsigned short partMusicModuleCue(unsigned short partId, unsigned short resNum,
                                  int *loops);

int partMusicCovers(unsigned short partId);

unsigned int partMusicResumeSkipMs(unsigned short cue);

#endif
