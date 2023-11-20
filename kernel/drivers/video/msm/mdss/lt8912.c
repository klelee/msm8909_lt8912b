#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/irqreturn.h>
#include <linux/kd.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include "lt8912.h"
#define MIPI_1080P
#define device_name	"lt8912"

/*****************************************************************************************/
/* lt8912 相关参数配置 */
uint8_t I2CADR;

#define _HDMI_Output_
#define _LVDS_Output_

enum
{
	H_act = 0,
	V_act,
	H_tol,
	V_tol,
	H_bp,
	H_sync,
	V_sync,
	V_bp
};

#ifdef _HDMI_Output_
enum {
	_32KHz = 0,
	_44d1KHz,
	_48KHz,

	_88d2KHz,
	_96KHz,
	_176Khz,
	_196KHz
};
uint16_t IIS_N[] =
{
	4096,               // 32K
	6272,               // 44.1K
	6144,               // 48K
	12544,              // 88.2K
	12288,              // 96K
	25088,              // 176K
	24576               // 196K
};

uint16_t Sample_Freq[] =
{
	0x30,               // 32K
	0x00,               // 44.1K
	0x20,               // 48K
	0x80,               // 88.2K
	0xa0,               // 96K
	0xc0,               // 176K
	0xe0                // 196K
};
#endif  // _HDMI_Output_

#ifdef _LVDS_Output_
/* 设置输出模式，ByPass模式，MIPI输出多大，LVDS就输出多大；Scaler模式，可以调整缩放比例 */
#define _Bypass_Mode_
// #define _Scaler_Mode_
/* 设置色深 */
#define _8_Bit_Color_   // 24 bit
// #define _6_Bit_Color_ // 18 bit
/* 定义LVDS输出模式 */
#define _VESA_
// #define _JEIDA_
// #define _De_mode_ // only DE 
#define _Sync_Mode_ // Hsync + Vsync + DE
/***Output sync mode sync polarity***/
#define _Hsync_polarity_active_high		0x02 // default, POSITIVE
#define _Hsync_polarity_active_low		0x00 // NEGATIVE
#define _Vsync_polarity_active_high		0x01 // default, POSITIVE
#define _Vsync_polarity_active_low		0x00 // NEGATIVE
#define _Hsync_polarity		_Hsync_polarity_active_high
#define _Vsync_polarity		_Vsync_polarity_active_high

#define _VesaJeidaMode 0x00
#define _DE_Sync_mode 0x00

#ifdef _VESA_
#define _VesaJeidaMode 0x00
#else
#define _VesaJeidaMode 0x20
#endif  // _VESA_
#ifdef _De_mode_
#define _DE_Sync_mode 0x08
#else
#define _DE_Sync_mode 0x00
#endif  // _De_mode_
#ifdef _8_Bit_Color_
#define _ColorDeepth 0x10
#else
#define _ColorDeepth 0x14
#endif  // _8_Bit_Color_
#endif  // _LVDS_Output_

#ifdef _HDMI_Output_
u8	AVI_PB0	   = 0x00;
u8	AVI_PB1	   = 0x00;
u8	AVI_PB2	   = 0x00;

/*
   Resolution			HDMI_VIC
   --------------------------------------
   640x480				1
   720x480P 60Hz		2
   720x480i 60Hz		6

   720x576P 50Hz		17
   720x576i 50Hz		21

   1280x720P 24Hz		60
   1280x720P 25Hz		61
   1280x720P 30Hz		62
   1280x720P 50Hz		19
   1280x720P 60Hz		4

   1920x1080P 24Hz		32
   1920x1080P 25Hz		33
   1920x1080P 30Hz		34

   1920x1080i 50Hz		20
   1920x1080i 60Hz		5

   1920x1080P 50Hz		31
   1920x1080P 60Hz		16

   3840x2160 30Hz		95 // 4K30

   Other resolution	0(default)
*/
u8	HDMI_VIC = 0x00; 
#endif  // _HDMI_Output_

/*****************************************************************************************/
/* MIPI 输入信号配置 */
#define MIPI_Lane 4

// 根据前端MIPI输入信号的Timing修改以下宏定义的值:
#define MIPI_H_Active	1280
#define MIPI_V_Active	720

