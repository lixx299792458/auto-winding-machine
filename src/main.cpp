#include <Arduino.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Encoder.h>
#include <Preferences.h>
#include <AccelStepper.h>
#include <MultiStepper.h>

//GPIO解锁
//esp32-c3简直就是在诈骗
// ESP32C3的GPIO11(VDD_SPI)默认功能是给flash供电，本开发板的Flash的VDD直接接3.3，所以可以将此IO用作GPIO.
// 以下是操作流程，注意以下的操作只能执行一次，更改后不能复原（因为是设置熔丝位，不是寄存器，一次性操作）
// 使用python的pip安装esptool。pip install esptool
// 将开发板插入电脑, 在设备管理器中可以看到端口, 记录端口号, 例如 COM20
// 打开命令行窗口输入espefuse -p COM20 burn_efuse VDD_SPI_AS_GPIO 1
// 看提示，输入’BURN’

//步进电机存在硬件问题
//其实这也是显而易见的，只不过我被驱动器上5-24V的丝印迷惑了
//3.3V的IO口和5V的阳极输入配合，光耦的二极管根本关不断。改成3.3V供给阳级。
#define DEBUG

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

//屏幕相关定义
//U8G2_SSD1306_128X64_NONAME_1_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, 11, 7, 6);

// //旋转编码器定义部分
Encoder myEnc(5, 4);

//定义步进电机部分
AccelStepper stepperA(AccelStepper::DRIVER, 1 , 0);
AccelStepper stepperB(AccelStepper::DRIVER, 18, 12);
MultiStepper steppers;
long target_positons[2] = {0,0};

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

//机器总共三种状态，0、参数设置状态。1、运行待确认状态。2、运行状态。
uint8_t running_state = 0;

//缠绕所需参数
//总共层数参数
int32_t layer_now = 0;
int32_t layer_total = 0;
//每层匝数参数
int32_t turns_now = 0;
int32_t turns_layer = 0;
int32_t turns_total = 0;
//处理最后一层不满现象
int32_t last_layer_turns = 0;

long last_time_stamp = 0;
int32_t last_turns_now = 9999;
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
	// pinMode(12,OUTPUT);
	// pinMode(13,OUTPUT);
	//设置按键切换状态
	pinMode(8,INPUT_PULLUP);
	pinMode(13,INPUT_PULLUP);
	// 蜂鸣器IO口设置部分
	// pinMode(19,OUTPUT);
	//配置步进电机
	pinMode(12,OUTPUT);
	pinMode(18,OUTPUT);
	stepperA.setMaxSpeed(200);
	stepperB.setMaxSpeed(200);
	stepperB.setPinsInverted(false,true,false);
	steppers.addStepper(stepperA);
	steppers.addStepper(stepperB);
	//开展步进电机的测试，阻塞性质
	// steppers.moveTo(target_positons);
	// steppers.runSpeedToPosition(); // Blocks until all are in position
	// delay(10000);
	//屏幕初始化
	u8g2.begin();
	u8g2.firstPage();
	do {
		u8g2.setFont(u8g2_font_7x14_mr);
		u8g2.setCursor(4, 28);
		u8g2.print("   Auto Winding");
		u8g2.setCursor(4, 52);
		u8g2.print("     Machine   ");		
	} while (u8g2.nextPage());
	delay(2000);
	#ifdef DEBUG
		Serial.begin(115200);
	#endif
}

