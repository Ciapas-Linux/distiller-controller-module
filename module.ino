
// ------------------------------|
// Modół roszerzenia 				 |
// --------------------------------------------------------------------------------------------|
// 3 PINy     3x DS18b20 z możliwością przypisania nazwy wyświetlanego trmometru   |
// 2 I2c        Czujnik atmosferyczny (balans temperatury dnia) + Beczki			              |
// 1 PIN       Czujnik zalania																						  |
// 3 PINy     Obsługa silnika krokowego																		  |			
// 1 PIN       Obsługa mocy z płynną regulacją 0 - 100%												  |		
// 2 PINy     EZ Płukanie OLM, Przedgon																		  |		
// --------------------------------------------------------------------------------------------|


// czujnik ciśnienia:  119 (0x77)
//  D0    = 16;
//  D1    = 5;
//  D2    = 4;
//  D3    = 0;
//  D4    = 2;
//  D5    = 14;
//  D6    = 12;
//  D7    = 13;
//  D8    = 15;
//  D9    = 3;
//  D10  = 1;


#include <EasyTransfer.h>
#include "OneWire.h" 
#include "Wire.h"            
#include "DallasTemperature.h" 
#include "Adafruit_BMP085.h" 

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Ticker.h> 
#include <time.h>
#include <vector>
#include <list>
#include <string>


// Update Webpage files:
//#include "static/index.html.gz.h"
#include "static/upload.html.gz.h"


#define DIODA    D8  // Z sn
 // PRZEDGON I POGON Z sn   jak zamkniety to stan wysoki:  pogon przedgon tosamo
#define EZ_GLOWICA  D3 
#define EZ_PLUK_OLM   D4  // Z sn    ????????? co to jest DIODA ?


#define DS_1_WODA  D7  // woda
#define DS_2_BUFOR  D6  // bufor
#define DS_3_WOLNY  D5  // wolny

// #define DS_1_WODA  D5  // woda
// #define DS_2_BUFOR  D6  // bufor
// #define DS_3_WOLNY  D7  // wolny

#define MC_OK 239 
#define MC_DS_ERROR 10
#define MC_EZ_GLOWICA_ON 11
#define MC_EZ_GLOWICA_OFF 12
#define MC_EZ_PLUK_OLM_ON 13
#define MC_EZ_PLUK_OLM_OFF 14
#define MC_UPDATE_ENABLE 15

char TempCharBuffer[12] = "0000000000\0"; // "-2147483648\0"
AsyncWebServer server(80);

bool DS18b20_1   	 = false;
bool DS18b20_2   	  = false;
bool DS18b20_3   	  = false;
bool AWARIA_DS       = false;
bool AP_CONNECTED = false;
uint8_t 		ipap1 = 0;
uint8_t 		ipap2 = 0;
uint8_t 		ipap3 = 0;
uint8_t 		ipap4 = 0;
bool shouldReboot = false;

struct MODULE_ERROR
{
	  bool DS1_error = true;
	  bool DS2_error = true;
	  bool DS3_error = true;
	  bool I2C_error  = true;
}ModError;

OneWire ds_1(DS_1_WODA); 
OneWire ds_2(DS_2_BUFOR);  
OneWire ds_3(DS_3_WOLNY);  
DallasTemperature sensor_1(&ds_1);
DallasTemperature sensor_2(&ds_2);
DallasTemperature sensor_3(&ds_3);

extern "C"
{ 
	#include "user_interface.h"
    #include "ets_sys.h"
    #include "osapi.h"
    #include "os_type.h"
}

//create two RX-/-TX objects
EasyTransfer ETin, ETout; 


// TIMER
bool Timer1_Occured = false;
os_timer_t  Timer1;

bool Timer2_Occured = false;
os_timer_t  Timer2;

bool Timer3_Occured = false;
os_timer_t  Timer3;

// MCU LIVE:
bool MCU_LIVE = false;
long MCU_Cntr = 0;
long last_MCU_Cntr = 0;

//UPDATE:
const size_t MAX_FILESIZE = 1024 * 1024 * 10; //15728640   --> 540672
static const char* MIMETYPE_HTML{"text/html"};

// communication esp8266 to another esp8266
// RX / TX receive&send structures
struct RECEIVE_DATA_STRUCTURE
{
    int action_code;
}receive_data;