#define MIPI_H_Total	1650
#define MIPI_V_Total	750

#define MIPI_H_FrontPorch	110
#define MIPI_H_SyncWidth	40
#define MIPI_H_BackPorch	220

#define MIPI_V_FrontPorch	5
#define MIPI_V_SyncWidth	5
#define MIPI_V_BackPorch	20

u8 MIPI_Lane_CH_Swap = 0x00;// 00: 0123 normal ; a8 : 3210 swap

#define _PN_Swap_En		0x20
#define _PN_Swap_Dis	0x00

u8	MIPI_Lane_PN_Swap = _PN_Swap_Dis ;

/* VESA配置 */
#define 	VESA_720x480_60			0
#define 	VESA_1280x720_60	  		(VESA_720x480_60+1)
#define 	VESA_1280x800_60			(VESA_1280x720_60+1)
#define 	VESA_1024x600_60		(VESA_1280x800_60+1)
#define 	VESA_1920x1080_60			(VESA_1024x600_60+1)

struct lt8912_data {
	struct i2c_client *lt8912_client;
	struct regmap           *regmap;
	struct input_dev        *input_hotplug;
	struct delayed_work 	hotplug_work;
	int reset_gpio;
	int last_val;
};

struct lt8912_data *lt;
/*****************************************************************************************/
/* LT8912 操作函数开始 */
static int lt8912_i2c_write_byte(uint8_t reg, uint8_t val)
{
	int ret = 0;
 
	if (!lt) {
		printk("dsi0 xxxx%s: Invalid argument\n", __func__);
		return -EINVAL;
	}
	lt->lt8912_client->addr = I2CADR >> 1;
	ret = i2c_smbus_write_byte_data(lt->lt8912_client, reg, val);
	if (ret)
		pr_err_ratelimited("%s: wr err: addr 0x%x, reg 0x%x, val 0x%x\n", __func__, I2CADR, reg, val);
	return ret;
}
 
static int lt8912_i2c_read_byte(uint8_t reg)
{
	int ret = 0;
 
	if (!lt) {
		printk("dsi0 xxxx%s: Invalid argument\n", __func__);
		return -EINVAL;
	}
 
	lt->lt8912_client->addr = I2CADR >> 1;

	ret = i2c_smbus_read_byte_data(lt->lt8912_client, reg);
	if (ret < 0) {
		printk("failed to read byte lt index=%d\n", reg);
		return -1;
	} else {
		printk("Like: Read Reg [%02X][%02X]\n", reg, ret);
	}
	return 0;
}

void MIPI_Digital(void)
{
	I2CADR = 0x92;
	lt8912_i2c_write_byte( 0x18, (u8)( MIPI_H_SyncWidth % 256 ) );     // hwidth
	lt8912_i2c_write_byte( 0x19, (u8)( MIPI_V_SyncWidth % 256 ) );     // vwidth
	lt8912_i2c_write_byte( 0x1c, (u8)( MIPI_H_Active % 256 ) );        // H_active[7:0]
	lt8912_i2c_write_byte( 0x1d, (u8)( MIPI_H_Active / 256 ) );        // H_active[15:8]

	lt8912_i2c_write_byte( 0x1e, 0x67 );
	lt8912_i2c_write_byte( 0x2f, 0x0c );

	lt8912_i2c_write_byte( 0x34, (u8)( MIPI_H_Total % 256 ) );         // H_total[7:0]
	lt8912_i2c_write_byte( 0x35, (u8)( MIPI_H_Total / 256 ) );         // H_total[15:8]

	lt8912_i2c_write_byte( 0x36, (u8)( MIPI_V_Total % 256 ) );         // V_total[7:0]
	lt8912_i2c_write_byte( 0x37, (u8)( MIPI_V_Total / 256 ) );         // V_total[15:8]

	lt8912_i2c_write_byte( 0x38, (u8)( MIPI_V_BackPorch % 256 ) );     // VBP[7:0]
	lt8912_i2c_write_byte( 0x39, (u8)( MIPI_V_BackPorch / 256 ) );     // VBP[15:8]
	lt8912_i2c_write_byte( 0x3a, (u8)( MIPI_V_FrontPorch % 256 ) );    // VFP[7:0]
	lt8912_i2c_write_byte( 0x3b, (u8)( MIPI_V_FrontPorch / 256 ) );    // VFP[15:8]
	lt8912_i2c_write_byte( 0x3c, (u8)( MIPI_H_BackPorch % 256 ) );     // HBP[7:0]
	lt8912_i2c_write_byte( 0x3d, (u8)( MIPI_H_BackPorch / 256 ) );     // HBP[15:8]
	lt8912_i2c_write_byte( 0x3e, (u8)( MIPI_H_FrontPorch % 256 ) );    // HFP[7:0]
	lt8912_i2c_write_byte( 0x3f, (u8)( MIPI_H_FrontPorch / 256 ) );    // HFP[15:8]
}

