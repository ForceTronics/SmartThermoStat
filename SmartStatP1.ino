/*This Arduino code is used for the Smart ThermoStat project on the ForceTronics blog http://forcetronic.blogspot.com/ or the Forcetronics Youtube page. 
This is for part 1 of the project and just implements general thermostat functionality. More advanced fuctions will be added in later versions. 
The project uses the following parts:
-->Adafruit Trellis keyboard for data entry
-->18-Bit Color TFT LCD Display
-->MCP97808 Temperature Sensor
-->3 Triac switches for controlling the a fan, heater, and air conditioning
*/

#include "SPI.h" //SPI comm library is used for LCD
#include "Adafruit_GFX.h" //LCD base library
#include "Adafruit_ILI9340.h" //LCD library
#include <Wire.h> //Library used for I2C comm
#include "Adafruit_Trellis.h" //keyboard library
#include "Adafruit_MCP9808.h" //Temp sensor library

// These are the pins used for the LCD and UNO: sclk 13, miso 12, mosi 11, cs 10, dc 9, rst 8
Adafruit_ILI9340 tft = Adafruit_ILI9340(10, 9, 8);
//create single keypad or matrix object for trellis (can add more later)
Adafruit_Trellis keyPad1 = Adafruit_Trellis();
//create key board or trellis object and add each keypad
Adafruit_TrellisSet keyBoard =  Adafruit_TrellisSet(&keyPad1);
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

int8_t isHeat = 0; //global variable to track if heat is on or off
int8_t pIsHeat = 0; //global variable to track past heat status
int8_t isCool = 0; //global variable to track if AC is on or off
int8_t pIsCool = 0; //global variable to track past cool status
int8_t isFan = 0; //global variable to track if fan is on or off
int8_t pIsFan = 0; //track past fan status
float heatSet = 70.0; //global variable to hold value of heat setting
float pastHeat = 70.0; //hold old set heat temp to erase from screen
float coolSet = 70.0; //global variable to hold value of cool setting
float pastCool = 70.0; //hold old set cool temp to erase from screen
float daTemp = 70.0; //global variable to hold the current measured temp
float pastTemp = 70.00; //global variable to hold old temp to erase from screen
float setTemp; //global variable to hold value being created in set mode
int8_t setState = 0; //used to track if in normal state or set temp state
int8_t setFunk = 0; //used to track which function (heat or cool) is selected
int8_t tempTimer = 0; //used to track when 3 seconds has passed to make next temp measurement
uint16_t setTimer = 0; //variable used as timer for set mode, when timer ends will exit set mode
int8_t setTempVal[] = {0, 0, 0}; //array to hold entered temp value
int8_t pHolder = 0; //keeps track of array position
int8_t kNumeric = 0; //when in set mode, tracks whether user is in numeric mode or arrow mode
int8_t digF = 1; //digital value to control whether fan is on or off, also used to track if fan is on or off
int8_t digC = 1; //digital value to control whether cool is on or off, also used to track if cool is on or off
int8_t digH = 1; //digital value to control whether heat is on or off, also used to track if heat is on or off
//for the above control signals 1 is off and 0 is on

//setup code that is excuted once when thermo stat is turned on
void setup() {
   tempsensor.begin(); //start temp sensor 
   tft.begin(); //start LCD control object
   tft.setRotation(1); //sets screen orentation
   tft.fillScreen(ILI9340_WHITE); //set the LCD screen background color
   
  // Interrupt pin requires a pullup
  pinMode(A2, INPUT);
  digitalWrite(A2, HIGH);
  
 //for controlling Thyristor switches: 1 is switch is open and 0 is switch is closed
  pinMode(5, OUTPUT);
  digitalWrite(5, digF); //turn off fan
  pinMode(4, OUTPUT);
  digitalWrite(4, digC); //turn off cool
  pinMode(3, OUTPUT);
  digitalWrite(3, digH); //turn off heat
  
  // begin() with the addresses of each panel in order
  keyBoard.begin(0x70);  // only one key board so this is default address
  //turn on LEDs for keyboard
  turnOnLEDs(); 
  //initialize display with temp and fac, cool, and heat status
  printTemp(daTemp);
  printHeat();
  printCool();
  printFan();
  
}

