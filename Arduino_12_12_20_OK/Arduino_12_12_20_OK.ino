//โปรแกรม Arduino 2020
// 3 phase PWM sine
// (c) 2016 C. Masenas
// ดัดแปลงเพิ่มเติม จากต้นฉับ DDS generator ของ KHM 2009 / Martin Nawrath
// Modified 12-01-2020 By Seree2004
#include <LiquidCrystal.h> //เรียกโปรแกรมคำสั่งการใช้งานจอแสดงผล LCD เข้ามาร่วมด้วย

byte Table_Buffer[128];
// เปลี่ยนวิธีการสั่งเซทค่าของรีจีสเตอร์ภายในของ Arduino ใหม่เพื่อให้เรียกใช้ง่ายขึ้น
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
// เปลี่ยนชื่อเรียกขาของ Arduino ใหม่เพื่อให้จำง่ายในการต่อวงจร
#define PWM_OUT_UL 6 // PWM1 UL output
#define PWM_OUT_UH 5 // PWM1 UH output
#define PWM_OUT_VL 9 // PWM2 VL output
#define PWM_OUT_VH 10 // PWM2 VH output
#define PWM_OUT_WL 11 // PWM3 WL output
#define PWM_OUT_WH 3 // PWM3 WH output
#define testPin 13 // Test OutPut เพื่อกระพริบหลอดไฟ LED
#define enablePin 8 // เพื่อปิด/เปิดภาค Power Driver
#define single_3ph 12 //เพือตัวจั้มเปอร์ ให้เป็นแบบ ซิงเกิลเฟส หรือ แบบ 3 เฟส
#define FAULT_DETECT 2 //เพื่อตรวจรับสัญญาณผิดพลาดต่างๆจะได้ปิดภาค Power Driver ก่อนที่จะทำให้อุปกรณ์เสียหาย

#define FREQ_POT 0 // เพื่อเต่อกับวอลลุ่มปรับความถี่ analog input A0 
//#define CURRENT_POT 1 // สำรองไว้ analog input A1 
//#define START_STOP 1 // ให้หยุด/เดินต่อ 
#define USER1 6 // สำรองไว้ analog input A6
#define USER2 7 // สำรองไว้ analog input A7

// ตั้งชื่อการกำหนดค่าของ วอลลุ่มปรับความเร็วรอบ (Hz)ต่ำสุดและสูงสุด
#define FREQ_MIN 10 // default: 10
#define FREQ_MAX 60 // เปลี่ยนความถี่สูงสุดที่นี่

#define MAX_AMPLITUDE 124
#define DT 5
// เปลี่ยนชื่อเรียกค่าตายตัวของตัวปรับวอลลุ่มของ Arduino ซึ่งเป็นความสามารถของ MCU ในแต่ละเบอร์อาจต่างกัน
#define POT_MIN 0 // default: 0
#define POT_MAX 1023 // default: 1023

// ค่าความถี่ PWM const double refclk=3921; // =16MHz /8/ 510 = 3921
const float refclk = 3921 ; // ค่าตายตัวที่ได้มาจากการคำนวน คริสตอลที่ใช้ 16000000Hz/8/510 = 3921

// ตั้งชื่อตัวแปรที่ใช้ในการคำนวนรอบของสัญญาณซายเวฟและต่อเนื่องกันอย่างสมบูรณ์ interrupt service declared as voilatile
volatile unsigned long sigma; // phase accumulator
volatile unsigned long delta; // phase increment

volatile unsigned long c4ms; // counter incremented every 4ms

//ตั้งชื่อตัวแปรต่าง ๆ
// ค่าเขียนอักษรภาษาไทย
byte character1[8] = {0,18,18,18,18,18,27,0};
byte character2[8] = {0,14,17,29,21,21,25,0};
byte character3[8] = {0,4,4,4,4,4,6,0};
byte character4[8] = {0,2,31,1,29,19,25,0};
byte character5[8] = {1,15,6,9,14,1,6,0};
// ค่าตัวแปรต่าง ๆ
byte temp_ocr, phase0, phase1, phase2, freq, freq_old, freq_new,temp,Amplitude;

