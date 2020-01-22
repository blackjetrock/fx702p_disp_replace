// include the library code:
#include <LiquidCrystal.h>
// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = PA_1, en = PA_0, d4 = PA_4, d5 = PA_3, d6 = PA_2, d7 = PA_1;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  // put your setup code here, to run once:
pinMode(PA_4, OUTPUT);
Serial.begin(9600);
while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
 
}

int i;

void loop() {
Serial.println("CQ1 Re-engineer");

  // put your main code here, to run repeatedly:
digitalWrite(PA_4, HIGH);
delay(100);
digitalWrite(PA_4, LOW);
delay(100);
digitalWrite(PA_4, HIGH);
delay(1000);
digitalWrite(PA_4, LOW);
delay(1000);
digitalWrite(PA_4, HIGH);
delay(200);
digitalWrite(PA_4, LOW);
delay(200);

i+=50;

if( i> 300 )
{
i=10;
}
 // Print a message to the LCD.
  lcd.print("hello, world!");
}