//enter main loop
void loop() {
  delay(30); //adafruit says 30ms delay is required, dont remove me! **not sure why***
   if (keyBoard.readSwitches()) { //check to see if any change in key state occurred
     int8_t key = getKeyPress(); //returns the key that was pressed
     if(!setState) { //if we are in normal mode do the following
       normalModeExecute(key); //execute function in normal mode based on key that was pressed
     }
     else { //we are in set mode so excute set mode function based on the key that was pressed
       setModeExecute(key);
     }
   }
   
   if(setState) {  ticSetTimer(); }//if in set mode, timer for setting temp (30 sec), after which returns to normal mode
   if(!setState) { getAndControlTemperature(); }//make new temp reading every 3 sec, print it out, and check if need heat or cool
}

//Gets and returns temp every 3 seconds. Checks if heat or cool is on and compares them to current temp. 
//Activates or deactivatives the heat or AC as needed
void getAndControlTemperature() {
  if(tempTimer < 99) { tempTimer++; } //check and tic timer
  else { //timer is up so measure temp
    if(isFan) { digitalWrite(5, 0); }//turn on fan, if it has been turned on
    else { digitalWrite(5, 1); }//turn off fan
    tempTimer = 0; //reset 3 second timer
    float temp = getTemp(); //get new temp value
    if(temp > 0 && temp < 120) { //make sure it falls within a reasonable range and error did not occur
      pastTemp = daTemp; //save past temp to be able to erase old value from screen
      temp = roundFloat(temp); //round to nearest 1/10 or one decimal point
      daTemp = temp;  //save new temp value to global temp variable
      eraseTemp(pastTemp); //erase old temp currently displayed
      printTemp(daTemp); //update thermostat display
      if(isCool && isHeat) { //if both heat and cool are on make sure heat set temp is less than cool
        if((heatSet + 1) > coolSet) { //if heat is set above or too close to cool turn it off
          isHeat = 0; //turn heat off
           printHeat(); //print that heat has been turned off
        }
        else { checkHeat(daTemp); } //if heat is set above cool, check heat if need to be turned on or off
        checkCool(daTemp); //turn on or off AC based on latest temp reading
      }
      else if(isCool) { //check if the AC is turned on
        checkCool(daTemp); //turn on or off cooling based on latest temp reading
      }
      else if(isHeat) { //check if the heat is turned on
        checkHeat(daTemp); //turn on or off heater based on latest temp reading
      }
      else { //make sure heat or cool is not on after it was turned off
        if(!isCool && !digC) {
          digC = 1; //turn cool global off
          digitalWrite(4, digC); //turn cool off
        }
        if(!isHeat && !digH) {
          digH = 1; //turn cool global off
          digitalWrite(3, digH); //turn cool off
        }
      }
    }
    else { } //can put something here to signal problem in temp reading
  }
}

//This function is only called if heat is on. It checks whether the heater needs to be turned on or off
void checkHeat(float temp) {
   if(digH) { //if heat is off allow 1 degree margin before turning it on
      temp = temp + 1.0;
    }
    else { temp = temp - .5; } //make the temp appear lower than it is so heat will push it slightly above set temp
    
    if(temp < heatSet) { //check if heat needs to be turned on
       digH = 0; //turn heat global on
       digitalWrite(3, digH); //turn on heat
    }
    else { //if heat is on it should not be activated
      digH = 1; //turn heat global off
      digitalWrite(3, digH); //turn heat off
    }
}

//This function is only called if AC is on. It checks whether the cooling needs to be turned on or off
void checkCool(float temp) {
    if(digC) { //if AC is off allow 1 degree margin before turning it on
      temp = temp - 1.0;
    }
    else { temp = temp + .5; } //make the temp appear higher than it is so AC will push it slightly below set temp
  
    if(temp > coolSet) { //check if cooling it needs to be activated
      digC = 0; //turn cool global on
      digitalWrite(4, digC); //turn on cool
    }
    else { //if AC is on it should not be activated
      digC = 1; //turn cool global off
      digitalWrite(4, digC); //turn cool off
    }
}

//This function returns which key was just pressed. If key press is unclear return -1
int8_t getKeyPress() {
  for (int8_t i=0; i<16; i++) { //loop through each key to see if any was pressed
	if (keyBoard.justPressed(i)) { //will be true for the key that was pressed
          toggleLED(i); //turn off button LED briefly then turn it back on
	  return i+1; //add 1 since numbering in software starts at zero but hardware label starts at 1
	} 
   }
   return -1; //no key was pressed
}

