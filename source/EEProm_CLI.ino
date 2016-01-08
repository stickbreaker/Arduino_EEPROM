/*
Utility to Read/Write Arduino Builtin EEPROM
Chuck Todd <ctodd@cableone.net>
29DEC2015

serial command format

[repeatCount] read [type] address
[repeatCount] write [type] address value

type: [blank], one 8bit value 0..255
      byte, one 8bit value 0..255
      short, one 8bit value -128..127
			word, two 8bit bytes 0..65535
			int, two 8bit bytes -32768..32767
			longword, four 8bit bytes 0..4294967296
			longint, four 8bit bytes -2147483648..2147483647
			chars, up to 49 characters of ascii data
			c,     up to 49 characters of ascii data
			chars[maxLength], up to maxLength characters of ascii data, null will terminate
			c[maxLength], up to maxLength characters of ascii data, null will terminate
			hex, read data, display as 16 HEX character with ASCII representation
			x, read data, display as 16 HEX character with ASCII representation
			hex[Length], read data, display as [Length] HEX character with ASCII representation
			x[Length], read data, display as [Length] HEX character with ASCII representation
example:
  read byte 0 
	r byte 0
	7 r                 // repeat prior read command 7 times, incrementing address by DATATYPE size
	read word 1
	read 1
	write longint 1000 123456789
	w longint 1004 123456789
	read chars 9
	write chars 5 This string will be stored starting at address 5, it is terminated with a null.
	read chars[5] 0    // read from address 0, a c_string of up to 5 chars or to null, which ever is first
	read chars 6       // read from address 6, a c_string up to 50 chars or to null which ever is first
	r                  // repeat last read instruction, incrementing address by last datatype size.
	r x[4] 0 // display from address 0, 0000: XX XX XX XX AAAA
*/
#include "string.h" // so that I can use an enum as a function return

void help(bool all){
if(all) Serial.println(F("This code allows you to read and write from the EEPROM memory."));
Serial.println(F("\nFormat:\n [repeatcount] read [type] address\n [repeatcount]write [type] address value"));
if(all) Serial.println(F("? for Full Help"));
}
void help1(){
Serial.print(F("\ntype: [blank], one 8bit value 0..255\n"
  "  byte, one 8bit value 0..255\n  short, one 8bit value -128..127\n  word, two 8bit bytes 0..65535\n"
	"  int, two 8bit bytes -32768..32767\n  longword, four 8bit bytes 0..4294967296\n"
	"  longint, four 8bit bytes -2147483648..2147483647\n  chars, up to 49 characters of ascii data\n"
	"  HEX, sixteen 8bit bytes AAAA: XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX printableasciida\n"
	"example:\n  read byte 0\n  read word 1\n  read 1\n  write longint 1234 123456789\n  read chars 9\n"
	"  write chars 5 This string will be stored starting at address 5, it is terminated with a null.\n"));

// the F("string") construct store the text in Flash instead of RAM
}

void setup()
{
Serial.begin(9600);
help(true);
  /* add setup code here */
}

#include <EEPROM.h>
String command;

void loop()
{
   if (Serial.available())
   {
      char c=Serial.read();
      if (c=='\n')
      {
				if(command.equalsIgnoreCase("?")){
					help(false);
					help1();
					command ="";
					return;
				}
				int a = command.toInt();
				if(a>0) command = command.substring(command.indexOf(" ")+1);
				while(a>0){
          parseCommand(command);
					a--;
					}
        parseCommand(command);
				command="";
      }
		  else command+= c;
   }
}
static int strLen=0;