/***********************************************************

***********************************************************/
void DDS_Config(void)
{
	I2CADR = 0x92;
	lt8912_i2c_write_byte( 0x4e, 0x52 );
	lt8912_i2c_write_byte( 0x4f, 0xde );
	lt8912_i2c_write_byte( 0x50, 0xc0 );
	lt8912_i2c_write_byte( 0x51, 0x80 );
	//  lt8912_i2c_write_byte( 0x51, 0x00 );

	lt8912_i2c_write_byte( 0x1e, 0x4f );
	lt8912_i2c_write_byte( 0x1f, 0x5e );
	lt8912_i2c_write_byte( 0x20, 0x01 );
	lt8912_i2c_write_byte( 0x21, 0x2c );
	lt8912_i2c_write_byte( 0x22, 0x01 );
	lt8912_i2c_write_byte( 0x23, 0xfa );
	lt8912_i2c_write_byte( 0x24, 0x00 );
	lt8912_i2c_write_byte( 0x25, 0xc8 );
	lt8912_i2c_write_byte( 0x26, 0x00 );

	lt8912_i2c_write_byte( 0x27, 0x5e );
	lt8912_i2c_write_byte( 0x28, 0x01 );
	lt8912_i2c_write_byte( 0x29, 0x2c );
	lt8912_i2c_write_byte( 0x2a, 0x01 );
	lt8912_i2c_write_byte( 0x2b, 0xfa );
	lt8912_i2c_write_byte( 0x2c, 0x00 );
	lt8912_i2c_write_byte( 0x2d, 0xc8 );
	lt8912_i2c_write_byte( 0x2e, 0x00 );

	lt8912_i2c_write_byte( 0x42, 0x64 );
	lt8912_i2c_write_byte( 0x43, 0x00 );
	lt8912_i2c_write_byte( 0x44, 0x04 );
	lt8912_i2c_write_byte( 0x45, 0x00 );
	lt8912_i2c_write_byte( 0x46, 0x59 );
	lt8912_i2c_write_byte( 0x47, 0x00 );
	lt8912_i2c_write_byte( 0x48, 0xf2 );
	lt8912_i2c_write_byte( 0x49, 0x06 );
	lt8912_i2c_write_byte( 0x4a, 0x00 );
	lt8912_i2c_write_byte( 0x4b, 0x72 );
	lt8912_i2c_write_byte( 0x4c, 0x45 );
	lt8912_i2c_write_byte( 0x4d, 0x00 );
	lt8912_i2c_write_byte( 0x52, 0x08 );
	lt8912_i2c_write_byte( 0x53, 0x00 );
	lt8912_i2c_write_byte( 0x54, 0xb2 );
	lt8912_i2c_write_byte( 0x55, 0x00 );
	lt8912_i2c_write_byte( 0x56, 0xe4 );
	lt8912_i2c_write_byte( 0x57, 0x0d );
	lt8912_i2c_write_byte( 0x58, 0x00 );
	lt8912_i2c_write_byte( 0x59, 0xe4 );
	lt8912_i2c_write_byte( 0x5a, 0x8a );
	lt8912_i2c_write_byte( 0x5b, 0x00 );
	lt8912_i2c_write_byte( 0x5c, 0x34 );

	lt8912_i2c_write_byte( 0x51, 0x00 );
}

