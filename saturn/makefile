SRL_MAX_TEXTURES = 100
SRL_MODE = NTSC
SRL_HIGH_RES = 0
SRL_FRAMERATE = 1
SRL_MAX_CD_BACKGROUND_JOBS = 3
SRL_MAX_CD_FILES = 256
SRL_MAX_CD_RETRIES = 5

SRL_USE_SGL_SOUND_DRIVER = 1
SRL_ENABLE_FREQ_ANALYSIS = 1

SGL_MAX_VERTICES = 2500
SGL_MAX_POLYGONS = 1500
SGL_MAX_EVENTS = 1
SGL_MAX_WORKS = 1

CD_NAME = Out of this World (USA)

BUILD_DROP = ./BuildDrop

SRL_INSTALL_ROOT ?= ../SaturnRingLib

SOURCES  = $(patsubst ./%,%,$(shell find src/ -name '*.c'))
SOURCES += $(patsubst ./%,%,$(shell find src/ -name '*.cxx'))

SRL_CUSTOM_CCFLAGS += -DBYPASS_PROTECTION -DOOTW_SATURN

SRL_CUSTOM_CCFLAGS += $(patsubst %,-I%,$(shell find src -type d))

SDK_ROOT = $(SRL_INSTALL_ROOT)/saturnringlib
include $(SDK_ROOT)/shared.mk

LIBS += $(SGLLDIR)/LIBPCM.A
