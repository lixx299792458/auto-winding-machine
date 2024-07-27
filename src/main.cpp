#include <Arduino.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Encoder.h>


#define DEBUG

//GPIO解锁
//esp32-c3简直就是在诈骗
// ESP32C3的GPIO11(VDD_SPI)默认功能是给flash供电，本开发板的Flash的VDD直接接3.3，所以可以将此IO用作GPIO.
// 以下是操作流程，注意以下的操作只能执行一次，更改后不能复原（因为是设置熔丝位，不是寄存器，一次性操作）
// 使用python的pip安装esptool。pip install esptool
// 将开发板插入电脑, 在设备管理器中可以看到端口, 记录端口号, 例如 COM20
// 打开命令行窗口输入espefuse -p COM20 burn_efuse VDD_SPI_AS_GPIO 1
// 看提示，输入’BURN’

//屏幕相关定义
//U8G2_SSD1306_128X64_NONAME_1_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, 11, 7, 6);

//ESP32-C3芯片，这个编码器库不支持
// //旋转编码器定义部分
// #define CLK 4 // CLK ENCODER 
// #define DT 5 // DT ENCODER 
// ESP32Encoder encoder;
Encoder myEnc(4, 5);
long oldPosition  = -999;


void setup() {
	// //板载LED测试部分
	pinMode(12,OUTPUT);
	// pinMode(13,OUTPUT);

	//设置按键切换状态
	pinMode(8,INPUT_PULLUP);
	pinMode(13,INPUT_PULLUP);
	// 蜂鸣器IO口设置部分
	// pinMode(19,OUTPUT);
	//屏幕初始化
	
	u8g2.begin();
	u8g2.firstPage();
	do {
		u8g2.setFont(u8g2_font_VCR_OSD_tu);
		u8g2.setFontMode(1);
		u8g2.setDrawColor(1);
		u8g2.drawBox(0, 0, 127, 63);
		u8g2.setDrawColor(2);
		u8g2.setCursor(1, 20);
		u8g2.print("1234");
	} while ( u8g2.nextPage() );

	#ifdef DEBUG
		Serial.begin(9600);
		// Serial.println("Basic Encoder Test:");
	#endif


}

void loop() {
	//板载LED测试部分
	digitalWrite(12,HIGH);
	// digitalWrite(13,HIGH);
	delay(1000);
	digitalWrite(12,LOW);
	// digitalWrite(13,LOW);
	delay(1000);
	//蜂鸣器测试部分
	// digitalWrite(19,HIGH);
	// delay(1000);
	// digitalWrite(19,LOW);
	// delay(1000);
	//编码器测试部分
	//   long newPosition = myEnc.read();
	//   if (newPosition != oldPosition) {
	//     oldPosition = newPosition;
	//     Serial.println(newPosition);
	//   }
	if (0 == digitalRead(8)){
		//消抖
		delay(20);
		while(!digitalRead(8));
		delay(20);
		// Serial.print("key1 click");
	}
	if (0 == digitalRead(13)){
		//消抖
		delay(20);
		while(!digitalRead(13));
		delay(20);
		// Serial.print("key2 click");
	}
}