#ifdef _HDMI_Output_
void Audio_Config(void)
{
	// Audio config
	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0xB2, 0x01 );   // 0x01:HDMI; 0x00: DVI

	I2CADR = 0x94;
	lt8912_i2c_write_byte( 0x06, 0x08 );   // 0x09
	lt8912_i2c_write_byte( 0x07, 0xF0 );   // enable Audio: 0xF0;	Audio Mute: 0x00

	lt8912_i2c_write_byte( 0x09, 0x00 );   // 0x00:Left justified; default


	lt8912_i2c_write_byte( 0x0f, 0x0b + Sample_Freq[_48KHz] );

	lt8912_i2c_write_byte( 0x37, (u8)( IIS_N[_48KHz] / 0x10000 ) );
	lt8912_i2c_write_byte( 0x36, (u8)( ( IIS_N[_48KHz] & 0x00FFFF ) / 0x100 ) );
	lt8912_i2c_write_byte( 0x35, (u8)( IIS_N[_48KHz] & 0x0000FF ) );

	lt8912_i2c_write_byte( 0x34, 0xD2 );   // 32 bit的数据长度
//	lt8912_i2c_write_byte( 0x34, 0xE2 ); // 16 bit的数据长度

	lt8912_i2c_write_byte( 0x3c, 0x41 );   // Null packet enable
}

void AVI_Config(void)
{
	I2CADR = 0x94;
	lt8912_i2c_write_byte( 0x3e, 0x0A );

	// 0x43寄存器是checksums，改变了0x45或者0x47 寄存器的值，0x43寄存器的值也要跟着变，
	// 0x43，0x44，0x45，0x47四个寄存器值的总和是0x6F

	HDMI_VIC = 0x04; // 720P 60; Corresponding to the resolution to be output
	// HDMI_VIC = 0x10;                        // 1080P 60
	// HDMI_VIC = 0x1F; // 1080P 50
	// HDMI_VIC = 0x00; // If the resolution is non-standard, set to 0x00
	AVI_PB1 = 0x10;                         // PB1,color space: YUV444 0x70;YUV422 0x30; RGB 0x10
	AVI_PB2 = 0x2A;                         // PB2; picture aspect rate: 0x19:4:3 ;     0x2A : 16:9
	AVI_PB0 = ( ( AVI_PB1 + AVI_PB2 + HDMI_VIC ) <= 0x6f ) ? ( 0x6f - AVI_PB1 - AVI_PB2 - HDMI_VIC ) : ( 0x16f - AVI_PB1 - AVI_PB2 - HDMI_VIC );

	lt8912_i2c_write_byte( 0x43, AVI_PB0 );    //avi packet checksum ,avi_pb0
	lt8912_i2c_write_byte( 0x44, AVI_PB1 );    //avi packet output RGB 0x10
	lt8912_i2c_write_byte( 0x45, AVI_PB2 );    //0x19:4:3 ; 0x2A : 16:9
	lt8912_i2c_write_byte( 0x47, HDMI_VIC );   //VIC(as below);1080P60 : 0x10
}

#endif

#ifdef _Bypass_Mode_
void LVDS_Bypass_Config(void)
{
	//lvds power up
	I2CADR = 0x90;                      //0x90;
	//lt8912_i2c_write_byte( 0x51, 0x05 );

	//core pll bypass
	lt8912_i2c_write_byte( 0x50, 0x24 );   //cp=50uA
	lt8912_i2c_write_byte( 0x51, 0x2d );   //Pix_clk as reference,second order passive LPF PLL
	lt8912_i2c_write_byte( 0x52, 0x04 );   //loopdiv=0;use second-order PLL

	//PLL CLK
	lt8912_i2c_write_byte( 0x69, 0x0e );
	lt8912_i2c_write_byte( 0x69, 0x8e );
	lt8912_i2c_write_byte( 0x6a, 0x00 );
	lt8912_i2c_write_byte( 0x6c, 0xb8 );
	lt8912_i2c_write_byte( 0x6b, 0x51 );

	lt8912_i2c_write_byte( 0x04, 0xfb ); //core pll reset
	lt8912_i2c_write_byte( 0x04, 0xff );

	//scaler bypass
	lt8912_i2c_write_byte( 0x7f, 0x00 );
	lt8912_i2c_write_byte( 0xa8, _VesaJeidaMode + _DE_Sync_mode + _ColorDeepth + _Hsync_polarity + _Vsync_polarity);

	msleep( 100 );
	lt8912_i2c_write_byte( 0x02, 0xf7 );   //lvds pll reset
	lt8912_i2c_write_byte( 0x02, 0xff );
	lt8912_i2c_write_byte( 0x03, 0xcb );   //scaler module reset
	lt8912_i2c_write_byte( 0x03, 0xfb );   //lvds tx module reset
	lt8912_i2c_write_byte( 0x03, 0xff );
}
#endif

