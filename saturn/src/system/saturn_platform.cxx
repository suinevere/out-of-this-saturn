#include <srl.hpp>
#include <sega_sys.h>
#include "saturn_platform.h"
#include "saturn_audio.h"
#include "cd_music.h"

using namespace SRL::Types;

#define SCREEN_W     320
#define SCREEN_H     200
#define PAGE_PITCH   (SCREEN_W / 2)
#define VRAM_PITCH   (512 / 2)
#define SCREEN_TOP   12

static HighColor g_paletteColors[16];
static SRL::Bitmap::Palette g_palette(g_paletteColors, 16);

static uint8_t g_paletteRaw[32];

static bool g_paletteDirty = false;

static uint8_t *g_vram = nullptr;

static volatile uint32_t g_frames = 0;

class AnotherWorldCanvas : public SRL::Bitmap::IBitmap
{
private:
    uint8_t *data;

public:
    AnotherWorldCanvas(uint8_t *blank) : data(blank) { }

    uint8_t *GetData() override
    {
        return this->data;
    }

    SRL::Bitmap::BitmapInfo GetInfo() const override
    {
        return SRL::Bitmap::BitmapInfo(SCREEN_W, SCREEN_H, (SRL::Bitmap::Palette *)&g_palette);
    }
};

static void onVblank()
{
    g_frames++;
}

#define SAT_SMPC_COMREG (*(volatile uint8_t *)0x2010001Fu)
#define SAT_SMPC_SF     (*(volatile uint8_t *)0x20100063u)
#define SAT_SMPC_SSHOFF 0x03u
#define SAT_SMPC_SYSRES 0x0Au
#define SAT_SMPC_TRIES  100000u

#define SAT_UINT_FIRST 0x40u
#define SAT_UINT_LAST  0x5fu

#define SAT_FRT_TIER      (*(volatile uint8_t *)0xFFFFFE10u)
#define SAT_IPRB          (*(volatile uint16_t *)0xFFFFFE60u)
#define SAT_IPRB_FRT_MASK 0xF0FFu

#define SAT_SCU_DSP_CTRL (*(volatile uint32_t *)0x25FE0080u)
#define SAT_DMAC_CHCR0   0xFFFFFF8Cu
#define SAT_DMAC_STRIDE  0x10u
#define SAT_DMAC_DRCR0   0xFFFFFE71u
#define SAT_DMAC_DMAOR   (*(volatile uint32_t *)0xFFFFFFB0u)
#define SAT_DIVU_CONT    (*(volatile uint32_t *)0xFFFFFFB8u)

static void sat_boot_sanitize(void)
{
    uint32_t spin;
    uint32_t i;

    SAT_FRT_TIER = 0u;
    SAT_IPRB = (uint16_t)(SAT_IPRB & SAT_IPRB_FRT_MASK);

    for (spin = 0; spin < SAT_SMPC_TRIES && (SAT_SMPC_SF & 1u) != 0u; spin++)
    {
    }

    if ((SAT_SMPC_SF & 1u) == 0u)
    {
        SAT_SMPC_SF = 1u;
        SAT_SMPC_COMREG = SAT_SMPC_SSHOFF;

        for (spin = 0; spin < SAT_SMPC_TRIES && (SAT_SMPC_SF & 1u) != 0u; spin++)
        {
        }
    }

    SAT_SCU_DSP_CTRL = 0u;

    for (i = 0u; i < 2u; i++)
    {
        volatile uint32_t *chcr =
            (volatile uint32_t *)(SAT_DMAC_CHCR0 + i * SAT_DMAC_STRIDE);

        (void)*chcr;
        *chcr = 0u;
        *(volatile uint8_t *)(SAT_DMAC_DRCR0 + i) = 0u;
    }

    (void)SAT_DMAC_DMAOR;
    SAT_DMAC_DMAOR = 0u;
    SAT_DIVU_CONT = 0u;

    for (i = SAT_UINT_FIRST; i <= SAT_UINT_LAST; i++)
    {
        SYS_SETUINT(i, 0);
    }

    SYS_SETSCUIM(0xFFFFFFFFu);
}

extern "C" void sat_boot_init(void)
{
    sat_boot_sanitize();

    SRL::Core::Initialize(HighColor(0, 0, 0));
    SRL::Core::OnVblank += onVblank;
}

extern "C" void sat_video_init(void)
{
    uint8_t *blank = (uint8_t *)SRL::Memory::LowWorkRam::Malloc(PAGE_PITCH * SCREEN_H);

    if (blank == nullptr)
    {
        SRL::Debug::Print(1, 1, "video: no room for setup page");
        return;
    }

    for (int32_t i = 0; i < PAGE_PITCH * SCREEN_H; i++)
    {
        blank[i] = 0;
    }

    for (int32_t i = 0; i < 16; i++)
    {
        g_paletteColors[i] = HighColor(0, 0, 0);
    }

    AnotherWorldCanvas canvas(blank);
    SRL::VDP2::NBG0::LoadBitmap((SRL::Bitmap::IBitmap *)&canvas);

    SRL::Debug::PrintClearLine(20);
    SRL::Debug::PrintClearLine(21);

    g_vram = (uint8_t *)SRL::VDP2::NBG0::GetCellAddress();

    SRL::VDP2::NBG0::SetPriority(SRL::VDP2::Priority::Layer2);
    SRL::Math::Types::Vector2D origin(0.0, -12.0);
    SRL::VDP2::NBG0::SetPosition(origin);
    SRL::VDP2::NBG0::ScrollEnable();

    SRL::Memory::LowWorkRam::Free(blank);
}