struct SEND_DATA_STRUCTURE
{
  int action_code;
  float 	      DS_1_WODA 	     = 00.00;
  float 	      DS_2_BUFOR 	     = 00.00;
  float 	      DS_3_WOLNY	     = 00.00;	
  float         gTempBMP      	 = 00.00;
  int32_t     gPresureBMP 		 = 0;
  int 			  AnalogSensorValue = 0;
}send_data;

//I2C sensor pomiaru ciśnienia 
byte bmp180_addr = 0;
Adafruit_BMP085 bmp;

const int analogInPin = A0;

/* format bytes as KB, MB or GB string */
String humanReadableSize(const size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
    else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}


void SetValveON(int valve)
{
	digitalWrite(valve, LOW);
	Serial.println(F("Zawor ON"));
return;
}

void SetValveOFF(int valve)
{
	digitalWrite(valve, HIGH);
	Serial.println(F("Zawor OFF"));
return;
}

void SetDiodeOFF()
{
	digitalWrite(DIODA, LOW);
	Serial.println(F("Dioda OFF"));
return;
}

void SetDiodeON()
{
	digitalWrite(DIODA, HIGH);
	Serial.println(F("Dioda ON"));
return;
}

void TimerCallback_1(void *pArg) 
{
   Timer1_Occured = true;
}

void TimerCallback_2(void *pArg) 
{
		Timer2_Occured = true;
}

void TimerCallback_3(void *pArg) 
{
		Timer3_Occured = true;
}

void Timer_update()
{
   if (Timer1_Occured == true) // akt temp timer 200 ms
  {
		Timer1_Occured = false;
		Akt_Temp();
  } 
  
   if (Timer2_Occured == true) // 10 second MCU timer
  {
		Timer2_Occured = false;
		if(MCU_Cntr > last_MCU_Cntr)
		{
			SetDiodeON();
		}else
		{
			SetDiodeOFF();
			MCU_LIVE = false;
		}
		last_MCU_Cntr = MCU_Cntr;
  } 
  
   if (Timer3_Occured == true) // MCU timer long live
  {
		if(MCU_LIVE == false)
		{
			SetValveOFF(EZ_GLOWICA);
			SetValveOFF(EZ_PLUK_OLM);
			Serial.println("MCU is dead !!!"); 
		}
			
  } 
  
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
		static unsigned long startTimer;
        if (!index)
        {
			// disable interrupt timer
			// setup_timer1_disable();
			
            startTimer = millis();
            const char* FILESIZE_HEADER{"FileSize"};

            Serial.printf("UPLOAD: Receiving: '%s'\n", filename.c_str());

            if (!request->hasHeader(FILESIZE_HEADER))
            {
                request->send(400, MIMETYPE_HTML, "No filesize header present!");
                request->client()->close();
                Serial.printf("UPLOAD: Aborted upload because missing filesize header.\n");
                return;
            }

            Serial.printf("UPLOAD: fileSize: %s\n", request->header(FILESIZE_HEADER));

            if (request->header(FILESIZE_HEADER).toInt() >= MAX_FILESIZE)
            {
                request->send(400, MIMETYPE_HTML,
                              "Too large. (" + humanReadableSize(request->header(FILESIZE_HEADER).toInt()) +
                              ") Max size is " + humanReadableSize(MAX_FILESIZE) + ".");

                request->client()->close();
                Serial.printf("UPLOAD: Aborted upload because filesize limit.\n");
                return;
            }
            
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            Update.runAsync(true);
            if (!Update.begin(maxSketchSpace))
            {
				Update.printError(Serial);
			}
    
        }

        //Store or do something with the data...
        //Serial.printf("file: '%s' received %i bytes\ttotal: %i\n", filename.c_str(), len, index + len);
        
         if(!Update.hasError())
		{
			  if(Update.write(data, len) != len)
			  {
					Update.printError(Serial);
			  }
		}
        
        

        if (final)
        {
            Serial.printf("UPLOAD: Done. Received %i bytes in %.2fs which is %.2f kB/s.\n",
                          index + len,
                          (millis() - startTimer) / 1000.0,
                          1.0 * (index + len) / (millis() - startTimer));
                          
                           if(Update.end(true))
						  {
							  Serial.printf("Udało się: %u bytes\n", index+len);
							  Serial.print(F("OK: MCU restarting now ..."));         
							  ESP.restart();
						  } else
						  {
							  Update.printError(Serial);
						  }
      
        }                

}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
} 