//this function is used in normal mode detects when the user wants to change heat, cool, or fan state. Also if user wants to enter set mode
//It will only execute if setState is false
void normalModeExecute(int8_t key) {
  //determine which button is pressed in normal mode and what action to take
  switch (key) {
    case 4: //fan button was pressed
      controlFan(); //function that turns on or off fan and updates screen
    break;
    case 8: //heat button was pressed
      controlHeat(); //function turns heat on or off and selects it and updates screen
      printCool();
    break;
    case 12: //cool button was pressed
      controlCool(); //function turns cool on or off and selects it and updates screen
      printHeat();
    break;
    case 13: //up arrow was pressed
      toogleSet(); //toggles selected function (heat or cool)
    break;
    case 15: //down arrow was pressed
      toogleSet(); //toggles selected function (heat or cool)
    break;
    case 16: //set button was pressed
      enterSetState(1); //set button was pressed so enter
    break;
  }
}

//function is called when set state is turned on. updates screen and sets proper variables for set state
void enterSetState(int8_t tprint) {
   setState = 1; //set mode variable
   kNumeric = 0; //start in arrow mode, not numeric mode
   pHolder = 0; //set array tracker for numeric key entry back to zero
   tft.fillScreen(ILI9340_WHITE); //Clear screen
   tft.setTextColor(ILI9340_BLACK); //set text color
   tft.setTextSize(2); //set test size
   tft.setCursor(0,80); //set cursor for text
   if(!setFunk) { //if your are setting the heat
     setTemp = heatSet; //temp in set mode is current heat temp 
     tft.print("Set heat, current setting: "); //print set heat string
     tft.print(heatSet,1); //print current set value of the heat
   }
   else { //you are setting AC
     setTemp = coolSet; //temp in set mode is current cool temp 
     tft.print("Set cool, current setting: "); //print set cool string
     tft.print(coolSet,1); //print current set value of the cool
   }
   
   if(tprint) { printTemp(setTemp); }//print the current temp setting for heat or cool if needed
}

//this function is used in set mode and is called when a key is pressed. 
//executes the appropriate action based on the key that was pressed
void setModeExecute(int8_t key) {
  int8_t value;//store the key press
  switch (key) {
    case 1: //value 1
      value = 1;
    break;
    case 2: //value 2
      value = 2;
    break;
    case 3: //value 3
      value = 3;
    break;
    case 5: //value 4
      value = 4;
    break;
    case 6: //value 5
      value = 5;
    break;
    case 7: //value 6
      value = 6;
    break;
     case 9: //value 7
      value = 7;
    break;
    case 10: //value 8
      value = 8;
    break;
    case 11: //value 9
      value = 9;
    break;
    case 13: //down arrow was pressed
      if(!kNumeric) { //already in arrow mode so just increment
        eraseTemp(setTemp); //erase currently displayed temp
      }
      else { //in numeric mode so clear display and increment
        enterSetState(0); //reset display and set numeric variable to false
      }
       setTemp = incrementTemp(setTemp, 0); //increment temp down .1 degree
       printTemp(setTemp); //print the current temp setting for heat or cool
      value = -1;
    break;
    case 14: //value 0
      value = 0;
    break;
    case 15: //Up arrow was pressed
      if(!kNumeric) { //already in arrow mode so just increment
        eraseTemp(setTemp); //erase currently displayed temp
      }
      else { //in numeric mode so clear display and increment
        enterSetState(0); //reset display and set numeric variable to false
      }
      setTemp = incrementTemp(setTemp, 1); //increment temp up .1 degree
      printTemp(setTemp); //print the current temp setting for heat or cool
      value = -1;
    break;
    case 16: //set button was pressed so exit set mode
      exitSetState(1); //exit set mode and use current entered value
      value = -1;
    break;
    default:
      value = -1; //an invalid button was pressed
    break;
  }
  
 
  if(value > -1) { //true if numeric key was pressed
    kNumeric = 1; //set numeric key mode to true
    if(pHolder > 2) { //if all three digits have been entered then start at front
      pHolder = 0; //set place holder back to front of array
      eraseSetTemp(); //erase current displayed temp
      setTempVal[pHolder] = value; //set value of the entered temp
      printSetTemp(value, ILI9340_BLACK, pHolder); //print the current entered values
      pHolder++;
    } 
    else { //is true if in the middle of entering temp value
     setTempVal[pHolder] = value; //set value of the entered temp
     printSetTemp(value, ILI9340_BLACK, pHolder); //print the current entered values
     pHolder++; 
    }
  }
  
}

