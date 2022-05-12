/*! @file
  @brief
  mruby/c SPI class for ESP32
*/

#include "mrbc_esp32_ili934x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "font.h"

static char* tag = "main";
#define DMA_CHAN    2

static struct RClass* mrbc_class_esp32_ili934x;
spi_device_handle_t spidev;
uint16_t dc;

static void spi_write_byte(const uint8_t* Data, size_t DataLength)
{
	spi_transaction_t SPITransaction;
    esp_err_t ret;

	if ( DataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = Data;
		ret = spi_device_transmit( spidev, &SPITransaction );
		assert(ret==ESP_OK); 
	}
}

// ILI934X library

uint8_t byte[1024];
static void ili934x_write_color(int color, int size)
{
	int index = 0;
	for(int i = 0;i < size; i++) {
		byte[index++] = (color >> 8) & 0xFF;
		byte[index++] = color & 0xFF;
	}
    gpio_set_level(dc, 1);
	spi_write_byte(byte, size * 2);
}

static void ili934x_write_command(int command)
{
    gpio_set_level(dc, 0);
	spi_write_byte(&command, 1);
}

static void ili934x_write_data(int data)
{
    gpio_set_level(dc, 1);
	spi_write_byte(&data, 1);
}

static void ili934x_write_addr(int addr1, int addr2)
{
    byte[0] = (addr1 >> 8) & 0xFF;
	byte[1] = addr1 & 0xFF;
	byte[2] = (addr2 >> 8) & 0xFF;
	byte[3] = addr2 & 0xFF;
    gpio_set_level(dc, 1);
    spi_write_byte(byte, 4);
}

static void ili934x_draw_pixel(int x, int y, int color)
{
    ili934x_write_command(0x2a);
    ili934x_write_addr(x, x);
    ili934x_write_command(0x2b);
    ili934x_write_addr(y, y);
    ili934x_write_command(0x2c);
    byte[0] = (color >> 8) & 0xFF;
    byte[1] = color & 0xFF;
    gpio_set_level(dc, 1);
    spi_write_byte(byte, 2);
}

static void ili934x_draw_line(int x1, int y1, int x2, int y2, int color)
{
    int i, dx, dy ,sx, sy, E;

    dx = (x2 > x1) ? x2 - x1 : x1 - x2;
    dy = (y2 > y1) ? y2 - y1 : y1 - y2;

    sx = (x2 > x1) ? 1 : -1;
    sy = (y2 > y1) ? 1 : -1;

    if (dx > dy)
    {
        E = -dx;
        for (i = 0; i <= dx; i++)
        {
            ili934x_draw_pixel(x1, y1, color);
            x1 += sx;
            E += 2 * dy;
            if (E >= 0)
            {
                y1 += sy;
                E -= 2 * dx;
            }
        }
    }
    else
    {
        E = -dy;
        for (i = 0; i <= dy; i++)
        {
            ili934x_draw_pixel(x1, y1, color);
            y1 += sy;
            E += 2 * dx;
            if (E >= 0)
            {
                x1 += sx;
                E -= 2 * dy;
            }
        }
    }
}

/*! Method __draw_rectangle(x1, y1, x2, y2, color)
    @param x1 point 1 x
           y1 point 1 y
           x2 point 2 x
           y2 point 2 y
           color 16bit color 
 */
static void
mrbc_esp32_ili934x_draw_fillrectangle(mrb_vm* vm, mrb_value* v, int argc)
{
    int x1 = GET_INT_ARG(1);
    int y1 = GET_INT_ARG(2);
    int x2 = GET_INT_ARG(3);
    int y2 = GET_INT_ARG(4);
    int color = GET_INT_ARG(5);
    ili934x_write_command(0x2a);
    ili934x_write_addr(x1, x2);
    ili934x_write_command(0x2b);
    ili934x_write_addr(y1, y2);
    ili934x_write_command(0x2c);
    int size = y2 - y1 + 1;
    for (int i = x1; i <= x2; i++)
        ili934x_write_color(color, size);
}

/*! Method __draw_line(x1, y1, x2, y2, color)
    @param x1 point 1 x
           y1 point 1 y
           x2 point 2 x
           y2 point 2 y
           color 16bit color 
 */
static void
mrbc_esp32_ili934x_draw_line(mrb_vm* vm, mrb_value* v, int argc)
{
    int x1 = GET_INT_ARG(1);
    int y1 = GET_INT_ARG(2);
    int x2 = GET_INT_ARG(3);
    int y2 = GET_INT_ARG(4);
    int color = GET_INT_ARG(5);
    ili934x_draw_line(x1, y1, x2, y2, color);
}