void onUpload(AsyncWebServerRequest *request)
{
	 AsyncWebServerResponse *response = request->beginResponse_P(200, MIMETYPE_HTML, upload_html_gz, upload_html_gz_len);
     response->addHeader("Content-Encoding", "gzip");
     request->send(response);
}

void setup_SERVER()
{
    
   server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
    {
		    request->send(200);
	},
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,size_t len, bool final)
          {
				handleUpload(request, filename, index, data, len, final);
		  }
	); 
    
   server.on("/", HTTP_OPTIONS, [](AsyncWebServerRequest * request)
    {
        AsyncWebServerResponse* response = request->beginResponse(204);
        response->addHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization, FileSize");
        response->addHeader("Access-Control-Allow-Credentials", "true");
        request->send(response);
    }); 
     
  
  Serial.println("");
  Serial.print(F("Uruchomiono server aktualizacji... "));
  Serial.println("");
  
  server.on("/", HTTP_GET, onUpload);      
  server.onNotFound(notFound);
  server.begin();
  
  Serial.println(""); 
  Serial.println(F("ASYNC HTTP server started"));
  Serial.println(""); 
  
}


void Akt_Temp() 
{
			send_data.DS_1_WODA = sensor_1.getTempCByIndex(0);
			send_data.DS_2_BUFOR = sensor_2.getTempCByIndex(0);
			send_data.DS_3_WOLNY = sensor_3.getTempCByIndex(0);
						          
             // sprawdzenie na sytuację awaria ds
             if( (send_data.DS_1_WODA == 0 ||  send_data.DS_1_WODA == -127.0) )
			 {
				AWARIA_DS = true;
			 }else
			 {
				AWARIA_DS = false;
			 } 
			 if( (send_data.DS_2_BUFOR == 0 ||  send_data.DS_2_BUFOR == -127.0)  )
			 {
				AWARIA_DS = true;
			 }else
			 {
				AWARIA_DS = false;
			 }  
		   	 if( (send_data.DS_3_WOLNY == 0 ||  send_data.DS_3_WOLNY == -127.0) )
			 {
				AWARIA_DS = true;
			 }else
			 {
				AWARIA_DS = false;
			 }  
                                               
            sensor_1.requestTemperaturesnodelay();
            sensor_2.requestTemperaturesnodelay();
            sensor_3.requestTemperaturesnodelay();
            
            send_data.AnalogSensorValue = analogRead(analogInPin);
            
            send_data.gTempBMP = 0;
            send_data.gTempBMP = bmp.readTemperature();
            send_data.gPresureBMP = bmp.readPressure();
 
}

void setup_I2C()
{
  Serial.println ();
  Serial.println (F("SETUP: I2C scanner. Scanning ..."));
  byte count = 0;
  Serial.println(F("SETUP: Init I2C on ESP8266_NODEMCU"));
  // Wire.begin(SDA,SCL);       ESP Default ist  D2 = 4 = SDA ; D1 = 5 = SCL
  Wire.begin();
  //Wire.begin(SDA,SCL); 
  for (byte i = 8; i < 127; i++)
  {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
      {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      
       if( i == 119) 
      {
		  bmp180_addr = i;
		  break;
	  }
      
      count++;
      delay (1);  
      } 
  } 
  
  Serial.println (F("Done."));
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");
  
  if(count == 0)
  {
		ModError.I2C_error = true;
		Serial.println ("Brak urządzeń I2C ...");
  }
  
  if(bmp180_addr == 119)
  {
	  if (!bmp.begin())
	  {
	      Serial.println("Could not find a valid BMP085 sensor, check wiring!");
      }else
      {
		  Serial.println("Znaleziono czujnik ciśnienia  BMP180 !");
	  }
  }
}