//in set mode this function prints to the LCD the value being entered from the key pad
void printSetTemp(int8_t val, uint16_t color, int8_t pH) {
  if(pH == 0) { //check if this is the first digit being printed
    eraseTemp(setTemp); //erase currently displayed temp
    tft.setTextColor(color);
    tft.setCursor(0,5);
    tft.setTextSize(6);
    tft.print(val); //print 10's number value
  }
  else if(pH == 1) {
    tft.print(val); //print 1's number value
    tft.print("."); //print decimal point
  }
  else {
    tft.print(val); //print .1's number value
  }
}

//Erases set temp that user types in if they circle around 
void eraseSetTemp() {
  printSetTemp(setTempVal[0], ILI9340_WHITE,0);
  printSetTemp(setTempVal[1], ILI9340_WHITE,1);
  printSetTemp(setTempVal[2], ILI9340_WHITE,2);
}

//Called when exiting set state. Checks entered value for heat or cool to see if legitimate and then stores it
//changes needed variables and display for normal mode
void exitSetState(int8_t useVal) {
  setState = 0; //set mode variable
  tft.fillScreen(ILI9340_WHITE); //Clear screen
  setTimer = 0; //put timer back to zero
  if(useVal) {
    //check what mode we are in, key pad or arrow to set the new temp
    float j; //holds set temp 
    if(kNumeric) { j = digitToTemp(); }//get key pad entered new set temp
    else { j = setTemp; } //get arrow entered new set temp
    
    if(checkTempRange(j)) {
      if(!setFunk) { //setting the heat
         heatSet = j; //set heat value
         pastHeat = j;
       }
       else { //you are setting AC
         coolSet = j; //set AC value
         pastCool = j;
       }
    }
  }
   
   setTempVal[0] = 0;//set array values back to zero
   setTempVal[1] = 0;
   setTempVal[2] = 0;
   
   //Set display back to normal mode
   printTemp(daTemp);
   printHeat();
   printCool();
   printFan();
}

//This function will enable or disable the fan as well as update the screen with new state
void controlFan() {
  pIsFan = isFan; //capture the previous state of the fan
  isFan = toggleState(isFan); //toggle fan value
  printFan(); //call function to change screen state 
}

//This function will enable or disable the heat and change heat to the selected function as well as update the screen with new state
void controlHeat() {
  isHeat = toggleState(isHeat); //toggle state of heat
  setFunk = 0; //set to 0 for heat selected
  printHeat(); //call function to change screen state 
}

//This function will enable or disable the AC and change AC to the selected function as well as update the screen with new state
void controlCool() {
  isCool = toggleState(isCool); //toggle state of cool
  setFunk = 1; //set to 1 for cool selected
  printCool(); //call function to change screen state 
}

//This function is used for arrow buttons when in normal state. it toggles which function (heat or cool) is selected
void toogleSet() {
  setFunk = toggleState(setFunk); //toogle the selected function variable
  printHeat(); //call function to change screen state 
  printCool();
}

//erases old temp so new one can be printed. Input is past temp value
void eraseTemp(float temp) {
  tft.setTextColor(ILI9340_WHITE);
   tft.setCursor(0,5);
  tft.setTextSize(6);
  tft.println(temp,1);
}

//prints the temp on the screen, input is current temp value
void printTemp(float temp) {
  tft.setTextColor(ILI9340_BLACK);
   tft.setCursor(0,5);
  tft.setTextSize(6);
  tft.println(temp,1);
}

//prints the status of the heat: off or on and what temp set
void printHeat() {
  eraseHeatCool(pIsHeat, pastHeat, 80); //erase previous values before print new states
  tft.setTextSize(2);
  tft.setCursor(0,80); //set cursor for list settings
  tft.setTextColor(ILI9340_BLACK); //set text color
  tft.print("Heat:"); //pring heat string
  if(isHeat) { 
    tft.print("ON"); //if heat is on let user know
    pIsHeat = 1; //set variable for past value
  }
   else {
    tft.print("OFF");
    pIsHeat = 0; //set variable for past value
  }
  tft.print("  Set:"); //set string
  tft.print(heatSet,1); //print current heat temp setting
  if(!setFunk) tft.print(" <--");  //Tell user if heat is current set selection
}

