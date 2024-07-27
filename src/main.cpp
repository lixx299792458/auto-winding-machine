#include <Arduino.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>

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


void setup() {
  pinMode(12,OUTPUT);
  pinMode(13,OUTPUT);
	u8g2.begin();
	u8g2.firstPage();
	do {
		u8g2.setFont(u8g2_font_VCR_OSD_tu);
		u8g2.setCursor(1, 20);
		u8g2.print("hello,esp32-c3");
	} while ( u8g2.nextPage() );
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(12,HIGH);
  digitalWrite(13,HIGH);
  delay(1000);
  digitalWrite(12,LOW);
  digitalWrite(13,LOW);
  delay(1000);

}