void setup_DS()
{
	//TERMOMETRY
  delay(2000);
  
  sensor_1.begin();
  if(sensor_1.getDeviceCount() != 1)
  {
	Serial.println(F("SETUP: Czujnik temperatury nr: 1 nie podlaczony !!!"));
	ModError.DS1_error = true;
	DS18b20_1 = false;
    delay(1500);  
  }else
  {
	 Serial.println(F("SETUP: Czujnik temperatury nr: 1 OK"));
	 DS18b20_1 = true;
	 ModError.DS1_error = false;
     delay(1500); 
  }
  
  sensor_2.begin();
  if(sensor_2.getDeviceCount() != 1)
  {
	Serial.println(F("SETUP: Czujnik temperatury nr: 2 nie podlaczony !!!"));
	DS18b20_2 = false;
	ModError.DS2_error = true;
    delay(1500);   
  }else
  {
	Serial.println(F("SETUP: Czujnik temperatury nr: 2 OK"));
	DS18b20_2 = true;
	ModError.DS2_error = false;
    delay(1500);   
  }
  
  sensor_3.begin();
  if(sensor_3.getDeviceCount() != 1)
  {
	Serial.println(F("SETUP: Czujnik temperatury nr: 3 nie podlaczony !!!"));
	DS18b20_3 = false;
	ModError.DS3_error = true;
    delay(1500);    
  }else
  {
	Serial.println(F("SETUP: Czujnik temperatury nr: 3 OK"));
	DS18b20_3 = true;
	ModError.DS3_error = false;
    delay(1500);   
  }
  
  //GET TERMO & SETUP
  sensor_1.setWaitForConversion(false);
  sensor_1.requestTemperaturesnodelay();
  sensor_1.setResolution(12);
  sensor_2.setWaitForConversion(false);
  sensor_2.requestTemperaturesnodelay();
  sensor_2.setResolution(12);
  sensor_3.setWaitForConversion(false);
  sensor_3.requestTemperaturesnodelay();
  sensor_3.setResolution(12);
  
  Serial.println(F(""));
  
  Serial.println("");
  Serial.print(F("SETUP: Dodatkowy test czujników temperatury >>>"));
  Serial.println(""); 

   String sTmpStr;    
    
  
  bool test_ds1 = false;
  bool test_ds2 = false;
  bool test_ds3 = false;
  
  
  for(int cnt = 0; cnt < 12;cnt++)
  {
      sensor_1.requestTemperatures();
      sensor_2.requestTemperatures();
      sensor_3.requestTemperatures();
      
      delay(100);  
      
      send_data.DS_1_WODA = sensor_1.getTempCByIndex(0);
	  send_data.DS_2_BUFOR = sensor_2.getTempCByIndex(0);
	  send_data.DS_3_WOLNY = sensor_3.getTempCByIndex(0);
	  
	                  
      Serial.println("."); 
      Serial.println(send_data.DS_1_WODA);
      Serial.println(send_data.DS_2_BUFOR);
      Serial.println(send_data.DS_3_WOLNY);
 
  }
      
   
     if( (send_data.DS_1_WODA == 0 ||  send_data.DS_1_WODA == -127.0) )
     {
      test_ds1 = false;
     }else
     {
	  test_ds1 = true;
	 } 
     
     if( (send_data.DS_2_BUFOR == 0 ||  send_data.DS_2_BUFOR == -127.0)  )
     {
      test_ds2 = false;
     }else
     {
	  test_ds2 = true;
	 }  
   
     if( (send_data.DS_3_WOLNY == 0 ||  send_data.DS_3_WOLNY == -127.0) )
     {
      test_ds3 = false;
     }else
     {
	  test_ds3 = true;
	 }  
   
  if( (test_ds1 == true) && (test_ds3) == true && (test_ds3 == true) )
  {
  
	  Serial.println("");
	  Serial.print(F("SETUP: Test OK"));
	  Serial.println("");
	  delay(500);
  
  }else
  {
      Serial.println("");
      Serial.print(F("SETUP: DS18B20: Test FAIL"));
      Serial.println("");
    
    AWARIA_DS = true;
   
   
     if(test_ds1 == false)
     {
       Serial.println("");
       Serial.print(F("DS1: Test error"));
       ModError.DS1_error = true;
       delay(2000);
     }
      if(test_ds2 == false)
     {
       Serial.println("");
       Serial.print(F("DS2: Test error"));
       ModError.DS2_error = true;
       delay(2000);
     }
      if(test_ds3 == false)
     {
       Serial.println("");
       Serial.print(F("DS3: Test error"));
       ModError.DS3_error = true;
       delay(2000);
     }
       
  }
}