DATATYPE getDataType(String inStr){
int i = inStr.indexOf(" ");
if(i>0){
	String A = inStr.substring(0,i);
	if(A.equalsIgnoreCase("byte")) return dtByte;
	if(A.equalsIgnoreCase("small")) return dtSmall;
	if(A.equalsIgnoreCase("int")) return dtInt;
	if(A.equalsIgnoreCase("word")) return dtWord;
	if(A.equalsIgnoreCase("longint")) return dtLongInt;
	if(A.equalsIgnoreCase("longword")) return dtLongWord;
	if(A.equalsIgnoreCase("HEX")||A.equalsIgnoreCase("x")) {
		strLen=0;
		return dtHEX;
		}
	if(A.equalsIgnoreCase("chars")||A.equalsIgnoreCase("c")){
		strLen =0;
   	return dtChars;
	}
	if(A.equalsIgnoreCase("none")) return dtNone;
  A.toUpperCase();
  if(A.startsWith("CHARS[")||A.startsWith("C[")){
    i=A.indexOf("]");
    A	= A.substring(A.indexOf("[")+1,i);
	  strLen = A.toInt();
		return dtChars;
		}
  if(A.startsWith("HEX[")||A.startsWith("X[")){
    i=A.indexOf("]");
    A	= A.substring(A.indexOf("[")+1,i);
	  strLen = A.toInt();
		return dtHEX;
		}
  }
return dtNone;
}

const char * displayType(DATATYPE dt){
switch (dt){
	case dtNone: return "NONE";
	case dtByte: return "byte";
	case dtSmall : return "small";
	case dtInt : return "int";
	case dtWord : return "word";
	case dtLongInt : return "longword";
	case dtLongWord : return "longword";
  case dtChars : return "chars";
  }
return "";
}

char getHex(char chx){
char a=chx;
if((a>='0')||(a<='9')) return a-48;
if((a>='A')||(a<='F')) return a-55;
if((a>='a')||(a<='f')) return a-87;
return 0;
}

static DATATYPE dtOld=dtNone;
static int addressRead=0;