void loop() {

	if((millis() - last_time_stamp) > 1000){
		last_time_stamp = millis();
	//参数监视功能
	#ifdef DEBUG
		Serial.print("running_state = ");Serial.println(running_state);		
		Serial.print("layer_now = ");Serial.println(layer_now);		
		Serial.print("turns_now = ");Serial.println(turns_now);	
		Serial.print("targetpositionA = ");Serial.println(target_positons[0]);
		Serial.print("targetpositionB = ");Serial.println(target_positons[1]);
		Serial.print("currentpositionA = ");Serial.println(stepperA.currentPosition());
		Serial.print("currentpositionB = ");Serial.println(stepperB.currentPosition());	
		Serial.print("stepperA_distanceToGo = ");Serial.println(stepperA.distanceToGo());
		Serial.print("stepperB_distanceToGo = ");Serial.println(stepperB.distanceToGo());	
		Serial.println();Serial.println();Serial.println();


	#endif	
	}
	//参数设置状态
	if(0 == running_state){
		//切换运行状态
		if (0 == digitalRead(13)){
			//消抖
			delay(20);
			while(!digitalRead(13));
			delay(20);
			running_state = 1;
			//切换至运行状态计算一次参数
			//计算相关绕线参数
			layer_now = 1;
			layer_total = total_turns / layer_turns;
			last_layer_turns = total_turns % layer_turns;
			//最后一层不等于0，则层数加1
			if(0 != last_layer_turns){
				layer_total ++;
			}
			turns_now = 0;
			turns_layer = layer_turns;
			turns_total = total_turns;
			//并将步进电机初始位置复位
			stepperA.setCurrentPosition(0);
			stepperB.setCurrentPosition(0);
			//做一个容错的判断，总匝数必须大于单层匝数,否则无法切换至待运行状态
			if(layer_turns > total_turns){
				running_state = 0;
			}
		}
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
					u8g2.drawBox(1, 22, 120, 20);
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
					u8g2.drawBox(1, 43, 120, 20);
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
					u8g2.drawBox(1, 22, 120, 20);
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
					u8g2.drawBox(1, 43, 120, 20);
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
					wire_diameter = wire_diameter_init + wire_diameter_encoder/4*10;
					nvs_logger.wire_diameter = wire_diameter;
					preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));					
				}
				break;
			case 1:
				newPosition = myEnc.read();
				if (newPosition != layer_turns_encoder) {
					layer_turns_encoder = newPosition;
					layer_turns = layer_turns_init + layer_turns_encoder/4;
					nvs_logger.layer_turns = layer_turns;
					preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));					
				}
				break;			
			case 2:
				newPosition = myEnc.read();
				if (newPosition != total_turns_encoder) {
					total_turns_encoder = newPosition;
					total_turns = total_turns_init + total_turns_encoder/4;
					nvs_logger.total_turns = total_turns;
					preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));
				}
				break;	
			case 3:
				newPosition = myEnc.read();
				if (newPosition != return_error_encoder) {
					return_error_encoder = newPosition;
					return_error = return_error_init + return_error_encoder/4;
					nvs_logger.return_error = return_error;
					preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));	
				}
				break;			
			case 4:
				newPosition = myEnc.read();
				if (newPosition != setting_step_encoder) {
					setting_step_encoder = newPosition;
					setting_step = setting_step_init + setting_step_encoder/4;
					nvs_logger.setting_step = setting_step;
					preferences.putBytes("nvs-log", &nvs_logger, sizeof(nvs_logger));	
				}
				break;	
			case 5:
				newPosition = myEnc.read();
				if (newPosition != move_steps_encoder) {
					move_steps_encoder = newPosition;
				}
				move_steps = move_steps_init + move_steps_encoder/4*setting_step;
				//此处还要驱动步进电机定位运动
				stepperA.moveTo(move_steps);
				stepperA.run();
				break;								
			default:
				break;
		}

	}
	//待运行状态
	if(1 == running_state){
		//点击编码器按键代表取消操作，返回参数设置界面
		if (0 == digitalRead(8)){
			//消抖
			delay(20);
			while(!digitalRead(8));
			delay(20);
			//开始缠绕之后不得再返回设置界面
			if(layer_now == 1){
				running_state = 0;
			}
		}
		//点击按键代表运行
		if (0 == digitalRead(13)){
			//消抖
			delay(20);
			while(!digitalRead(8));
			delay(20);
			running_state = 2;
			//运行之前计算参数
			turns_layer = layer_turns;
			//确定好圈数即可,判断是否是最后一圈且最后一圈不整齐
			if((0!=last_layer_turns)&(layer_now == layer_total))
			{
				turns_layer = last_layer_turns;
			}
			//这个的设定只需要一次，我看又得放在按键转换状态的瞬间了。
			//运行需要三个要素，1、当前层数。2、当前目标方向。3、缠绕圈数。
			//每层是设定一个目标，一次运行到底
			//层数分奇偶，方向不同
			//奇数则正向运行
			if(1 == layer_now % 2){
				//根据本层所需匝数，结合线径，计算步进电机目标位置
				//电机A为排线电机，电机B为绕线电机
				//总距离 = （漆包线直径 + 漆膜厚度*2 + 预留缝隙）* 本层匝数
				//2gt皮带齿距2mm，轴上的同步轮20T，200steps = 40mm
				//
				//再把总距离根据机械变比和步数微分转换为步进电机步数即可
				//以下计算过程，单位均为um
				//target_positons[0] = ((wire_diameter + 2*25 + 100) * turns_layer) / (2000*20) * 200;
				//简化运算，且正向即使不满，也不会出错
				target_positons[0] = ((wire_diameter + 2*25 + 100) * turns_layer) / 400;
				//总圈数 = 本层匝数
				//再把总圈数转换为步进电机步数即可
				//步进电机是绝对定位的，取得现有位置再加上偏移
				target_positons[1] = stepperB.currentPosition() + 200 * turns_layer;
			}
			//偶数则反向运行
			if(0 == layer_now % 2){
				//正常反向运行是到0，也就是回家了。
				target_positons[0] = stepperA.currentPosition() - ((wire_diameter + 2*25 + 100) * turns_layer) / 400;
				//Y轴绕线电机只有一个运行方向
				target_positons[1] = stepperB.currentPosition() + 200 * turns_layer;
			}
			//设定步进电机组输入，并开始无阻塞运行
			steppers.moveTo(target_positons);
		}
		//显示相关内容及确认取消按钮
		u8g2.firstPage();
		do {
			u8g2.setFont(u8g2_font_7x14_mr);
			u8g2.setCursor(4, 16);
			u8g2.print(" layer:  ");u8g2.print(layer_now);u8g2.print("/");u8g2.print(layer_total);
			u8g2.setCursor(4, 37);
			u8g2.print(" continue?");
			u8g2.setCursor(4, 58);
			u8g2.print(" YES!         NO!");
		} while ( u8g2.nextPage() );

		// if(layer_now == layer_total){
		// 	u8g2.firstPage();
		// 	do {
		// 		u8g2.setFont(u8g2_font_7x14_mr);
		// 		u8g2.setCursor(4, 40);
		// 		u8g2.print("done!");
		// 	} while ( u8g2.nextPage() );
		// 	delay(5000);
		// 	running_state = 0;
		// }

	}
	//运行状态
	if(2 == running_state){
		//此函数需要一直调用，步进电机的运行脉冲来自前台而非后台。
		steppers.run();

		// //turns_now是根据绕线电机运行情况计算而来的。
		// //同样也要区分正反
		// if(1 == layer_now % 2){
		// 	//正向运行是正数
		// 	turns_now = stepperB.currentPosition()/200;
		// }
		// //偶数则反向运行
		// if(0 == layer_now % 2){
		// 	//反向运行也是正数，但是是倒着数的，需要反过来
		// 	turns_now = layer_turns - stepperB.currentPosition()/200;
		// }
		//Y轴绕线电机运行方向始终是一个
		turns_now = (stepperB.currentPosition()/200) % turns_layer;


		//显示运行状态匝数
		//在有变化时运行该显示，降低前端的消耗，保障run函数的运行
		if(turns_now != last_turns_now){
			last_turns_now = turns_now;
			u8g2.firstPage();
			do {
				u8g2.setFont(u8g2_font_7x14_mr);
				u8g2.setCursor(4, 30);
				u8g2.print("layers:");u8g2.print(layer_now);u8g2.print("/");u8g2.print(layer_total);				
				u8g2.setCursor(4, 48);
				u8g2.print("turns:");u8g2.print(turns_now);u8g2.print("/");u8g2.print(turns_layer);
			} while ( u8g2.nextPage() );		//结束后切换为待运行状态
		}
		//判断电机是否停机，两个都要判断，如果都已经停机，说明绕弯了本层，退出运行状态即可
		if((0 == stepperA.distanceToGo()) & (0 == stepperB.distanceToGo())){
			running_state = 1;
			layer_now ++;
			//从此处结束整个运行似乎更为妥当
			//判断是否为最后一行，是的话，结束整个程序，显示结束界面
			if(layer_now > layer_total)
			{
				u8g2.firstPage();
				do {
					u8g2.setFont(u8g2_font_7x14_mr);
					u8g2.setCursor(4, 40);
					u8g2.print("all done!");				
				} while ( u8g2.nextPage() );		//结束后切换为待运行状态
				delay(3000);
				running_state = 0;
				//重置所有参数，防止其干扰下一次运行
				layer_now = 0;
				layer_total = 0;
				turns_now = 0;
				turns_layer = 0;
				turns_total = 0;
				last_layer_turns = 0;
				last_time_stamp = 0;
				last_turns_now = 9999;
				//最后一层不完整缠绕后，步进电机绝对位置记忆的也是错的，不过最终都需要对刀
				stepperA.setCurrentPosition(0);
				stepperB.setCurrentPosition(0);
			}
		}
	}	
}

