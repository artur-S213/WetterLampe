#include <FastLED.h>
#include <ArduinoJson.h>                  
#include <math.h>                         
#include <WiFiManager.h>  
                
#define BRIGHTNESS 60
#define MAX_BRIGHT 255
#define DATA_PIN D3 
#define NUM_LEDS 8   

CRGB leds[NUM_LEDS];

WiFiManager wifiManager;
WiFiClient client;

// ========================  hier den API-Key eintragen!!!  ============================================================================================================
const String city = "Ganderkesee";
const String api_key = "2d1b9b40ff5be256216260b01b65dbc2";    // dein Open-Weather-Map-API-Schluessel, kostenlos beziehbar ueber https://openweathermap.org/
// ========================================================================================================================================================================

//Variablen zum abspeichern der aktuellen Wetterdaten
int weatherID = 0;
int newWeatherID;
int weatherID_shortened = 0;
String weatherforecast_shortened = " ";
int temperature_Celsius_Int = 0;
unsigned long lastcheck = 0;

//Variablen für die LEDWave-Methode
uint16_t x = 0;
uint16_t wave_scale;
uint16_t wave2;
uint16_t y;

//Variablen für den Übergang zwischen Lichtanimationen
int timemark = 0;
int timemark2 = 0;


//--------Palette--------------//
uint8_t paletteIndex = 0;
  DEFINE_GRADIENT_PALETTE (heatmap_gp){
    0, 255, 244, 2, //yellow
    76, 255, 244, 2, //yellow
    92, 94, 170, 227, //blue
    191, 47, 134, 252, //blue
    255, 47, 134, 252 //blue
    
  };
  //aktiviere Palette
  CRGBPalette16 myPal = heatmap_gp;

//----------Setup--------------//
void setup() {
  Serial.begin(115200);    
  //LED-Streifen wird eingerichtet
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 400);
  FastLED.clear();
  FastLED.show();
  //Zu Beginn leuchtet der LED-Streifen rot
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB( 250, 0, 0);
  }
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
  //verbindet sich mit dem Internet
  wifiManager.autoConnect("deineWetterLampe");
  
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB( 0, 0, 0);
  }
  FastLED.setBrightness(0);
  FastLED.show();
  delay(2000);

  //Erste Lichtanimation wird gestartet
  getCurrentWeatherConditions();
  weatherID = newWeatherID;
  int brightness = getLEDAnimaitonBrightness(weatherID);
  while (FastLED.getBrightness() < brightness){
    fade_in (1500, 0, brightness);
    choose_LED_animation(weatherID);
  }
}

//--------Loop---------//

void loop() {  

  //alle 30 min wird die aktuelle Wetterlage aktualisiert
  if (millis() - lastcheck >= 1800000) {         
    getCurrentWeatherConditions();
    lastcheck = millis();   
    //Wenn sich Wetterlage verändert hat, dann wechsle die Lichtanimation mit Übergang
    if ((weatherID / 100)!= (newWeatherID / 100)){
      //Helligkeitswerte die für den Übergang benötigt werden
      int startValue = getLEDAnimaitonBrightness(weatherID);
      int endValue = getLEDAnimaitonBrightness(newWeatherID);
      // Zeitwerte, die für den Übergang benötigt werden
      int fadeoutTime = getTransitionTime(weatherID);
      int fadeinTime = getTransitionTime(newWeatherID);
      //Übergang zwischen den Lichtanimationen
      change_LED_animation(fadeoutTime, fadeinTime, startValue, endValue, weatherID, newWeatherID);
      weatherID = newWeatherID;

    }                      
  }

  choose_LED_animation(weatherID); 
}

//----------Functions----------//

//Entscheidet anhand der WeatherID welche Lichtanimation gestartet wird
void choose_LED_animation(int id){
  if (id == 800){
        LED_animation_clearSky();
      }
      else {
        int id_short = id / 100;
        switch (id_short) {
          case 2: 
            LED_animation_thunderstorm();
            break;
          case 3: 
            LED_animation_dizzle(); 
            break;
          case 5: 
            LED_animation_rain(); 
            break;
          case 6: 
            LED_animation_snow(); 
            break;
          case 7:
            LED_animation_cloudy();
            break;
          case 8: 
            LED_animation_cloudy(); 
          break;
        }
      }
}
//Mit dieser Funktion werden die aktuellen Wetterdaten abgerufen und in Variablen gespeichert
void getCurrentWeatherConditions() {
  //Mikrocontroller verbindet sich mit openWeatherMap
  int WeatherData;
  Serial.print("connecting to "); Serial.println("api.openweathermap.org");
  if (client.connect("api.openweathermap.org", 80)) {
    client.println("GET /data/2.5/weather?q=" + city + ",DE&units=metric&lang=de&APPID=" + api_key);
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("connection failed"); //Wenn die Verbindung abbricht, dann leuchtet dr LED-Streifen rot
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB( 250, 0, 0);
    }
  }
  //speichert die Größe des json file 
  const size_t capacity = JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(14) + 360;
  //mithilfe des DynamicJsonDocument wird der Inhalt der json files gespeichert
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, client);
  client.stop();
//Speichert aktuelle Wetterlage
  newWeatherID = doc["weather"][0]["id"];
            
}

//--------Animationen--------------//

//===============================================