extern "C" void sat_video_set_palette(const uint8_t *colors)
{
    if (colors == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < 32; i++)
    {
        g_paletteRaw[i] = colors[i];
    }

    for (int32_t i = 0; i < 16; i++)
    {
        const uint8_t c1 = colors[i * 2 + 0];
        const uint8_t c2 = colors[i * 2 + 1];
        const uint8_t r = (uint8_t)(c1 & 0x0F);
        const uint8_t g = (uint8_t)((c2 & 0xF0) >> 4);
        const uint8_t b = (uint8_t)(c2 & 0x0F);

        g_paletteColors[i] = HighColor::FromRGB555((uint8_t)((r << 1) | (r >> 3)),
                                                   (uint8_t)((g << 1) | (g >> 3)),
                                                   (uint8_t)((b << 1) | (b >> 3)));
    }

    g_paletteDirty = true;
}

static void sat_video_flush_palette(void)
{
    if (!g_paletteDirty)
    {
        return;
    }

    if (SRL::VDP2::NBG0::TilePalette.GetData() != nullptr)
    {
        SRL::VDP2::NBG0::TilePalette.Load(g_paletteColors, 16);
    }

    g_paletteDirty = false;
}

extern "C" void display_get_palette(uint8_t *out)
{
    if (out == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < 32; i++)
    {
        out[i] = g_paletteRaw[i];
    }
}

extern "C" void sat_video_present(const uint8_t *page)
{
    timers_pump();

    SRL::Core::Synchronize();

    sat_video_flush_palette();

    if (page != nullptr && g_vram != nullptr)
    {
        const uint8_t *src = page;
        uint8_t *dst = g_vram;

        for (int32_t line = 0; line < SCREEN_H; line++)
        {
            slDMACopy((void *)src, (void *)dst, PAGE_PITCH);
            src += PAGE_PITCH;
            dst += VRAM_PITCH;
        }

        slDMAWait();
    }
}

extern "C" void sat_video_sync(void)
{
    timers_pump();
    SRL::Core::Synchronize();
    sat_video_flush_palette();
}

static uint32_t g_padLatch = 0;

extern "C" void sat_loading_tick(void)
{
    timers_pump();
    g_padLatch |= sat_input_level();
}

extern "C" uint32_t sat_input_level(void)
{
    uint32_t bits = 0;
    SRL::Input::Digital port0(0);

    if (port0.IsConnected())
    {
        if (port0.IsHeld(SRL::Input::Digital::Button::Up))    bits |= PAD_BIT_UP;
        if (port0.IsHeld(SRL::Input::Digital::Button::Down))  bits |= PAD_BIT_DOWN;
        if (port0.IsHeld(SRL::Input::Digital::Button::Left))  bits |= PAD_BIT_LEFT;
        if (port0.IsHeld(SRL::Input::Digital::Button::Right)) bits |= PAD_BIT_RIGHT;
        if (port0.IsHeld(SRL::Input::Digital::Button::A)) bits |= PAD_BIT_A;
        if (port0.IsHeld(SRL::Input::Digital::Button::B)) bits |= PAD_BIT_B;
        if (port0.IsHeld(SRL::Input::Digital::Button::C)) bits |= PAD_BIT_C;
        if (port0.IsHeld(SRL::Input::Digital::Button::X)) bits |= PAD_BIT_X;
        if (port0.IsHeld(SRL::Input::Digital::Button::Y)) bits |= PAD_BIT_Y;
        if (port0.IsHeld(SRL::Input::Digital::Button::Z)) bits |= PAD_BIT_Z;
        if (port0.IsHeld(SRL::Input::Digital::Button::L)) bits |= PAD_BIT_L;
        if (port0.IsHeld(SRL::Input::Digital::Button::R)) bits |= PAD_BIT_R;
        if (port0.IsHeld(SRL::Input::Digital::Button::START)) bits |= PAD_BIT_PAUSE;
    }

    return bits;
}

extern "C" uint32_t sat_input_read(void)
{
    const uint32_t bits = sat_input_level() | g_padLatch;

    g_padLatch = 0;
    return bits;
}

extern "C" uint32_t clock_ms(void)
{
    return (g_frames * 50u) / 3u;
}

extern "C" void sat_sleep_ms(uint32_t ms)
{
    const uint32_t until = clock_ms() + ms;

    while (clock_ms() < until)
    {
        timers_pump();
        SRL::Core::Synchronize();
        g_padLatch |= sat_input_level();
    }
}

extern "C" void diag_row(int row, const char *fmt, int a, int b, int c, int d,
                         int e)
{

    SRL::Debug::Print(0, (uint8_t)row,
                      "                                        ");
    SRL::Debug::Print(0, (uint8_t)row, fmt, a, b, c, d, e);
}

extern "C" void boot_bios(void)
{
    SYS_EXECDMP();

    for (;;)
    {
        SRL::Core::Synchronize();
    }
}

#define RESET_WAIT_FRAMES 120

extern "C" int sat_system_reset(void)
{
    uint32_t spin;

    cd_music_stop();

    for (spin = 0; spin < SAT_SMPC_TRIES && (SAT_SMPC_SF & 1u) != 0u; spin++)
    {
    }

    if ((SAT_SMPC_SF & 1u) == 0u)
    {
        SAT_SMPC_SF = 1u;
        SAT_SMPC_COMREG = SAT_SMPC_SYSRES;
    }

    for (int frame = 0; frame < RESET_WAIT_FRAMES; frame++)
    {
        SRL::Core::Synchronize();
    }

    SYS_EXECDMP();

    for (int frame = 0; frame < RESET_WAIT_FRAMES; frame++)
    {
        SRL::Core::Synchronize();
    }

    return 0;
}
