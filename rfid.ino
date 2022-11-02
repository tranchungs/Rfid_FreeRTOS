#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

const byte ROWS = 4;  //four rows
const byte COLS = 3;  //three columns
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};                                     
byte rowPins[ROWS] = { A0, 2, 3, 4 };  
byte colPins[COLS] = { 6, 7, 8 };      
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char key_code[3];
char new_pass[3];
char password[3] = { '1', '1', '1' };  // Initial Password


int card1[4]{ 48, 67, 3, 32 };  // UID thẻ từ được nhận diện
int card2[4]{ 188, 186, 8, 50 };


constexpr uint8_t RST_PIN = 9;  // Define SS_PIN of the RC522 RFID Reader to digital pin 10 of the Arduino
constexpr uint8_t SS_PIN = 10;  // Define RST_PIN of the RC522 RFID Reader to digital pin 9 of the Arduino
MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

Servo myservo;  // khoi tao servo
int pos = 0;    // bien save vi tri servo

int Buzzer = A1;    // đầu ra bao hieu
int RedLed = A2;    // đầu ra leb red
int GreenLed = A3;  // đầu ra lab green


unsigned int index = 0;
unsigned int index2 = 0;
byte nuidPICC[4];  // UID number of your Mifare card. This is a UID number with 4 pairs of digits. Example: EB 70 C0 BC

TaskHandle_t HandleInputTask;
TaskHandle_t StartProgramTask;
QueueHandle_t commandQueue;
void setup() {
  Serial.begin(9600);
  pinMode(RedLed, OUTPUT);    // Define RedLed as OUTPUT
  pinMode(GreenLed, OUTPUT);  // Define GreenLed as OUTPUT
  pinMode(Buzzer, OUTPUT);    // Define Buzzer as OUTPUT
  commandQueue = xQueueCreate(1, sizeof(char));
  myservo.attach(5);  // attaches the servo on pin 5 to the servo object
  SPI.begin();        // Init SPI bus
  rfid.PCD_Init();    // Init MFRC522
  // =====================================================
  lcd.begin();  // Khoi tao LCD
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  RFID & KEYPAD ");
  lcd.setCursor(0, 1);
  lcd.print("  Lock Project  ");
  // lcd.clear();
  // ====================================================
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;  // Mảng chứa UID của RFID
  }
  myservo.write(180);
  xTaskCreate(HandleInput, "HandleInput", 128, NULL, 1, &HandleInputTask);
  xTaskCreate(StartProgram, "StartProgram", 128, NULL, 1, &StartProgramTask);
  vTaskStartScheduler();
}
void loop() {
}

void StartProgram(void *pvParameters) {
  while (1) {
    if (commandQueue != 0) {
      char key;
      if (xQueueReceive(commandQueue, &key, (TickType_t)0)) {
        if (key == '*') {
          vTaskPrioritySet(StartProgramTask, 3);
          OnChangePass();
          vTaskPrioritySet(StartProgramTask, 1);
        } else {
          vTaskPrioritySet(StartProgramTask, 3);
          OnCheckPass(key);
          vTaskPrioritySet(StartProgramTask, 1);
        }
      }
    }
  }
}

void HandleInput(void *pvParameters) {
  for (;;) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      Serial.println(key);
      xQueueOverwrite(commandQueue, &key);
    }
    if (rfid.PICC_IsNewCardPresent()) { //handle Card
      if (rfid.PICC_ReadCardSerial()) {
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        Serial.println(rfid.PICC_GetTypeName(piccType));
        for (byte z = 0; z < 4; z++) {
          nuidPICC[z] = rfid.uid.uidByte[z];
          Serial.println(nuidPICC[z]);
        }
        if (card2[0] == nuidPICC[0] && card2[1] == nuidPICC[1] && card2[2] == nuidPICC[2] && card2[3] == nuidPICC[3]) {
          Unlock();
        } else {
          NotUnlock();
        }
      }
    }
  }
}


void Unlock() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Open door");
  digitalWrite(GreenLed, HIGH);
  delay(300);
  tone(Buzzer, 3000);
  delay(500);
  noTone(Buzzer);
  delay(100);
  tone(Buzzer, 3000);
  delay(500);
  noTone(Buzzer);
  delay(100);
  digitalWrite(GreenLed, LOW);
  for (int z = 180; z > 110; z -= 5) {
    myservo.write(z);
    delay(300);
  }
  delay(4000);
  myservo.write(180);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input Pass | Scan Card");
}
void NotUnlock() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong Pass");
  digitalWrite(RedLed, HIGH);
  delay(200);
  tone(Buzzer, 3000);
  delay(2000);
  noTone(Buzzer);
  digitalWrite(RedLed, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input Pass | Scan Card");
}
void OnChangePass() {
  lcd.clear();
  lcd.print("Input old pass:");
  lcd.setCursor(0, 1);
  while (index < 3) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      key_code[index++] = key;
      lcd.print(String(key));
    }
  }
  if (index == 3) {
    if (!strncmp(password, key_code, 3)) {  // if pass true => unlock else notUnlock
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Input new pass:");
      lcd.setCursor(0, 1);
      while (index2 < 3) {
        char key = keypad.getKey();
        if (key != NO_KEY) {
          new_pass[index2++] = key;
          lcd.print(String(key));
        }
      }
      if (index2 == 3) {
        password[0] = new_pass[0];
        password[1] = new_pass[1];
        password[2] = new_pass[2];
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Success");
        index2 = 0;
      }
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong pass");
    }
    index = 0;  //reset index=0
  }
}
void OnCheckPass(char p) {
  key_code[index++] = p;
  if (index == 3) {
    if (!strncmp(password, key_code, 3)) {  // if pass true => unlock else notUnlock
      Unlock();
    } else {
      NotUnlock();
    }
    index = 0;  //reset index=0
  }
}