void setup_PINS()
{
   Serial.println("");
   Serial.println(F("SETUP:  set initial pin modes . . ."));	
   
// #define DIODA    D8  // ZSW

// PRZEDGON I POGON ZSN  jak zamkniety to stan wysoki:  pogon przedgon tosamo
// #define EZ_GLOWICA  D3 
// #define EZ_PLUK_OLM   D4  // ZSN    
   
      
   pinMode(DIODA , OUTPUT);  // D8 ZSW na pająku dioda od prawej
   digitalWrite( DIODA  , LOW);  // wyłączmy na starcie
   //SetValveOFF(DIODA);  
   
   pinMode(EZ_GLOWICA , OUTPUT);  // D3 ZSN Wyłączamy na starcie
   digitalWrite(EZ_GLOWICA  , HIGH); 
   //SetValveOFF(EZ_GLOWICA);      
   
   pinMode( EZ_PLUK_OLM	, OUTPUT);  // D4 ZSN Wyłączamy na starcie
   digitalWrite( EZ_PLUK_OLM , HIGH);  
   
}

void setup_RXTX()
{
  Serial.println("");
  Serial.println(F("SETUP:  start RXTX comunication . . ."));		
  ETin.begin(details(receive_data), &Serial);
  ETout.begin(details(send_data), &Serial);
}

void setup_TIMERS()
{
  Serial.println("");
  Serial.println(F("SETUP:  start timers . . ."));	
  
  // Akt temp
  Timer1_Occured = false;
  os_timer_setfn(&Timer1, TimerCallback_1, NULL);
  os_timer_arm(&Timer1, 200, true);
  
  // MCU Live
  Timer2_Occured = false;
  os_timer_setfn(&Timer2, TimerCallback_2, NULL);
  os_timer_arm(&Timer2, 10000, true);
  
  // MCU Live long 5 minuten
  Timer3_Occured = false;
  os_timer_setfn(&Timer3, TimerCallback_3, NULL);
  os_timer_arm(&Timer3, 60*1000*5, true);
}

void setup_STATUS()
{
	
   Serial.println(F(""));  
   Serial.print(F("Free Memory: "));
   Serial.print(system_get_free_heap_size());
   Serial.println(F("")); 
   Serial.println(F("")); 
  
   Serial.println(F("")); 
   Serial.print(F("Flash Memory size: "));
   Serial.print(getFlashChipRealSize());
   Serial.println(F("")); 
   Serial.println(F("")); 
   
   Serial.print(F("ESP CPU freq: "));
   Serial.println(system_get_cpu_freq());
   Serial.println(F("")); 
	
   if(AWARIA_DS == false)
  {
	   Serial.println("");
	   Serial.println(F("SETUP: OK: Moduł rozszeżenia gotowy do roboty !!!"));
	   Serial.println("");
	   Serial.println("");
  }else
  {
	   Serial.println("");
	   Serial.println(F("SETUP: UWAGA ERROR: Moduł rozszeżenia nie jest gotowy do roboty !!!"));
	   Serial.println("");
	   Serial.println("");
  }
}

 
void setup_INIT_DATA()
{
  Serial.println ();
  Serial.println (F("SETUP: initialize data structures ..."));	
  send_data.action_code  = 0;
  send_data.gPresureBMP = 0;
  send_data.gTempBMP = 0.00;
  send_data.AnalogSensorValue = 0;
  send_data.DS_1_WODA = 0.00;
  send_data.DS_2_BUFOR = 0.00;
  send_data.DS_3_WOLNY = 0.00;
  
  receive_data.action_code = 0;
}

void setup_UPDATE()
{
   setup_WIFI();
   setup_SERVER();
}

bool setup_WIFI()
{
		  WiFi.disconnect();
		  WiFi.softAPdisconnect();
		  WiFi.setPhyMode(WIFI_PHY_MODE_11B);
		  WiFi.setOutputPower(20.5);						// max: 20.5
		  WiFi.setSleepMode(WIFI_NONE_SLEEP,0);
		  WiFi.mode(WIFI_AP); 	
		  AP_CONNECTED = WiFi.softAP("DST2AIR-Module","destylacja");
		  
		   Serial.println("");  
		   Serial.println(F("Uruchamiam punkt dostępu wifi ..."));
		 		  		 
		   if( AP_CONNECTED == true)
		  {
			  Serial.println("");  
			  Serial.println("AP Ready");
			
			  Serial.println("");
			  Serial.println(F("AP IP address: "));
			  Serial.print(WiFi.softAPIP());
			  Serial.println(""); 
			  Serial.println(""); 
			  						  
			 ipap1 = WiFi.softAPIP()[0];
		     ipap2 = WiFi.softAPIP()[1];
		     ipap3 = WiFi.softAPIP()[2];
		     ipap4 = WiFi.softAPIP()[3];	
				
			 return true;	
				
		  }
		  else
		  {
			Serial.println("");  
			Serial.println("AP Failed!");
			ipap1 = 0;
		    ipap2 = 0;
		    ipap3 = 0;
		    ipap4 = 0;
		   return false;
		  }

}