void lt8912_reset(void)
{
	gpio_direction_output(lt->reset_gpio, 0);
	msleep(100);
	gpio_direction_output(lt->reset_gpio, 1);
	msleep(100);
}

void lt8912_read_test(void)
{
	// video check
	lt8912_i2c_read_byte( 0x9c );
	lt8912_i2c_read_byte( 0x9d );
	lt8912_i2c_read_byte( 0x9e );
	lt8912_i2c_read_byte( 0x9f );
	// pixel clock
	lt8912_i2c_read_byte( 0x0c );
	lt8912_i2c_read_byte( 0x0d );
	lt8912_i2c_read_byte( 0x0e );
	lt8912_i2c_read_byte( 0x0f );
	// mipi lane
	lt8912_i2c_read_byte( 0x13 );
	// lane swap
	lt8912_i2c_read_byte( 0x15 );

}

void lt8912_start(void)
{
	lt8912_reset();
	I2CADR = 0x90;                      // IIC address

	/* LVDS 配置 */
	lt8912_i2c_write_byte( 0x08, 0xff );   // Register address : 0x08;	 Value : 0xff
	lt8912_i2c_write_byte( 0x09, 0xff );
	lt8912_i2c_write_byte( 0x0a, 0xff );
	lt8912_i2c_write_byte( 0x0b, 0x7c );   //
	lt8912_i2c_write_byte( 0x0c, 0xff );

	lt8912_i2c_write_byte( 0x51, 0x15 );

	I2CADR = 0x90;

	lt8912_i2c_write_byte( 0x31, 0xa1 );
	lt8912_i2c_write_byte( 0x32, 0xbf );
	lt8912_i2c_write_byte( 0x33, 0x17 ); // bit0/bit1 =1 Turn On HDMI Tx；  bit0/bit1 = 0 Turn Off HDMI Tx
	lt8912_i2c_write_byte( 0x37, 0x00 );
	lt8912_i2c_write_byte( 0x38, 0x22 );
	lt8912_i2c_write_byte( 0x60, 0x82 );

	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0x39, 0x45 );
	lt8912_i2c_write_byte( 0x3a, 0x00 );
	lt8912_i2c_write_byte( 0x3b, 0x00 );

	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0x3e, 0x96 + MIPI_Lane_PN_Swap );   //
	lt8912_i2c_write_byte( 0x41, 0x7c );   //HS_eq current

	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0x44, 0x31 );
	lt8912_i2c_write_byte( 0x55, 0x44 );
	lt8912_i2c_write_byte( 0x57, 0x01 );
	lt8912_i2c_write_byte( 0x5a, 0x02 );

	I2CADR = 0x92;
	lt8912_i2c_write_byte( 0x10, 0x01 );               // 0x05
	lt8912_i2c_write_byte( 0x11, 0x08 );               // 0x12
	lt8912_i2c_write_byte( 0x12, 0x04 );
	lt8912_i2c_write_byte( 0x13, MIPI_Lane % 0x04 );   // 00 4 lane  // 01 lane // 02 2lane                                            //03 3 lane
	lt8912_i2c_write_byte( 0x14, 0x00 );

	lt8912_i2c_write_byte( 0x15, MIPI_Lane_CH_Swap );  // 00: 0123 normal ; a8 : 3210 swap
	
	lt8912_i2c_write_byte( 0x1a, 0x03 );
	lt8912_i2c_write_byte( 0x1b, 0x03 );

	//  设置 MIPI Timing
	MIPI_Digital();
	DDS_Config();

#ifdef _HDMI_Output_
	Audio_Config();
	AVI_Config();
#endif

	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0x03, 0x7f );       // mipi rx reset
	msleep( 10 );
	lt8912_i2c_write_byte( 0x03, 0xff );

	lt8912_i2c_write_byte( 0x05, 0xfb );       // DDS reset
	msleep( 10 );
	lt8912_i2c_write_byte( 0x05, 0xff );