//Hilfsmethode für rain und thunderstorm, die mithilfe einer Wellenfunktion verschiedene Farbtöne erzeugt
void LEDwave (int bpm, int sin_min, int sin_max, int colour, int factor, int brightness){
  //Welle mit Geschwindigkeit, Minimum, Maximum
  wave_scale = beatsin8(bpm, sin_min, sin_max);
  y = millis() / 5;
  //für jede LED wird ein anderer Farbton erzeugt. Dadurch erhält man eine Wellenanimation 
  for (int i = 0; i < NUM_LEDS; i++){
    //Welle wird nochmal verzerrt für die richtigen Farbtöne
    int var = inoise8((i*wave_scale), y );
    leds[i] = CHSV(colour, wave_scale-factor, var);
  }
  FastLED.show();
}

//===============================================

void LED_animation_clearSky() { // Effekt, der angezeigt wird, wenn der Himmel klar ist
    //LED-Streifen zeigt den Farbverlauf der Palette an
   //fill_palette(array, anzahlLeds,Startpunkt in Palette, Veränderung der Farbe, Palette, Brightness, linear)
  fill_palette(leds, NUM_LEDS, 0, 255/NUM_LEDS, myPal, BRIGHTNESS,LINEARBLEND);
  FastLED.show();
}

//===============================================

void LED_animation_cloudy() {//alle LEDs leuchten Grau
  
  for (int i = 0; i < NUM_LEDS; i++){
    leds[i] = CRGB(100, 123, 139);
  }
  FastLED.show();
}

//===============================================

void LED_animation_rain(){//LEDs leuchten in verschiedenen Blautönen
  LEDwave(70, 220, 250, 158, 50, 64);
}

//===============================================

void LED_animation_dizzle(){//LEDs leuchten langsamer in verschiedenen Blautönen, mit Wellenfunktion(ähnlich wie die LEDwave Funktion)
  
  wave_scale = beatsin8(55, 230, 250);
  y = millis() / 5;
  
  for (int i = 0; i < NUM_LEDS; i++){
    
    int var = inoise8((i*wave_scale), y );
    leds[i] = CHSV(150, var-40, 130);
  }
  FastLED.show();
}

//===============================================

void LED_animation_thunderstorm(){
  //rain Methode
  LEDwave(90, 220, 250, 158, 50, 64);
  //Blitz wird zufällig generiert mit Zufallszahl von 0 bis 1000
  int lightning = random(1000);
  //Wenn die Zufallszahl 15 ist, dann soll ein Blitz ausgelöst werden
  if (lightning == 15){
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(255, 252, 124); 
    }
    FastLED.show();
  }
}

//===============================================

void LED_animation_snow(){//LEDs leuchten  in verschiedenen Weißtönen, mit Wellenfunktion(ähnlich wie die LEDwave Funktion)

  wave_scale = beatsin8(65, 220, 255);
  y = millis() / 5;
 
  for (int i = 0; i < NUM_LEDS; i++){
    
    int var = inoise8(i*wave_scale,y);
    leds[i] = CHSV(150, 0, var+25);
  }
  FastLED.show();
}


//---------Funktionen für den Übergang zwischen zwei Lichtanimationen -----------------------//


int getLEDAnimaitonBrightness(int id){//gibt die entsprechende Helligkeit der jeweiligen Lichtanimation zurück
  if ((id / 100) == 6 || id == 800){
    return 100;
  }
  else if (id == 0){
    return 0;
  }
  else {
    return 64;
  }
}

//gibt die Zeitwerte für die jeweiligen Ein- und Ausblendungen der Lichtanimationen in Millisekunden zurück
int getTransitionTime(int id){
  id = id / 100;
  if (id == 6){
    return 2000;
  }
  else{
    return 1500;
  }
  
}


// Führt den die Überblendung zwischen zwei Lichtanimtionen durch
void change_LED_animation(int fadeoutTime, int fadeinTime, int fromBrightness, int toBrightness, int animation_out, int animation_in){ //Wechselt zwischen zwei Lichtanimationen
  //in der ersten while-Schleife wird die aktuelle Lichtanimation ausgeblendet
  while (FastLED.getBrightness() > 0){
      fade_out (fadeoutTime,fromBrightness, 0); 
      choose_LED_animation(animation_out);
  }
  delay(1000);
  //in der zweiten while-Schleife wird die neue Lichtanimation eingeblendet
  while (FastLED.getBrightness() < toBrightness){
    fade_in (fadeinTime, 0, toBrightness);
    choose_LED_animation(animation_in);
  }
}


//in regelmäßigen Zeitintervallen wird die Helligkeit um 1 reduziert(erste while-Schleife in change_LED_animation)
void fade_out (int duration, int start_brightness, int end_brightness){
  //Falls LED heller wird als sie soll, dann wird sie auf den entsprechenden Endwert gesetzt
  if (FastLED.getBrightness() <= end_brightness) {
    FastLED.setBrightness(end_brightness);  
  }
  else{
    //Berechnung des Zeitintervalls
    int timeInterval = duration / (start_brightness - end_brightness);
    //Wenn der Zeitintervall erreicht wurde, dann wird die Helligkeit um 1 reduziert
    if (millis() - timemark >= timeInterval){
      timemark = millis();
      FastLED.setBrightness(FastLED.getBrightness() - 1);
    }
  }
  FastLED.show();
}


//in regelmäßigen Zeitintervallen wird die Helligkeit um 1 erhöht(zweite while-Schleife in change_LED_animation)
//gleiches Prinzip wie bei fade_out
void fade_in (int duration, int start_brightness, int end_brightness){
  if (FastLED.getBrightness() >= end_brightness) {
    FastLED.setBrightness(end_brightness);  
  }
  else{
    int timeInterval =  (int) (duration / (end_brightness - start_brightness));
    if (millis() - timemark2 >= timeInterval){
      
      timemark2 = millis();
      FastLED.setBrightness(FastLED.getBrightness() + 1);

    }
  }
  FastLED.show();
}
