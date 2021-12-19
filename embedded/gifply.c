#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "tusb.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include <stdio.h>

#include <math.h>
#include "st7789_lcd.pio.h"
#include "gifdec.h"

#define MIN(A, B) ((A) < (B) ? (A) : (B))

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define IMAGE_SIZE 256
#define LOG_IMAGE_SIZE 8

#define PIN_BUTTON 1
#define PIN_D0 8
#define PIN_CS 6
#define PIN_DC 5
#define PIN_RESET 7
#define PIN_WR 4
#define PIN_RD 3

#define ILI9341_TFTWIDTH 240  ///< ILI9341 max TFT width
#define ILI9341_TFTHEIGHT 320 ///< ILI9341 max TFT height

#define ILI9341_NOP 0x00     ///< No-op register
#define ILI9341_SWRESET 0x01 ///< Software reset register
#define ILI9341_RDDID 0x04   ///< Read display identification information
#define ILI9341_RDDST 0x09   ///< Read Display Status

#define ILI9341_SLPIN 0x10  ///< Enter Sleep Mode
#define ILI9341_SLPOUT 0x11 ///< Sleep Out
#define ILI9341_PTLON 0x12  ///< Partial Mode ON
#define ILI9341_NORON 0x13  ///< Normal Display Mode ON

#define ILI9341_RDMODE 0x0A     ///< Read Display Power Mode
#define ILI9341_RDMADCTL 0x0B   ///< Read Display MADCTL
#define ILI9341_RDPIXFMT 0x0C   ///< Read Display Pixel Format
#define ILI9341_RDIMGFMT 0x0D   ///< Read Display Image Format
#define ILI9341_RDSELFDIAG 0x0F ///< Read Display Self-Diagnostic Result

#define ILI9341_INVOFF 0x20   ///< Display Inversion OFF
#define ILI9341_INVON 0x21    ///< Display Inversion ON
#define ILI9341_GAMMASET 0x26 ///< Gamma Set
#define ILI9341_DISPOFF 0x28  ///< Display OFF
#define ILI9341_DISPON 0x29   ///< Display ON

#define ILI9341_CASET 0x2A ///< Column Address Set
#define ILI9341_PASET 0x2B ///< Page Address Set
#define ILI9341_RAMWR 0x2C ///< Memory Write
#define ILI9341_RAMRD 0x2E ///< Memory Read

#define ILI9341_PTLAR 0x30    ///< Partial Area
#define ILI9341_VSCRDEF 0x33  ///< Vertical Scrolling Definition
#define ILI9341_MADCTL 0x36   ///< Memory Access Control
#define ILI9341_VSCRSADD 0x37 ///< Vertical Scrolling Start Address
#define ILI9341_PIXFMT 0x3A   ///< COLMOD: Pixel Format Set

#define ILI9341_FRMCTR1 \
    0xB1                     ///< Frame Rate Control (In Normal Mode/Full Colors)
#define ILI9341_FRMCTR2 0xB2 ///< Frame Rate Control (In Idle Mode/8 colors)
#define ILI9341_FRMCTR3 \
    0xB3                     ///< Frame Rate control (In Partial Mode/Full Colors)
#define ILI9341_INVCTR 0xB4  ///< Display Inversion Control
#define ILI9341_DFUNCTR 0xB6 ///< Display Function Control

#define ILI9341_PWCTR1 0xC0 ///< Power Control 1
#define ILI9341_PWCTR2 0xC1 ///< Power Control 2
#define ILI9341_PWCTR3 0xC2 ///< Power Control 3
#define ILI9341_PWCTR4 0xC3 ///< Power Control 4
#define ILI9341_PWCTR5 0xC4 ///< Power Control 5
#define ILI9341_VMCTR1 0xC5 ///< VCOM Control 1
#define ILI9341_VMCTR2 0xC7 ///< VCOM Control 2

#define ILI9341_RDID1 0xDA ///< Read ID 1
#define ILI9341_RDID2 0xDB ///< Read ID 2
#define ILI9341_RDID3 0xDC ///< Read ID 3
#define ILI9341_RDID4 0xDD ///< Read ID 4

#define ILI9341_GMCTRP1 0xE0 ///< Positive Gamma Correction
#define ILI9341_GMCTRN1 0xE1 ///< Negative Gamma Correction

critical_section_t crit_sec;

enum current_usb_job_t
{
    USB_JOB_NONE = 0,
    USB_JOB_READ_1,
    USB_JOB_READ_2,
    USB_JOB_WRITE_1,
    USB_JOB_WRITE_2,
} current_usb_job = USB_JOB_NONE;

struct state_t
{
    bool gif_1_locked;
    bool gif_2_locked;
    bool gif_1_should_play;
    bool gif_1_playing;
} state = {
    .gif_1_locked = false,
    .gif_2_locked = false,
    .gif_1_should_play = true,
    .gif_1_playing = true};

