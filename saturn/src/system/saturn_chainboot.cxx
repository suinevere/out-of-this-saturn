extern "C" {
#include "boot.h"
#include "disc.h"
#include "cd_music.h"
#include "saturn_scsp.h"
#include "saturn_audio.h"
#include "saturn_fade.h"
}

#include <srl.hpp>
#include <srl_scu.hpp>
#include <sgl.h>
#include <sega_gfs.h>
#include <sega_sys.h>

#define CHAINBOOT_IMAGE  "0.BIN"
#define CHAINBOOT_MARKER "ANOTHER.BIN"

#define CHAINBOOT_ENTRY 0x06004000u

#define CHAINBOOT_UNCACHED 0x20000000u

#define CHAINBOOT_MAX_BYTES 0x000fc000u

#define SMPC_COMREG (*(volatile uint8_t *)0x2010001Fu)
#define SMPC_SF     (*(volatile uint8_t *)0x20100063u)
#define SMPC_SSHOFF 0x03u
#define SMPC_TRIES  100000u

#define CHAINBOOT_UINT_FIRST 0x40u
#define CHAINBOOT_UINT_LAST  0x5fu

static const unsigned short g_trampoline[7] = {
	0x6046, 0x2502, 0x7504, 0x4610, 0x8bfa, 0x472b, 0x0009
};

typedef void (*chainboot_fn)(const void *src, void *dst,
                             unsigned long longwords, void *entry);

static int chainboot_exists(const char *name)
{
	DiscFile *file = disc_open(name);

	if (file == nullptr)
	{
		return 0;
	}

	disc_close(file);
	return 1;
}

static int chainboot_slave_off(void)
{
	unsigned int spin;

	for (spin = 0; spin < SMPC_TRIES && (SMPC_SF & 1u) != 0u; spin++)
	{
	}

	if ((SMPC_SF & 1u) != 0u)
	{
		return 0;
	}

	SMPC_SF = 1u;
	SMPC_COMREG = SMPC_SSHOFF;

	for (spin = 0; spin < SMPC_TRIES && (SMPC_SF & 1u) != 0u; spin++)
	{
	}

	return (SMPC_SF & 1u) == 0u;
}

extern "C" int chainboot_available(void)
{
	return chainboot_exists(CHAINBOOT_MARKER) && chainboot_exists(CHAINBOOT_IMAGE);
}

extern "C" void chainboot_run(void)
{
	if (!chainboot_exists(CHAINBOOT_MARKER))
	{
		return;
	}

	DiscFile *image = disc_open(CHAINBOOT_IMAGE);

	if (image == nullptr)
	{
		return;
	}

	const int32_t bytes = disc_size(image);

	if (bytes <= 0 || (unsigned long)bytes > CHAINBOOT_MAX_BYTES)
	{
		disc_close(image);
		return;
	}

	const unsigned long longwords = ((unsigned long)bytes + 3ul) / 4ul;

	void *staged = SRL::Memory::LowWorkRam::Malloc((size_t)bytes);

	if (staged == nullptr)
	{
		disc_close(image);
		return;
	}

	void *tramp = SRL::Memory::LowWorkRam::Malloc(sizeof(g_trampoline));

	if (tramp == nullptr)
	{
		SRL::Memory::LowWorkRam::Free(staged);
		disc_close(image);
		return;
	}

	const int32_t loaded = disc_read(image, 0, staged, bytes);

	disc_close(image);

	if (loaded != bytes)
	{
		SRL::Memory::LowWorkRam::Free(tramp);
		SRL::Memory::LowWorkRam::Free(staged);
		return;
	}

	for (unsigned int i = 0; i < sizeof(g_trampoline) / sizeof(g_trampoline[0]); i++)
	{
		((unsigned short *)tramp)[i] = g_trampoline[i];
	}

	if (!chainboot_slave_off())
	{
		SRL::Memory::LowWorkRam::Free(tramp);
		SRL::Memory::LowWorkRam::Free(staged);
		return;
	}

	fade_set(FADE_DARK);
	cd_music_stop();

	GFS_Reset();
	sat_audio_stop();
	voice_stop_all();
	slSoundOffWait();

	for (unsigned int vector = CHAINBOOT_UINT_FIRST;
	     vector <= CHAINBOOT_UINT_LAST; vector++)
	{
		SYS_SETUINT(vector, 0);
		SYS_SETSINT(vector, 0);
	}

	SYS_SETSCUIM(0xffffffffu);
	__asm__ __volatile__("ldc %0, sr" :: "r"(0x000000f0u) : "memory");

	chainboot_fn go = (chainboot_fn)((unsigned long)tramp | CHAINBOOT_UNCACHED);

	*reinterpret_cast<volatile uint16_t *>(
	    SRL::SCU::DSP::RegisterMap::CacheControlRegister) |=
	    SRL::SCU::DSP::CachePurgeBit;

	go((const void *)((unsigned long)staged | CHAINBOOT_UNCACHED),
	   (void *)(CHAINBOOT_ENTRY | CHAINBOOT_UNCACHED),
	   longwords,
	   (void *)CHAINBOOT_ENTRY);
}