//prints the status of the AC: off or on and what temp set
void printCool() {
  eraseHeatCool(pIsCool, pastCool, 100); //erase previous values before print new states
  tft.setTextSize(2);
  tft.setCursor(0,100); //set cursor for list settings
  tft.setTextColor(ILI9340_BLACK); //set text color
  tft.print("Cool:"); //cool string
  if(isCool)  { //if AC is on
    tft.print("ON");
    pIsCool = 1; //set variable for past value
  }
  else {
    tft.print("OFF");
    pIsCool = 0; //set variable for past value
  }
  tft.print("  Set:"); //set string
  tft.print(coolSet,1);
  if(setFunk) tft.print(" <--");  //Tell user if cool is current set selection
}

//this function erases the current displayed cool and heat settings when in normal mode
//Called when change is being made and they need to be updated
void eraseHeatCool(int8_t state, float val, int cur) {
  tft.setTextSize(2);
  tft.setCursor(0,cur); //set cursor for list settings
  tft.setTextColor(ILI9340_WHITE); //set text color
  tft.print("     "); //cool or heat with colon
  if(state) tft.print("ON"); //erase old state
  else tft.print("OFF");
  tft.print("  Set:"); //set string
  tft.print(val,1); //erase old set value
  tft.print(" <--"); //erase selected function arrow
  tft.print("  <--"); //erase selected function arrow
}


//prints the status of the fan: on or off
void printFan() {
  eraseFan(); //erase text for old fan setting
  tft.setTextColor(ILI9340_BLACK); //set text color
  tft.setTextSize(2);
  tft.setCursor(0,120); //set cursor for list settings
  if(isFan)  tft.print("Fan:ON");
  else tft.print("Fan:OFF");
}

//This is called to erase the current state of the fan so it can be updated
void eraseFan() {
  tft.setTextSize(2);
  tft.setCursor(0,120); //set cursor for list settings
  tft.setTextColor(ILI9340_WHITE); //set text color
  if(pIsFan)  tft.print("Fan:ON");
  else tft.print("Fan:OFF");
}

//this function gets temp value from sensor and converts it to F
float getTemp() {
  float c = tempsensor.readTempC();
  return (c * 9.0 / 5.0 + 32);
}

//toggles the state of a bool variable
int8_t toggleState(int8_t state) { 
  if(state) return 0;
  else return 1;
}

//round float to nearest 1/10th
float roundFloat(float val) {
  int t = (int)(val*10); 
  float j = (float)t;
  return (j/10);
}


//timer function when in set mode. timer gives ~30 sec if you do not enter by 30 sec
//it will exit set mode with no change to the set value
void ticSetTimer() {
  if(setTimer < 999) { setTimer++; }
  else {
   exitSetState(0); //timer is up so exit set state and do not use entered value
  }
}

//converts a three single digit ints into a float with 10, 1, .1
//used in set mode for manually entering temp value
float digitToTemp() {
  return (((float)setTempVal[0]*10) + (float)setTempVal[1] + ((float)setTempVal[2]/10));
}

//function that ensures set heat or cool temperature falls within certain range
int8_t checkTempRange(float temp) {
  if(temp > 55 && temp < 90.1)  return 1;
  else return 0;
}

//Function increments the temperature input, either up .1 or down .1
//used in set mode when arrow key is pressed
float incrementTemp(float temp, int8_t uD) {
  
  if(checkTempRange(temp)) {
    if(uD == 1) { return (temp + .1); }
    else { return (temp - .1); }
  }
  else { return temp; }
}

//turn on all LEDs for keyboard
void turnOnLEDs() {
 for(int i=0; i<16; i++) {
   keyBoard.setLED(i); //turn LED on
   keyBoard.writeDisplay(); //write on state
 }
}

//when button is pressed toogle the LED off briefly then back on
void toggleLED(int8_t i) {
  keyBoard.clrLED(i); //clear lit LED
  keyBoard.writeDisplay(); //write off state
  delay(150);
  keyBoard.setLED(i); //turn LED on
  keyBoard.writeDisplay(); //write on state
}