#define GIF_1 (PICO_FLASH_SIZE_BYTES - 1024 * 1024)
#define GIF_2 (PICO_FLASH_SIZE_BYTES - 512 * 1024)
static int gif_length = -1;
void usb_task()
{
    tusb_init();
    while (true)
    {
        tud_task();

        if (current_usb_job == USB_JOB_READ_1 || current_usb_job == USB_JOB_READ_2)
        {
            static uint8_t *read_pos = NULL;
            int available = tud_vendor_write_available();
            if (available)
            {
                if (gif_length == -1)
                {
                    if (available < 4)
                        continue;
                    if (current_usb_job == USB_JOB_READ_1)
                    {
                        read_pos = (uint8_t *)(GIF_1 + XIP_BASE);
                    }
                    else
                    {
                        read_pos = (uint8_t *)(GIF_2 + XIP_BASE);
                    }

                    memcpy(&gif_length, read_pos, 4);
                    gif_length += 4;
                }

                int len = MIN(gif_length, available);
                int written = tud_vendor_write(read_pos, len);
                read_pos += written;
                gif_length -= written;
            }

            if (gif_length == 0)
            {
                current_usb_job = USB_JOB_NONE;
                gif_length = -1;
            }
        }
        else if (current_usb_job == USB_JOB_WRITE_1 || current_usb_job == USB_JOB_WRITE_2)
        {
            critical_section_enter_blocking(&crit_sec);
            if (current_usb_job == USB_JOB_WRITE_1 && state.gif_1_playing || current_usb_job == USB_JOB_WRITE_2 && !state.gif_1_playing)
            {
                critical_section_exit(&crit_sec);
                continue;
            }
            critical_section_exit(&crit_sec);

            int available = tud_vendor_available();
            static uint32_t write_pos = 0;
#define BUF_SIZE 256
            uint8_t buf[BUF_SIZE];
            multicore_lockout_start_blocking();

            while (available >= BUF_SIZE && (gif_length >= BUF_SIZE || gif_length < 0))
            {
                tud_vendor_read(buf, BUF_SIZE);
                if (gif_length == -1)
                {
                    memcpy(&gif_length, buf, 4);
                    gif_length += 4;
                    if (gif_length > 512 * 1024)
                    {
                        current_usb_job = USB_JOB_NONE;
                        gif_length = -1;
                        break;
                    }

                    if (current_usb_job == USB_JOB_WRITE_1)
                    {
                        write_pos = GIF_1;
                    }
                    else
                    {
                        write_pos = GIF_2;
                    }
                    uint32_t status = save_and_disable_interrupts();
                    flash_range_erase(write_pos, ((gif_length + 4095) / 4096) * 4096);
                    restore_interrupts(status);
                }
                uint32_t status = save_and_disable_interrupts();
                flash_range_program(write_pos, buf, BUF_SIZE);
                restore_interrupts(status);

                gif_length -= BUF_SIZE;
                available -= BUF_SIZE;
                write_pos += BUF_SIZE;
            }

            if (gif_length > 0 && gif_length < BUF_SIZE && gif_length <= available)
            {
                tud_vendor_read((uint8_t *)&buf, gif_length);
                memset(buf + gif_length, 0, BUF_SIZE - gif_length);
                gif_length = 0;
                flash_range_program(write_pos, buf, BUF_SIZE);
            }
            multicore_lockout_end_blocking();

            if (gif_length == 0)
            {
                state.gif_1_locked = false;
                state.gif_2_locked = false;
                current_usb_job = USB_JOB_NONE;
                gif_length = -1;
            }
        }
    }
}

#define REQ_READ_1 1
#define REQ_READ_2 2
#define REQ_WRITE_1 3
#define REQ_WRITE_2 4
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP)
        return true;

    switch (request->bmRequestType_bit.type)
    {
    case TUSB_REQ_TYPE_VENDOR:
        switch (request->bRequest)
        {
        case REQ_READ_1:
            current_usb_job = USB_JOB_READ_1;
            break;
        case REQ_READ_2:
            current_usb_job = USB_JOB_READ_2;
            break;
        case REQ_WRITE_1:
            critical_section_enter_blocking(&crit_sec);
            if (state.gif_2_locked)
            {
                critical_section_exit(&crit_sec);
                return false;
            }
            state.gif_1_locked = true;
            state.gif_1_should_play = false;
            critical_section_exit(&crit_sec);
            current_usb_job = USB_JOB_WRITE_1;
            break;
        case REQ_WRITE_2:
            if (state.gif_1_locked)
            {
                critical_section_exit(&crit_sec);
                return false;
            }
            state.gif_2_locked = true;
            state.gif_1_should_play = true;
            critical_section_exit(&crit_sec);
            current_usb_job = USB_JOB_WRITE_2;
            break;
        }
        gif_length = -1;
    }
    return tud_control_status(rhport, request);
}
static const uint8_t initcmd[] = {
    ILI9341_SWRESET, 0x80,
    ILI9341_SLPOUT, 0x80,
    ILI9341_PIXFMT, 1, 0x55,
    ILI9341_MADCTL, 1, 0x00,
    ILI9341_CASET, 4, 0, 0, 0, 240,
    ILI9341_PASET, 4, 0, 0, 1, 64,
    //ILI9341_INVON   , 0x80,
    ILI9341_NORON, 0x80,
    ILI9341_DISPON, 0x80, // Display on
    0x00                  // End of list
};