void setup()
{
   system_update_cpu_freq(160); //160 albo 80 Mhz
     
   Serial.begin(115200);
   for(int x = 0;x < 15;x++)
   {
	 Serial.println(F("."));
     delay(500);
   }
   Serial.println(F(""));
   Serial.println(F(""));
   Serial.println(F("Sterowanie-Destylatorem moduł rozszeżeń v 1.2"));    
   Serial.println(F(""));
   Serial.println(F(""));      
   
   setup_INIT_DATA();
   setup_PINS();
   setup_DS();
   setup_I2C();
       
   WiFi.mode( WIFI_OFF );
   
   setup_TIMERS();
   setup_STATUS();
   
   setup_RXTX();
}

void ReceiveData()
{
	if(ETin.receiveData())
	{
			Serial.println(receive_data.action_code);
			Serial.printf("\n");
						
			switch(receive_data.action_code)
			{
				case MC_OK:
							if(AWARIA_DS == false)
							{
								send_data.action_code = MC_OK;
								
								if(MCU_LIVE == false)
								{
									Serial.println("");
									SetDiodeON();
									Serial.println("Polaczono ze sterownikiem !");
									Serial.println("");
									MCU_LIVE = true;
								}
							}else
							{
								send_data.action_code = MC_DS_ERROR;
							}
							MCU_Cntr++;
				break;
								
				case MC_EZ_GLOWICA_ON:
							SetValveON(EZ_GLOWICA);
							Serial.println("receive command EZ_GLOWICA ON");
				break;
				
				case MC_EZ_GLOWICA_OFF:
							SetValveOFF(EZ_GLOWICA);
							Serial.println("receive command EZ_GLOWICA OFF");
				break;
				
				case MC_EZ_PLUK_OLM_ON:
							SetValveON(EZ_PLUK_OLM);
							Serial.println("receive command EZ_POLM ON");
				break;
				
				case MC_EZ_PLUK_OLM_OFF:
							SetValveOFF(EZ_PLUK_OLM);
							Serial.println("receive command EZ_POLM OFF");
				break;
				
				case MC_UPDATE_ENABLE:
							setup_UPDATE();
							Serial.println("receive command wifi ON");
				break;
					
			}
			
			receive_data.action_code = 0;
			ETout.sendData();
					
	   }
}

void loop()
{
   Timer_update();	
   ReceiveData(); 
}

uint32_t getFlashChipRealSize(void)
{
    return (1 << ((spi_flash_get_id() >> 16) & 0xFF));
}

// BRUDNOPIS:


// Serial.println(txdata.gTempDS1);
// Serial.println(txdata.gTempDS2);
// Serial.println(txdata.gTempDS3);
// Serial.println("");

// Serial.print("Temperatura z bmp = ");
// Serial.print(txdata.gTempBMP);
// Serial.println(" *C");
// Serial.print("Ciśnienie z bmp = ");
// Serial.print(txdata.gPresureBMP);
// Serial.println(" Pa");
// Serial.println("");

 // "BME280  (const byte *)"\x76\x77"
 // "BME680  (const byte *)"\x76\x77"
 // "BMP085  (const byte *)"\x77"
 // "BMP180  (const byte *)"\x77"
 // "BMP280  (const byte *)"\x76\x77"
 //  urządzenia I2C
 //  119 (0x77)
 // (1 hPa ≡ 100 Pa)  = 1 mbar
 // kilopascal (1 kPa ≡ 1000 Pa)
 // megapascal (1 MPa ≡ 1,000,000 Pa)
 // gigapascal (1 GPa ≡ 1,000,000,000 Pa).
  
// if(digitalRead(LED_BUILTIN) == LOW)
// {
//   digitalWrite(LED_BUILTIN,HIGH);
// }else
// {
//   digitalWrite(LED_BUILTIN,LOW);
// }
	