void parseCommand (String com)
{
    char ch;
		byte b;
		int i;
		uint16_t w;
		long li;
		uint32_t lw;
		char chx[50],ch1[5];
		
		const char * chpt;
	enum CMD {cmdRead,cmdWrite,cmdUnknown};
	CMD cmd;
		
  String part1;
 
	part1 = com.substring(0, com.indexOf(" "));
  if(part1.equalsIgnoreCase("read")||part1.equalsIgnoreCase("r"))	cmd=cmdRead;
	else if(part1.equalsIgnoreCase("write")||part1.equalsIgnoreCase("w")) cmd=cmdWrite;
	else cmd=cmdUnknown;

  if(cmd!=cmdUnknown){
		part1 = com.substring(part1.length()+1);

		DATATYPE dt = getDataType(part1);
		if(dt!=dtNone){
			part1 = part1.substring(part1.indexOf(" ")+1); // skip over datatype
			dtOld = dt;
			}
		else dt =dtOld;
		if (part1.length()!=0) {
			if(part1.indexOf("-")==0) addressRead = addressRead -strLen;
			else if(part1.indexOf("+")==0) addressRead = addressRead +strLen;
			else if(part1.indexOf(".")==0) ;
			else addressRead = part1.toInt();
		  }
    if(cmd==cmdWrite){ // get value for write
			part1 = part1.substring(part1.indexOf(" ")+1);
		}
		switch(cmd){
			case cmdRead : Serial.print(F("\nREAD ")); break;
			case cmdWrite: Serial.print(F("\nWRITE ")); break;
		}
		Serial.print(displayType(dt));
		Serial.print(F(" ADDRESS: "));
    Serial.print(addressRead, DEC);
    Serial.print(F("\tVALUE: "));
   
		switch (dt){
			case dtNone:;
			case dtByte: 
			  if(cmd==cmdRead) EEPROM.get(addressRead,b);
				else {
					b = part1.toInt();
					EEPROM.put(addressRead,b);
				}
				Serial.print(b,DEC);
				Serial.print(F(" (byte)"));
				addressRead++;
			  break;
			case dtSmall : 
				if(cmd==cmdRead) EEPROM.get(addressRead,ch);
				else{
					ch = part1.toInt();
					EEPROM.put(addressRead,ch);
				}
				Serial.print(ch,DEC);
				Serial.print(F(" (Small Int)"));
				addressRead++;
			  break;
			case dtInt : 
				if(cmd==cmdRead) EEPROM.get(addressRead,i);
				else{
					i = part1.toInt();
					EEPROM.put(addressRead,i);
				}
				Serial.print(i,DEC);
				Serial.print(F(" (int)"));
				addressRead=addressRead+2;
			  break;
			case dtWord:
				if(cmd==cmdRead) EEPROM.get(addressRead,w);
				else {
					w = part1.toInt();
					EEPROM.put(addressRead,w);
				}
			  Serial.print(w,DEC);
				Serial.print(F(" (word)"));
				addressRead=addressRead+2;
				break;
			case dtLongInt: 
				if(cmd==cmdRead) EEPROM.get(addressRead,li);
				else{
					li = part1.toInt();
					EEPROM.put(addressRead,li);
				}
				Serial.print(li,DEC);
				Serial.print(F(" (long)"));
				addressRead=addressRead+4;
				break;
			case dtLongWord:
			  if(cmd==cmdRead) EEPROM.get(addressRead,lw);
				else{
					lw = part1.toInt();
					EEPROM.put(addressRead,lw);
					}
				Serial.print(lw,DEC);
				Serial.print(F(" (unsigned long)"));
				addressRead=addressRead+4;
				break;
			case dtChars:
			  if(cmd==cmdRead) {
					b=0;
					while((b<50)&&(((strLen>0)&&(b<strLen))||(strLen==0))){
						chx[b]=EEPROM.read(addressRead++);
						b++;
						if(chx[b-1]=='\0') break;
						
					}
					chx[49]='\0';
					
					chpt = (const char*)chx; // cast to const char pointer so that I need only one print()
				}
				else{ // write
				  chpt = part1.c_str(); // access the String object as a cString.
					b=0;
					do{
						EEPROM.write(addressRead++,chpt[b++]);
						if(chpt[b-1]=='\0') break; // look for cString termination marker
						
					}while(b<255); // string is too long!
				}
				Serial.print(chpt);
				Serial.print(F(" (chars)"));
				if (strLen!=0){
					
					addressRead = addressRead + (strLen - b);
				}					
				break;
			case dtHEX:
				sprintf(chx,"%04X: ",addressRead);
				Serial.print(chx);
				if(cmd==cmdRead){
					b=0;
					while((b<16)&&(((strLen>0)&&(b<strLen))||(strLen==0))){
						chx[b]=EEPROM.read(addressRead++);
						sprintf(ch1,"%02X ",(uint8_t)chx[b]); Serial.print(ch1);
						if((chx[b]<32)||(chx[b]>127)) chx[b]='.';
						b++;
						}
					chx[b]=0;
					}
				else { // write
					chpt = part1.c_str();
					// r xcode for right justify with strLen = field width.
					if(strLen!=0){ //right Justify
						b=part1.length()/2;
						b= b % strLen;
						while(b<strLen){
							EEPROM.write(addressRead++,0);
							sprintf(ch1,"00 "); Serial.print(ch1);
							b++;
							}
						}
					b=0;
					while((b*2)<part1.length()){
						ch=(getHex(chpt[b*2])<<4)+getHex(chpt[(b*2)+1]);
						EEPROM.write(addressRead++,ch);
						sprintf(ch1,"%02X ",(uint8_t)ch); Serial.print(ch1);
						if((ch<32)||(ch>128)) ch='.';
						chx[b++]=ch;

						}
					chx[b]=0;
					b=strLen; //hex is always right justified if strLen is set
					}
				Serial.print(chx);
				Serial.print(F(" (HEX)"));
				if (strLen!=0){
					
					addressRead = addressRead + (strLen - b);
				}					
			  break;
			default : Serial.print(F("unknown Type\n"));
		}
   }
   else
   {
      Serial.println("COMMAND NOT RECOGNIZED");
			help(false);
			help1();
   }
}