/*! Method __draw_circle(x, y, r, color)
    @param x0 center x
           y0 center y
           r radius
           color 16bit color 
 */
static void
mrbc_esp32_ili934x_draw_circle(mrb_vm* vm, mrb_value* v, int argc)
{
    int x0 = GET_INT_ARG(1);
    int y0 = GET_INT_ARG(2);
    int r = GET_INT_ARG(3);
    int color = GET_INT_ARG(4);
    int x = 0, y = -r, err = 2 - 2 * r, old_err;
    do
    {
        ili934x_draw_pixel(x0 - x, y0 + y, color);
        ili934x_draw_pixel(x0 - y, y0 - x, color);
        ili934x_draw_pixel(x0 + x, y0 - y, color);
        ili934x_draw_pixel(x0 + y, y0 + x, color);
        if ((old_err = err) <= x)
            err += ++x * 2 + 1;
        if (old_err > y || err > x)
            err += ++y * 2 + 1;
    } while (y < 0);
}

/*! Method __draw_fillcircle(x, y, r, color)
    @param x0 center x
           y0 center y
           r radius
           color 16bit color 
 */
static void
mrbc_esp32_ili934x_draw_fillcircle(mrb_vm* vm, mrb_value* v, int argc)
{
    int x0 = GET_INT_ARG(1);
    int y0 = GET_INT_ARG(2);
    int r = GET_INT_ARG(3);
    int color = GET_INT_ARG(4);
    int x = 0, y = -r, err = 2 - 2 * r, old_err, ChangeX = 1;

    do
    {
        if (ChangeX)
        {
            ili934x_draw_line(x0 - x, y0 - y, x0 - x, y0 + y, color);
            ili934x_draw_line(x0 + x, y0 - y, x0 + x, y0 + y, color);
        } // endif
        ChangeX = (old_err = err) <= x;
        if (ChangeX)
            err += ++x * 2 + 1;
        if (old_err > y || err > x)
            err += ++y * 2 + 1;
    } while (y <= 0);
}

/*! Method __draw_pixel(x, y, color)
    @param x point x
           y point y
           color 16bit color 
 */
static void
mrbc_esp32_ili934x_draw_pixel(mrb_vm* vm, mrb_value* v, int argc)
{
    int x = GET_INT_ARG(1);
    int y = GET_INT_ARG(2);
    int color = GET_INT_ARG(3);
    ili934x_draw_pixel(x, y, color);
}

/*! Method __draw_char(x, y, color)
    @param x point x
           y point y
           c character
           color 16bit color 
           height height pixel size of drawing font
 */
static void
mrbc_esp32_ili934x_draw_char(mrb_vm* vm, mrb_value* v, int argc)
{
    int x = GET_INT_ARG(1);
    int y = GET_INT_ARG(2);
    int c = GET_INT_ARG(3);
    int color = GET_INT_ARG(4);
    int height = GET_INT_ARG(5);

    int index = get(c);
    int ptr = pointers[index];
    if (index == -1) {
        SET_INT_RETURN(HEIGHT);
        return;
    }
    int Width = bitmaps[ptr] >> 12;
    int width = Width * height / HEIGHT;
    int bit = 15 - 4;
    int pre_pos = 0;
    int n = !!(bitmaps[ptr] & 1 << bit);
    for (int dy = 0; dy < height; dy++)
    {
        int i = dy * HEIGHT / height;
        for (int dx = 0; dx < width; dx++)
        {
            int j = dx * Width / width;
            int pos = i * Width + j;
            if (pos != pre_pos)
            {
                bit -= (pos - pre_pos);
                if(bit < 0) {
                    bit += 16;
                    ptr++;
                }
                if(bit >= 16) {
                    bit -= 16;
                    ptr--;
                }
                n = !!(bitmaps[ptr] & 1 << bit);
            }
            if(n)
                ili934x_draw_pixel(x + dx, y + dy, color);
            pre_pos = pos;
        }
    }
    SET_INT_RETURN(width);
}

/*! Register SPI Class.p
 */
void
mrbc_mruby_esp32_spi_gem_init(struct VM* vm)
{
  // 各メソッド定義
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_fillrectangle",mrbc_esp32_ili934x_draw_fillrectangle);
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_circle",       mrbc_esp32_ili934x_draw_circle);
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_fillcircle",   mrbc_esp32_ili934x_draw_fillcircle);
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_line",         mrbc_esp32_ili934x_draw_line);
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_pixel",        mrbc_esp32_ili934x_draw_pixel);
  mrbc_define_method(vm, mrbc_class_esp32_spi, "ili934x_draw_char",         mrbc_esp32_ili934x_draw_char);
}
