#include <Arduino.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Encoder.h>
#include <Preferences.h>
//永久存储部分
Preferences preferences;
//结构体才是正道
struct logtype {
	int32_t wire_diameter;
	int32_t layer_turns;
	int32_t total_turns;
	int32_t return_error;
	int32_t setting_step;
};
logtype nvs_logger = {0,0,0,0,0};

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
Encoder myEnc(5, 4);

//定义整个GUI需要的全局变量
//6个菜单
uint8_t item_index = 0;
int32_t newPosition = 0;
//1、漆包线线径，单位mm，精度0.01
char itemA[] = "wire dia:";
int32_t wire_diameter_init = 0;
int32_t wire_diameter_encoder = 0;
int32_t wire_diameter = 0;
//2、单层匝数
char itemB[] = "layer turns:";
int32_t layer_turns_init = 0;
int32_t layer_turns_encoder = 0;
int32_t layer_turns = 0;
//3、总匝数
char itemC[] = "total turns:";
int32_t total_turns_init = 0;
int32_t total_turns_encoder = 0;
int32_t total_turns = 0;
//4、回程误差设置
char itemD[] = "return error:";
int32_t return_error_init = 0;
int32_t return_error_encoder = 0;
int32_t return_error = 0;
//5、对刀步距
char itemE[] = "setting step:";
int32_t setting_step_init = 0;
int32_t setting_step_encoder = 0;
int32_t setting_step = 0;
//6、移动对刀
char itemF[] = "move steps:";
int32_t move_steps_init = 0;
int32_t move_steps_encoder = 0;
int32_t move_steps = 0;

//TODO:使得设置的数值可以保存，并在下次重启后赋予各参数初始值，移动对刀步数除外。

void setup() {
	//上次设置的数值初始化
	preferences.begin("nvs-log", false);
	preferences.getBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));
	wire_diameter_init = nvs_logger.wire_diameter;
	layer_turns_init = nvs_logger.layer_turns;
	total_turns_init = nvs_logger.total_turns;
	return_error_init = nvs_logger.return_error;
	setting_step_init = nvs_logger.setting_step;
	wire_diameter = nvs_logger.wire_diameter;
	layer_turns = nvs_logger.layer_turns;
	total_turns = nvs_logger.total_turns;
	return_error = nvs_logger.return_error;
	setting_step = nvs_logger.setting_step;
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
	// u8g2.firstPage();
	// // do {
	// // 	u8g2.setFont(u8g2_font_VCR_OSD_tu);
	// // 	u8g2.setFontMode(1);
	// // 	u8g2.setDrawColor(1);
	// // 	u8g2.drawBox(0, 0, 127, 63);
	// // 	u8g2.setDrawColor(2);
	// // 	u8g2.setCursor(1, 20);
	// // 	u8g2.print("1234");
	// // } while ( u8g2.nextPage() );
	// do {
	// 	u8g2.setFont(u8g2_font_VCR_OSD_tu);
	// 	u8g2.setCursor(1, 20);
	// 	u8g2.print("1234");

	// 	u8g2.setDrawColor(2);
	// 	u8g2.drawBox(0, 0, 127, 24);

	// } while ( u8g2.nextPage() );
	#ifdef DEBUG
		Serial.begin(9600);
		// Serial.println("Basic Encoder Test:");
	#endif


}