bool single = false;
bool testPin_status = false;
bool OPTO_EN = true;
// ตั้งชื่อค่าตายตัวที่เรากำหนดในการต่อขาต่าง ๆ ของ Arduino เข้ากับจอ LCD เพื่อให้จำง่าย
const int RS = 7, EN = 4, i4 = A2, i5 = A3, i6 = A4, i7 = A5;
// แล้วสั่งให้กำหนดตามนี้ได้เลย
LiquidCrystal lcd(RS, EN, i4, i5, i6, i7);

//*********** เปลี่ยนให้แสดงผล Version ที่นี่ **************************** 
char My_Version[9] = "12-12-20";
//*****************************************************************
void(* resetSoftware)(void) = 0; //ตั้งฟังชั่นการ RESET โปรแกรม
// เริ่มต้นตัวโปรแกรมโดยคำสั่งตั้งค่าต่าง ๆ ก่อน
void setup(){
//-----------------------------------------------------------------------------
    Serial.begin(115200);
    Serial.println("Strrt");
    pinMode(single_3ph, INPUT_PULLUP);
    pinMode(enablePin, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(testPin, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก

    pinMode(PWM_OUT_UL, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(PWM_OUT_UH, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(PWM_OUT_VL, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(PWM_OUT_VH, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(PWM_OUT_WL, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก
    pinMode(PWM_OUT_WH, OUTPUT); // สั่งเซตให้เป็นขาสัญญาณออก

// เซ็ตให้ขาที่ไปควบคุมโมดูลขับให้ถูกต้องก่อน ส่วนนี้สำคัญเพราะจะทำให้โมดูลขับเสียถ้าไม่เซ็ตให้ถูกต้องก่อน 
    digitalWrite(PWM_OUT_UL, HIGH);
    digitalWrite(PWM_OUT_UH, LOW);
    digitalWrite(PWM_OUT_VL, HIGH);
    digitalWrite(PWM_OUT_VH, LOW);
    digitalWrite(PWM_OUT_WL, HIGH);
    digitalWrite(PWM_OUT_WH, LOW);

    pinMode(FAULT_DETECT, INPUT_PULLUP); // สั่งเซตให้เป็นขารับสัญญาณเข้า และ พูลอัพ 
    digitalWrite(enablePin, LOW); // สั่งให้ขานี้ ปิด (เป็น0) เพื่อทำให้ภาค Power Drive อย่าเพิ่งทำงานตอนนี้
    digitalWrite(testPin, LOW); // Clear testPin
//-----------------------------------------------------------------------------

    if (digitalRead(single_3ph) == LOW){single = true;} // ตรวจสอบว่าขาจั้มเปอร์จั้มเป็นซิงเกิลเฟสหรือเปล่า ถ้าจั้มก็บอกว่า จริง ถ้าไม่จ้ัมก็บอกว่า เท็จ

    lcd.begin(16,2); // เริ่มเปิดใช้งานจอแสดงผล LCD
// โหลดข้อมูลตัวอักษรภาษาไทยไปเก็บไว้ในจอ LCD 
    lcd.createChar(0,character1);
    lcd.createChar(1,character2);
    lcd.createChar(2,character3);
    lcd.createChar(3,character4);
    lcd.createChar(4,character5);

    lcd.setCursor(1, 0); // สังให้แสดงผลคอลั่มที่ 5 ในบรรทัดแรก (0)
// lcd.print("DadSeree"); //ถ้าจะใช้ภาษาอังกฤษ

    lcd.write(byte(0)); //ภาษาไทย ไม่เกิน 8 ตัวอักษร
    lcd.write(byte(1));
    lcd.write(byte(1));
    lcd.write(byte(2));
    lcd.write(byte(3));
    lcd.write(byte(4));

    lcd.setCursor(0, 1); // สังให้แสดงผล เวอร์ชั่น วัน เดือน ปี ที่เขียน คอลั่มที่ 1 (0) ในบรรทัดที่ 2 (1)
    for (temp=0; temp <= 7; temp++){
      lcd.print(My_Version[temp]);
      delay(200);
    }

    delay(3000); // หน่วงเวลาให้แสดงผลนี้ เป็นเวลานาน 3 วินาที
    lcd.clear();
//----------------------------------------------------------------------------- 
    if (single == true){ // ถ้าตัวจั้มเปอร์เป็นจริงก็ให้แสดงผล ที่ คอลั่ม 6 บรรทัดแรก J1P = จ่ายไฟแบบซิงเกิลเฟส
      lcd.setCursor(5, 0);
      lcd.print("J1P");
    } 
    else{
      lcd.setCursor(5, 0); // หรือถ้าตัวจั้มเปอร์เป็นเท็จ ก็ให้แสดงผลที่ คอลั่ม 6 บรรทัดแรก J3P = จ่ายไฟแบบ 3 เฟส
      lcd.print("J3P"); 
    }
//------------------ จะหยุดรอที่นี่ถ้าวอลลู่มปรับ Speed น้อยกว่า 18 Hz------------------ 
    do{
      //freq_new = map(analogRead(FREQ_POT), POT_MIN, POT_MAX, FREQ_MIN, FREQ_MAX); //รับค่า Speed จากวอลลุ่ม
      freq_new = map(analogRead(FREQ_POT), POT_MIN, POT_MAX, FREQ_MIN, FREQ_MAX); //รับค่า Speed จากวอลลุ่ม
      
      lcd.setCursor(0, 1); // แสดงผลที่คอลั่ม 1 บรรทัด 2 รอสวิตช์ลูกลอยเปิด WAIT SW
      lcd.print("!WAIT SW");
      delay (1000);
      lcd.setCursor(0, 1); // แสดงผลที่คอลั่ม 1 บรรทัด 2 รอสวิตช์ลูกลอยเปิด SpeedLow
      lcd.print("SpeedLow");
      delay (1000);
    }while (freq_new <= FREQ_MIN); 
//---------------------------------------------------------------------------- 
    lcd.setCursor(0, 1); // แสดงผลที่คอลั่ม 2 บรรทัด 2 คำว่า Frequency รอไว้ก่อนเพื่อให้ค่าจริงมาต่อ
    lcd.print("F:");
//-----------------------------------------------------------------------------
// ไปเรียกโปรแกรมการตั้งค่า ไทม์เเมอร์ เพื่อกำหนดค่าการสร้างสัญญาณ PWM 3 ชุด โดยใช้ตัวไทม์เมอร์ของ Arduino ทั้ง 3 ตัว Setup the timers
    setup_timer0();
    setup_timer1();
    setup_timer2(); 
// ส่วนนี้เป็นการคำนวนตามสูตรเพื่อ ให้ได้สัญญาณซายเวฟครบวงรอบต่อเนื่องต้องใช้ความละเอียดถึง 32 บิต และจะได้ผลลับที่ 8 บิต สูงสุด แต่เวลาใช้งานจะต้องเลื่อนบิตกลับมาใชั 8 บิตล่าง 
//-----------------------------------------------------------------------------
    cbi (TIMSK2,TOIE2); // หยุดการทำงานของไทม์เมอร์ 2 disable timer2 overflow detect
    freq_old = 15; //จะเริ่มครั้งแรกด้วยความถี่ตั้งแต่ 10 ก่อนเพื่อ WarmUp
    freq = 15;
    Amplitude = 60;
    LoadModulateBuffer (Amplitude);
    delta = pow(2,32)*freq/refclk ; 
    sbi (TIMSK2,TOIE2); //เปิดให้ไทม์เมอร์ 2 ทำงาน enable timer2 overflow detect เริ่มสร้างสัญญาณซายเวฟ 10 Hz.
    c4ms = 1;
    while(c4ms);
    c4ms = 1;
    while(c4ms); 
//-----------------------------------------------------------------------------
    attachInterrupt(0, fault, LOW); //เริ่มให้ทำการตรวจการผิดปรกติ กันโหลดเกิน
    //attachInterrupt(0, fault, FALLING); //เริ่มให้ทำการตรวจการผิดปรกติ กันโหลดเกิน
    freq_new = map(analogRead(FREQ_POT), POT_MIN, POT_MAX, FREQ_MIN, FREQ_MAX); //รับค่า Speed จากวอลลุ่ม
    
    changeFreq_ramp(freq_new); //ให้ค่อยๆไต่ระดับขึ้นไป
}
//----------------------------------------------------------------------------
//------------------------------- ทำงาน วน อยู่ในส่วนนี้เท่านั้น ---------------------
void loop(){ 
    if (testPin_status){ //ในช่วงที่ หลอด LED ติด จะเช็คว่ามีการเปลี่ยน Speed หรือไม่
      freq_new = map(analogRead(FREQ_POT), POT_MIN, POT_MAX, FREQ_MIN, FREQ_MAX); //รับค่า Speed จากวอลลุ่ม
      Serial.println(freq_new);
      if (freq_new != freq_old){ //ถ้าความถี่ใหม่ไม่เท่ากับความถี่เดิมแสดงว่าต้องการเปลี่ยน Speed ใหม่
        changeFreq(freq_new);
      } 
    }
//--------- ถ้าวอลลู่มปรับลงต่ำกว่า 18 Hz หรือสวิตช์ลูกลอยตัดอยู่ ให้หยุดเริ่มใหม่ ----------- 
    if (freq_new <= FREQ_MIN){
      delay(1000); // wait 1 second
      resetSoftware(); // RESET! เริ่มต้นใหม่
    }
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// ชุดคำสั่งในการเปลี่ยนความถี่
void changeFreq(float _freq){
    temp = _freq;
    Amplitude = temp * 2 + 40;
    if (Amplitude >= MAX_AMPLITUDE) Amplitude = MAX_AMPLITUDE;
    lcd.setCursor(0, 0);
    lcd.print(Amplitude + 96);
    lcd.print("V "); 
    lcd.setCursor(2, 1);
    lcd.print(temp);
    lcd.print(" Hz.");
    freq = _freq;
    LoadModulateBuffer (Amplitude); 
    delta=pow(2,32)*freq/refclk; // อัพเดทความถี่ใหม่ update phase increment
    c4ms = 1;
    while(c4ms);
    freq_old = _freq; 
} 
//----------------------------------------------------------------------------
// ชุดคำสั่งในการเปลี่ยนความถี่ แบบไต่ระดับ RAMP
void changeFreq_ramp(float _freq){
  if (_freq > freq_old){ 
    for (temp = freq_old; temp <= _freq; temp++){
      Amplitude = temp * 2 + 40;
      if (Amplitude >= MAX_AMPLITUDE) Amplitude = MAX_AMPLITUDE; 
      lcd.setCursor(0, 0);
      lcd.print(Amplitude + 96);
      lcd.print("V ");
      lcd.setCursor(2, 1);
      lcd.print(temp);
      lcd.print(" Hz.");
      freq = temp;
      LoadModulateBuffer (Amplitude); 
      delta=pow(2,32)*freq/refclk; // อัพเดทความถี่ใหม่ update phase increment
      c4ms = 1;
      while(c4ms); 
    } 
  }
  freq_old = _freq;
} 
//------------------------------------------------------------------------------
// ชุดคำสั่งในการตั้งค่า ไทม์เมอร์ ตัวแรก (0)
// timer0 setup
// set prescaler to 8, PWM mode to phase correct PWM, 16000000/512/8 = 3.9kHz clock
void setup_timer0(void){
// Timer0 Clock Prescaler to :ตั้งค่าไทม์เมอร์ให้ หารอีก 8 เพื่อจะได้ความถี่ PWM ขนาด 3.9 Khz ให้เหมาะกับ IPM เบอร์เก่า ๆ
  cbi (TCCR0B, CS00);
  sbi (TCCR0B, CS01);
  cbi (TCCR0B, CS02);

// Timer0 PWM Mode set to Phase Correct PWM
  cbi (TCCR0A, COM0B0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง แรก (OCR0B) ให้ส่งสัญญาณออกเป็น Sine PWM ปรกติ +
  sbi (TCCR0A, COM0B1);
  sbi (TCCR0A, COM0A0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง แรก (OCR0A) ให้ส่งสัญญาณออกเป็น Sine PWM กลับเฟส Invert เป็น -
  sbi (TCCR0A, COM0A1);
  sbi (TCCR0A, WGM00); //ตั้งค่าให้สัญญาณพัลซ์ทุกลูกอยู่แนวเดียวกันเสมอในทุกเฟส (Mode 1 / Phase Correct PWM)
  cbi (TCCR0A, WGM01);
  cbi (TCCR0B, WGM02);
}

//------------------------------------------------------------------------------
// timer1 setup
// set prescaler to 8, PWM mode to phase correct PWM, 16000000/512/8 = 3.9kHz clock ให้เหมาะกับ IPM เบอร์เก่า ๆ
void setup_timer1(void){
// Timer1 Clock Prescaler to :ตั้งค่าไทม์เมอร์ให้ หารอีก 8 เพื่อจะได้ความถี่ PWM ขนาด 3.9 Khz 
  cbi (TCCR1B, CS10);
  sbi (TCCR1B, CS11);
  cbi (TCCR1B, CS12);

// Timer1 PWM Mode set to Phase Correct PWM
  cbi (TCCR1A, COM1B0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง 2(OCR1B) ให้ส่งสัญญาณออกเป็น Sine PWM ปรกติ +
  sbi (TCCR1A, COM1B1);
  sbi (TCCR1A, COM1A0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง 2(OCR1A) ให้ส่งสัญญาณออกเป็น Sine PWM กลับเฟส Invert เป็น -
  sbi (TCCR1A, COM1A1);
  sbi (TCCR1A, WGM10); //ตั้งค่าให้สัญญาณพัลซ์ทุกลูกอยู่แนวเดียวกันเสมอในทุกเฟส (Mode 1 / Phase Correct PWM)
  cbi (TCCR1A, WGM11);
  cbi (TCCR1B, WGM12);
  cbi (TCCR1B, WGM13);
}

//------------------------------------------------------------------------------
// timer2 setup
// set prescaler to 8, PWM mode to phase correct PWM, 16000000/512/8 = 3.9kHz clock ให้เหมาะกับ IPM เบอร์เก่า ๆ
void setup_timer2(void){
// Timer1 Clock Prescaler to :ตั้งค่าไทม์เมอร์ให้ หารอีก 8 เพื่อจะได้ความถี่ PWM ขนาด 3.9 Khz 
// Timer2 Clock Prescaler to : /8
  cbi (TCCR2B, CS20);
  sbi (TCCR2B, CS21);
  cbi (TCCR2B, CS22);

// Timer2 PWM Mode set to Phase Correct PWM
  cbi (TCCR2A, COM2B0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง 3(OCR2B) ให้ส่งสัญญาณออกเป็น Sine PWM ปรกติ +
  sbi (TCCR2A, COM2B1);
  sbi (TCCR2A, COM2A0); // ตั้งค่าให้ชุดส่งสัญญาณช่อง 3(OCR2A) ให้ส่งสัญญาณออกเป็น Sine PWM กลับเฟส Invert เป็น -
  sbi (TCCR2A, COM2A1);
  sbi (TCCR2A, WGM20); //ตั้งค่าให้สัญญาณพัลซ์ทุกลูกอยู่แนวเดียวกันเสมอในทุกเฟส (Mode 1 / Phase Correct PWM)
  cbi (TCCR2A, WGM21);
  cbi (TCCR2B, WGM22);
}

//------------------------------------------------------------------------------
// ส่งInterrupt Service Routine attached to INT0 vector
void fault(){
  digitalWrite(enablePin, LOW); // สั่งให้ขานี้ ปิด (เป็น0) เพื่อทำให้ภาค Power Drive อย่าเพิ่งทำงานตอนนี้
  noInterrupts();
  cbi (TIMSK2,TOIE2); // หยุดการทำงานของไทม์เมอร์ 2 disable timer2 overflow detect
  digitalWrite(PWM_OUT_UL, HIGH);
  digitalWrite(PWM_OUT_UH, LOW);
  digitalWrite(PWM_OUT_VL, HIGH);
  digitalWrite(PWM_OUT_VH, LOW);
  digitalWrite(PWM_OUT_WL, HIGH);
  digitalWrite(PWM_OUT_WH, LOW);
  lcd.clear();
  lcd.setCursor(0, 0); // สังให้แสดงผลคอลั่มที่ 1 ในบรรทัดแรก (0) 
  lcd.print("OverLoad"); // แสดงผล Error
  lcd.setCursor(0, 1); // สังให้แสดงผลคอลั่มที่ 1 ในบรรทัด 2 (1) 
  lcd.print("ResetOFF");
  delay(5000); // wait 5 second
  lcd.setCursor(0, 0); // สังให้แสดงผลคอลั่มที่ 1 ในบรรทัดแรก (0) 
  lcd.print("OverLoad"); // แสดงผล Error
  lcd.setCursor(0, 1); // สังให้แสดงผลคอลั่มที่ 1 ในบรรทัด 2 (1) 
  lcd.print("Reseting");
  delay(5000); // wait 5 second
  resetSoftware(); // RESET! เริ่มต้นใหม่
  //while (1);
}
//------------------------------------------------------------------------------

//ISR(TIMER2_OVF_vect) {
//// สูตรการคำนวน ความถี่กลับมา แล้วเลื่อน จาก 32 บิต กลับมาใช้เพียง 8 บิตล่าง 
//sigma = sigma+delta; // soft DDS, phase accu with 32 bits
//phase0 = sigma >> 25; // use upper 8 bits for phase accu as frequency information
//
////ชุดคำสั่งนี้แบ่งเป็น 2 ส่วน ชุดแรกเพื่อ ส่งสัญญาณ ซายเวฟ ทั้ง 3 คู่ ให้มีเฟสต่างกัน 0-90-180 เพื่อใช้กับมอร์เตอร์ซิงเกิ้ลเฟส และ ชุดต่อมา สำหรับ 3 เฟส 0-120-120 
//  if (single == true){ // ถ้าจั้มเปอร์เป็นจริง ให้ทำงานส่วนนี้ คือแบบซิงเกิลเฟส
//    phase1 = (phase0 + 64) & 0b01111111;
//    phase2 = (phase0 + 32) & 0b01111111;
//
//    temp_ocr = Table_Buffer[phase1];
//    OCR0A = temp_ocr + DT; // pin D6
//    OCR0B = temp_ocr; // pin D5
//
//    temp_ocr = Table_Buffer[phase0];
//    OCR1A = temp_ocr + DT; // pin D9
//    OCR1B = temp_ocr; // pin D10
//
//    temp_ocr = Table_Buffer[phase2];
//    OCR2A = temp_ocr + DT; // pin D11
//    OCR2B = temp_ocr; // pin D3 
//  } 
////---------------------- ถ้าจั้มเปอร์เป็นเท็จ ให้ทำงานส่วนนี้ คือแบบ 3 เฟส ที่มีเฟสต่างกัน 120 องศา------------------------------------------------------------------
//  else{
//    phase1 = (phase0 + 42) & 0b01111111;
//    phase2 = (phase0 + 84) & 0b01111111;
//
//    temp_ocr = Table_Buffer[phase0];
//    OCR0A = temp_ocr + DT; // pin D6
//    OCR0B = temp_ocr; // pin D5
//
//    temp_ocr = Table_Buffer[phase1];
//    OCR1A = temp_ocr + DT; // pin D9
//    OCR1B = temp_ocr; // pin D10
//
//    temp_ocr = Table_Buffer[phase2];
//    OCR2A = temp_ocr + DT; // pin D11
//    OCR2B = temp_ocr; // pin D3 
//  }
//
//  if ((phase0 == 64) && (OPTO_EN = true)){
//       digitalWrite(enablePin, HIGH); // เปิด ภาค Power Drive ให้เริ่มทำงานได้
//       OPTO_EN = false;
//  }
//    if (c4ms++ == 510){
//      c4ms = 0;
//      digitalWrite(testPin, testPin_status);
//      testPin_status = !testPin_status; 
//    }   
//}
//--------------------------------------------------------------------------------------------------------------------
void LoadModulateBuffer(byte V_level) {
byte cout;
signed char p, n;
double angle;
if (V_level >= MAX_AMPLITUDE)V_level = MAX_AMPLITUDE;
  p = V_level + 1;
  n = +V_level;
  for (cout = 0; cout < 128; cout++) {
    angle = cout * M_PI / 64;
    Table_Buffer[cout] = p + n * sin(angle);
  }
}
//จบโปรแกรมแล้วครับใครจะเขียนเพิ่มเติมอะไรได้ครับ แต่อย่าลืมให้เครดิตผู้ที่เขียนต้นฉบับด้วยครับ