#ifdef _LVDS_Output_
#ifdef _Bypass_Mode_
	LVDS_Bypass_Config();
#endif
	I2CADR = 0x90;
	lt8912_i2c_write_byte( 0x44, 0x30 ); // Turn on LVDS output
#endif
	I2CADR = 0x90;                      // IIC address
	lt8912_read_test();
}
EXPORT_SYMBOL_GPL(lt8912_start);
/*****************************************************************************************/

static const struct of_device_id of_lt8912_match[] = {
	{ .compatible = "lontium,lt8912",},
	{},
};

static const struct i2c_device_id lt8912_id[] = {
	{device_name, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lt8912_id);

static int lt8912_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	lt->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	printk("lt8912_reset_gpio number: %d\n", lt->reset_gpio);
	if (lt->reset_gpio < 0)
		return lt->reset_gpio;
	return 0;
}

static int lontium_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	printk("Like: %s enter!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "lt8912 i2c check failed.\n");
		return -ENODEV;
	}
	lt = devm_kzalloc(&client->dev, sizeof(struct lt8912_data), GFP_KERNEL);
	lt->lt8912_client = client;
	if (client->dev.of_node) {
		ret = lt8912_parse_dt(&client->dev);
		if (ret) {
			printk("Like: lt8912_parse_dt failed!\n");
			goto out;
		}
	} else {
		printk("Like: lt8912 device tree is failed!\n");
		ret = -ENODEV;
		goto out;
	}
	dev_set_drvdata(&client->dev, lt);
	if (gpio_is_valid(lt->reset_gpio)) {
		ret = gpio_request(lt->reset_gpio, "lt8912_reset_gpio");
		if (ret) {
			printk("Like: reset gpio request failed!\n");
			goto out;
		}
		printk("Like: enter gpio_request!\n");
		ret = gpio_direction_output(lt->reset_gpio, 0);
		if (ret) {
			printk("set_direction for reset gpio failed\n");
			goto free_reset_gpio;
		}
		msleep(20);
		gpio_set_value_cansleep(lt->reset_gpio, 1);
		printk("Like: enter gpio_set_value_cansleep\n");
	}

	lt8912_start();

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

free_reset_gpio:
	if (gpio_is_valid(lt->reset_gpio))
		gpio_free(lt->reset_gpio);
out:
	printk("Like: lt8912_probe exit...\n");
	return ret;
}

static int lontium_i2c_remove(struct i2c_client *client)
{
	lt = dev_get_drvdata(&client->dev);
	pm_runtime_disable(&client->dev);
	if (gpio_is_valid(lt->reset_gpio))
		gpio_free(lt->reset_gpio);
	return 0;
}

static int lt8912_suspend(struct device *tdev) {
	printk("Like: lt8912 will suspend!\n");
	// gpio_set_value(lt->reset_gpio, 0);
    return 0;
}

static int lt8912_resume(struct device *tdev) {
	printk("Like: Will restart lt8912!\n");
	lt8912_start();
    return 0;
}

static const struct dev_pm_ops lt8912_pm_ops =
{ 
    .suspend = lt8912_suspend,
    .resume = lt8912_resume, 
};

static struct i2c_driver lontium_i2c_driver = {
	.driver = {
		.name = "lt8912",
		.owner = THIS_MODULE,
		.of_match_table = of_lt8912_match,
		.pm = &lt8912_pm_ops,
	},
	.probe = lontium_i2c_probe,
	.remove = lontium_i2c_remove,
	.id_table = lt8912_id,
};

static int __init lontium_i2c_test_init(void)
{
	int ret;
	printk("Like: lt8912b driver enter!\n");
	ret = i2c_add_driver(&lontium_i2c_driver);
	return ret;
}

static void __exit lontium_i2c_test_exit(void)
{
	printk("Like: lt8912b driver exit!\n");
	i2c_del_driver(&lontium_i2c_driver);
}

module_init(lontium_i2c_test_init);
module_exit(lontium_i2c_test_exit);
MODULE_AUTHOR("like <like@aucma.com.cn>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LT8912 MIPI·DSI to HDMI and LVDS");