void loop() {
	//板载LED测试部分
	// digitalWrite(12,HIGH);
	// // digitalWrite(13,HIGH);
	// delay(1000);
	// digitalWrite(12,LOW);
	// // digitalWrite(13,LOW);
	// delay(1000);
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
	//处理菜单循环按键
	if (0 == digitalRead(8)){
		//消抖
		delay(20);
		while(!digitalRead(8));
		delay(20);
		// Serial.print("key1 click");
		//自增并对6取余数，完成菜单位置切换
		item_index ++;
		item_index = item_index%6;
		//根据状态直接来一波处理，恢复编码器变量
		//主要原因就是这个操作只需要执行一次，因此放在此处最完美
		switch (item_index)
		{
			case 0:
				myEnc.write(wire_diameter_encoder);
				break;
			case 1:
				myEnc.write(layer_turns_encoder);
				break;			
			case 2:
				myEnc.write(total_turns_encoder);
				break;
			case 3:
				myEnc.write(return_error_encoder);
				break;				
			case 4:
				myEnc.write(setting_step_encoder);
				break;
			case 5:
				myEnc.write(move_steps_encoder);
				break;								
			default:
				break;
		}
	}
	//根据菜单位置处理显示及其他问题
	switch (item_index) {
		case 0:
			//首先显示前三行并选中第一行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				// u8g2.print(itemA);u8g2.print(wire_diameter);u8g2.print("um");
				u8g2.print(itemA);u8g2.printf("%4.2f",(wire_diameter/1000.0F));u8g2.print("mm");
				u8g2.setCursor(4, 37);
				u8g2.print(itemB);u8g2.print(layer_turns);
				u8g2.setCursor(4, 58);
				u8g2.print(itemC);u8g2.print(total_turns);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 1, 120, 20);
			} while ( u8g2.nextPage() );
			break;
		case 1:
			//首先显示前三行并选中第2行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				u8g2.print(itemA);u8g2.printf("%4.2f",(wire_diameter/1000.0F));u8g2.print("mm");
				u8g2.setCursor(4, 37);
				u8g2.print(itemB);u8g2.print(layer_turns);
				u8g2.setCursor(4, 58);
				u8g2.print(itemC);u8g2.print(total_turns);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 22, 120, 41);
			} while ( u8g2.nextPage() );
			break;
		case 2:
			//首先显示前三行并选中第3行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				u8g2.print(itemA);u8g2.printf("%4.2f",(wire_diameter/1000.0F));u8g2.print("mm");
				u8g2.setCursor(4, 37);
				u8g2.print(itemB);u8g2.print(layer_turns);
				u8g2.setCursor(4, 58);
				u8g2.print(itemC);u8g2.print(total_turns);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 43, 120, 62);
			} while ( u8g2.nextPage() );
			break;
		case 3:
			//首先显示后三行并选中第1行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				u8g2.print(itemD);u8g2.print(return_error);
				u8g2.setCursor(4, 37);
				u8g2.print(itemE);u8g2.print(setting_step);
				u8g2.setCursor(4, 58);
				u8g2.print(itemF);u8g2.print(move_steps);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 1, 120, 20);
			} while ( u8g2.nextPage() );
			break;
		case 4:
			//首先显示后三行并选中第2行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				u8g2.print(itemD);u8g2.print(return_error);
				u8g2.setCursor(4, 37);
				u8g2.print(itemE);u8g2.print(setting_step);
				u8g2.setCursor(4, 58);
				u8g2.print(itemF);u8g2.print(move_steps);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 22, 120, 41);
			} while ( u8g2.nextPage() );
			break;
		case 5:
			//首先显示后三行并选中第2行
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 16);
				u8g2.print(itemD);u8g2.print(return_error);
				u8g2.setCursor(4, 37);
				u8g2.print(itemE);u8g2.print(setting_step);
				u8g2.setCursor(4, 58);
				u8g2.print(itemF);u8g2.print(move_steps);
				u8g2.setDrawColor(2);
				u8g2.drawBox(1, 43, 120, 62);
			} while ( u8g2.nextPage() );
			break;
		default:
			// code block
			break;
	}
	//处理编码器的问题
	switch (item_index)	{
		case 0:
			newPosition = myEnc.read();
			if (newPosition != wire_diameter_encoder) {
				wire_diameter_encoder = newPosition;
			}
			wire_diameter = wire_diameter_init + wire_diameter_encoder/4*10;
			nvs_logger.wire_diameter = wire_diameter;
			preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));
			break;
		case 1:
			newPosition = myEnc.read();
			if (newPosition != layer_turns_encoder) {
				layer_turns_encoder = newPosition;
			}
			layer_turns = layer_turns_init + layer_turns_encoder/4;
			nvs_logger.layer_turns = layer_turns;
			preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));
			break;			
		case 2:
			newPosition = myEnc.read();
			if (newPosition != total_turns_encoder) {
				total_turns_encoder = newPosition;
			}
			total_turns = total_turns_init + total_turns_encoder/4;
			nvs_logger.total_turns = total_turns;
			preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));
			break;	
		case 3:
			newPosition = myEnc.read();
			if (newPosition != return_error_encoder) {
				return_error_encoder = newPosition;
			}
			return_error = return_error_init + return_error_encoder/4;
			nvs_logger.return_error = return_error;
			preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));			
			break;			
		case 4:
			newPosition = myEnc.read();
			if (newPosition != setting_step_encoder) {
				setting_step_encoder = newPosition;
			}
			setting_step = setting_step_init + setting_step_encoder/4;
			nvs_logger.setting_step = setting_step;
			preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));	
			break;	
		case 5:
			newPosition = myEnc.read();
			if (newPosition != move_steps_encoder) {
				move_steps_encoder = newPosition;
			}
			move_steps = move_steps_init + move_steps_encoder/4;
			//此处还要驱动步进电机定位运动
			break;								
		default:
			break;
	}
	//切换运行状态
	if (0 == digitalRead(13)){
		//消抖
		delay(20);
		while(!digitalRead(13));
		delay(20);
		// Serial.print("key2 click");
	}
}