#define SERIAL_CLK_DIV 3.0f

static inline void lcd_set_dc_cs(bool dc, bool cs)
{
    sleep_us(1);
    gpio_put_masked((1u << PIN_DC) | (1u << PIN_CS), !!dc << PIN_DC | !!cs << PIN_CS);
    sleep_us(1);
}

static inline void lcd_write_cmd(PIO pio, uint sm, const uint8_t cmd, const uint8_t *args, size_t count)
{
    st7789_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(0, 0);
    st7789_lcd_put(pio, sm, cmd);
    if (count > 0)
    {
        st7789_lcd_wait_idle(pio, sm);
        lcd_set_dc_cs(1, 0);
        for (size_t i = 0; i < count; ++i)
            st7789_lcd_put(pio, sm, *args++);
    }
    st7789_lcd_wait_idle(pio, sm);
    lcd_set_dc_cs(1, 1);
}

static inline void lcd_init(PIO pio, uint sm, const uint8_t *initcmd)
{
    const uint8_t *cmd = initcmd;
    while (*cmd)
    {
        uint8_t count = cmd[1] & 0x7F;
        lcd_write_cmd(pio, sm, cmd[0], cmd + 2, count);
        if (cmd[1] & 0x80)
            sleep_ms(150);
        cmd += count + 2;
    }
}

static inline void st7789_start_pixels(PIO pio, uint sm)
{
    lcd_write_cmd(pio, sm, ILI9341_RAMWR, NULL, 0);
    lcd_set_dc_cs(1, 0);
}

void toggle_gif(uint gpio, uint32_t events)
{
    if (gpio != PIN_BUTTON)
        return;
    static uint64_t last_press_us;
    uint64_t time = time_us_64();
    if (last_press_us + 300 * 1000 > time)
        return;
    last_press_us = time;
    critical_section_enter_blocking(&crit_sec);
    if (!(state.gif_1_locked || state.gif_2_locked))
    {
        state.gif_1_should_play = !state.gif_1_should_play;
    }
    critical_section_exit(&crit_sec);
}

void lcd_task()
{
    multicore_lockout_victim_init();
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &st7789_lcd_program);
    st7789_lcd_program_init(pio, sm, offset, PIN_D0, PIN_WR, SERIAL_CLK_DIV);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RESET);
    gpio_init(PIN_RD);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_set_dir(PIN_RD, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_RD, 1);
    gpio_put(PIN_RESET, 1);

    lcd_init(pio, sm, initcmd);
    gd_GIF *gif;
    gif = gd_open_gif((uint8_t *)(GIF_1 + XIP_BASE) + 4);
    printf("%p", gif);
    while (1)
    {
        critical_section_enter_blocking(&crit_sec);
        if (state.gif_1_should_play != state.gif_1_playing)
        {
            uint8_t *gif_pos = (uint8_t *)((state.gif_1_should_play ? GIF_1 : GIF_2) + XIP_BASE) + 4;
            critical_section_exit(&crit_sec);
            state.gif_1_playing = state.gif_1_should_play;
            gd_close_gif(gif);
            gif = gd_open_gif(gif_pos);
        }
        else
        {
            critical_section_exit(&crit_sec);
        }
        if (!gif)
            continue;
        int ret = gd_get_frame(gif);
        if (ret == 0)
        {
            gd_rewind(gif);
            continue;
        }
        st7789_start_pixels(pio, sm);

        for (int y = 39; y >= 0; --y)
        {
            for (int x = SCREEN_WIDTH - 1; x >= 0; x--)
            {
                uint16_t color = gif->canvas[(y * SCREEN_HEIGHT + x)];
                st7789_lcd_put(pio, sm, color >> 8);
                st7789_lcd_put(pio, sm, color & 0xFF);
            }
        }
        for (int y = 0; y < SCREEN_HEIGHT; ++y)
        {
            for (int x = SCREEN_WIDTH - 1; x >= 0; x--)
            {
                uint16_t color = gif->canvas[(y * SCREEN_HEIGHT + x)];
                st7789_lcd_put(pio, sm, color >> 8);
                st7789_lcd_put(pio, sm, color & 0xFF);
            }
        }
        for (int y = SCREEN_HEIGHT - 1; y >= SCREEN_HEIGHT - 40; --y)
        {
            for (int x = SCREEN_WIDTH - 1; x >= 0; x--)
            {
                uint16_t color = gif->canvas[(y * SCREEN_HEIGHT + x)];
                st7789_lcd_put(pio, sm, color >> 8);
                st7789_lcd_put(pio, sm, color & 0xFF);
            }
        }
        //sleep_ms(gif->gce.delay * 5);
    }
}

int main()
{
    gpio_init(PIN_BUTTON);

    gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_BUTTON);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON, GPIO_IRQ_EDGE_FALL, true, &toggle_gif);
    critical_section_init(&crit_sec);
    multicore_launch_core1(usb_task);
    lcd_task();
}
