/*

RUN in LINUX G++. PLEASE USE C++ 17.

Running instructions:
cd to the folder containing assembler.cpp, input.txt and linking_loader.cpp
FIRST RUN assembler.cpp on input.txt. INSTRUCTIONS in assembler.cpp
1) g++ linking_loader.cpp
2) ./a.out
Main Output in LOADER_INSTRUCTIONS.txt
MEMORY MAP in MEMORY_MAP.txt

*/

#include<bits/stdc++.h>

using namespace std;

//ESTAB according to book instructions - stores addresses of EXTDEF symbols and section names
unordered_map<string,string> ESTAB;

//Mapping from operation assempler codes to machine codes
static unordered_map<string,string> OPTAB = {
	{"LDA","00"},
	{"LDL","08"},
	{"LDX","04"},
	{"LDB","68"},
	{"LDT","74"},
	{"STA","0C"},
	{"STL","14"},
	{"STX","10"},
	{"LDCH","50"},
	{"STCH","54"},
	{"ADD","18"},
	{"SUB","1C"},
	{"MUL","20"},
	{"DIV","24"},
	{"COMP","28"},
	{"COMPR","A0"},
	{"CLEAR","B4"},
	{"J","3C"},
	{"JLT","38"},
	{"JEQ","30"},
	{"JGT","34"},
	{"JSUB","48"},
	{"RSUB","4C"},
	{"TIX","2C"},
	{"TIXR","B8"},
	{"TD","E0"},
	{"RD","D8"},
	{"WD","DC"}
};

//Errors
bool INCORRECT_FORMAT=false;
string INCORRECT_FORMAT_LINES;
bool DUPLICATE_EXTERNAL_SYMBOL=false;
string DUPLICATE_EXTERNAL_SYMBOL_LINES;
bool UNDEFINED_EXTERNAL_SYMBOL=false;
string UNDEFINED_EXTERNAL_SYMBOL_LINES;

//Get rid of spaces before and aster
string stripString(string s){
	while(s.length()>0 && s[s.length()-1]==' ')s.pop_back();
	int i=0;
	for(;i<s.length();i++){
		if(s[i]!=' ')return s.substr(i);
	}
	return s;
}

bool disectLine(string &line, string &label, string &opcode, string &operand, bool &extendedFlag){

	if(stripString(line).length()==0){
		return false;
	}
	//Checking if valid line and not comment
	if((line.length()>80 || line.length()<21) && stripString(line)[0]!='.'){
		return false;
	} else if(stripString(line)[0]=='.'){
		//Comments are still printed in intermediate.txt
		return true;
	}
	//Lines are padded to make them 80 characters long.
	while(line.length()<80)line+=' ';
	//Using rules mentioned above. Also, removing leading and trailing spaces.
	label=stripString(line.substr(0,10));
	opcode=stripString(line.substr(10,10));
	operand=stripString(line.substr(20));

	//Handle extended format
	if(opcode[0]=='+'){
		extendedFlag=true;
		opcode=opcode.substr(1);
	} else {
		extendedFlag=false;
	}

	return true;
}

//Get name of file
string getNameWithoutExtension(string s){
	for(int i=s.length()-1;i>=0;i--){
		if(s[i]=='.'){
			return s.substr(0,i);
		}
	}
	return s;
}

//Convert hexadecimal to decimal
int hex_to_dec(string hex){
	int dec=0;
	for(int i=0;i<hex.length();i++){
		int dig=0;
		if(hex[i]>='A' && hex[i]<='F')dig=hex[i]-'A'+10;
		else dig=hex[i]-'0';
		dec=dec*16+dig;
	}
	return dec;
}

//Convert decimal number to hexadecimal and return string
string dec_to_hex(int x){
	if(x==0)return "0";
	string hex;
	while(x>0){
		int dig=x%16;
		if(dig<10){
			hex+='0'+dig;
		} else {
			dig-=10;
			hex+='A'+dig;
		}
		x/=16;
	}
	reverse(hex.begin(),hex.end());
	return hex;
}

void errorHandling(){
	cout<<"LOADER TERMINATED WITH THE FOLLOWING ERRORS:\n";
	if(DUPLICATE_EXTERNAL_SYMBOL){
		cout<<"DUPLICATE_EXTERNAL_SYMBOL: "<<DUPLICATE_EXTERNAL_SYMBOL_LINES<<endl;
	}
	if(INCORRECT_FORMAT){
		cout<<"INCORRECT_FORMAT: "<<INCORRECT_FORMAT_LINES<<endl;
	}
}

bool checkIfFileExists(string s){
	ifstream f(s.c_str());
	return f.good();
}

int main(int argc, char *argv[]){
	//PROGADDR is to be given by OS. Here it is taken to be 0. This is the first section starting address
	int PROGADDR=0;
	int CSADDR=PROGADDR;	//Current section starting address

	string line;	//To process each line
	string progName="";	//Current section name
	int progLength=0;	//Current section length
	int progStartAddr=0;	//Current section starting address

	fstream fin,fout;	//Input and output streams
	if(!checkIfFileExists("object.txt")){
		cout<<"NO FILE NAMED object.txt\nPLEASE MAKE OBJECT FILE IN THE SAME DIRECTORY.\n";
		return 0;
	}
	fin.open("object.txt",ios::in);

	//PASS 1
	while(getline(fin,line,'\n')){
		if(stripString(line)==""){
			continue;
		}
		line=stripString(line);	//Get rid of spaces before and aster
		if(line[0]!='H'){	//This line has to be a header line. Else syntax error
			INCORRECT_FORMAT=true;
			INCORRECT_FORMAT_LINES+=line;
			errorHandling();
			return 0;
		}
		progName=stripString(line.substr(1,6));	//2-7 -> Program name
		progStartAddr=hex_to_dec(line.substr(7,6));	//7->12 -> start addr
		int CSLTH=hex_to_dec(line.substr(13,6));	//13->18 length of program
		if(ESTAB.find(progName)!=ESTAB.end()){	//Program name already defined - ERROR
			DUPLICATE_EXTERNAL_SYMBOL=true;
			DUPLICATE_EXTERNAL_SYMBOL_LINES+=line+'\n';
			errorHandling();
			return 0;
		}
		ESTAB[progName]=dec_to_hex(CSADDR);	//Establish starting address of program
		while(getline(fin,line,'\n')){
			if(stripString(line).length()==0)
				continue;
			line=stripString(line);
			if(line[0]=='E'){	//If end of section
				break;
			}
			if(line[0]=='D'){	//Define record - Add every SYMBOL to ESTAB
				if(line.length()>1){
					string symbolString=line.substr(1);	//First character is D
					for(int i=0;i<symbolString.length();i+=12){	//6 characters for symbol, 6 for valye
						string symbol=symbolString.substr(i,6);
						string addr=symbolString.substr(i+6,6);
						if(ESTAB.find(symbol)!=ESTAB.end()){	//If symbol already exists, ERROR
							DUPLICATE_EXTERNAL_SYMBOL=true;
							DUPLICATE_EXTERNAL_SYMBOL_LINES+=line;
							errorHandling();
							return 0;
						}
						ESTAB[symbol]=dec_to_hex(hex_to_dec(addr)+CSADDR);	//Add symbol to ESTAB
					}
				}
			}
		}
		CSADDR+=CSLTH;	//Onto the next section
	}

	fin.close();

	//Maximum now addr is CSADDR-1. Therefore memory size required is CSADDR. Making it divisible by 32.
	int memSize=(CSADDR*2/32+(CSADDR*2%32!=0))*32;	//ceil(CSADDR/32)*32. Hexadecimal, therefore *2
	char RAM[memSize];	//Simulate memory in an array
	for(int i=0;i<memSize;i++){
		RAM[i]='.';	//.=blank space
	}

	//PASS 2
	fin.open("object.txt",ios::in);

	CSADDR=PROGADDR;	//Currect section starting address
	int EXECADDR=PROGADDR;	//Execution starting address
	while(getline(fin,line,'\n')){
		if(stripString(line).length()==0)
			continue;
		line=stripString(line);	//Remove trailing and leading spaces
		int CSLTH=hex_to_dec(line.substr(13,6));	//Length of section
		//Start reading section
		while(getline(fin,line,'\n')){
			if(stripString(line).length()==0)
				continue;
			line=stripString(line);
			if(line[0]=='T'){	//Text record
				string code=line.substr(9);	//Effective code
				int start_address=(hex_to_dec(line.substr(1,6))+CSADDR)*2;	//(IN HALF BYTES) -> Start address to insert code
				for(int i=0;i<code.length();i++){
					RAM[start_address+i]=code[i];
				}
			} 
			else if(line[0]=='M'){	//Modification Record
				char sign=line[9];	//Sign to be used
				string symbol=line.substr(10);	//Symbol of Modif record
				if(ESTAB.find(symbol)==ESTAB.end()){	//If symbol not found
					UNDEFINED_EXTERNAL_SYMBOL=true;
					UNDEFINED_EXTERNAL_SYMBOL_LINES+=line+'\n';
					errorHandling();
					return 0;
				}
				int valueToAdd=hex_to_dec(ESTAB[symbol]);	//Value to be added
				int addressForModification=(CSADDR+hex_to_dec(line.substr(1,6)))*2;	//In HALF BYTES -> Memory address to start modification
				int lengthOfModification=hex_to_dec(line.substr(7,2));	//In half bytes
				if(lengthOfModification%2)	//Then last half bytes are to be modified
					addressForModification++;
				string valueToModify;	//The current value at that address
				for(int i=addressForModification;i<addressForModification+lengthOfModification;i++){
					valueToModify+=RAM[i];
				}
				int integer_helper=hex_to_dec(valueToModify);	//Easier to used
				integer_helper=(sign=='+')?(integer_helper+valueToAdd):(integer_helper-valueToAdd);	//Modify value
				valueToModify=dec_to_hex(integer_helper);
				while(valueToModify.length()<lengthOfModification)valueToModify='0'+valueToModify;	//Padding
				for(int i=addressForModification;i<addressForModification+lengthOfModification;i++){	//Write back to memory location
					RAM[i]=valueToModify[i-addressForModification];
				}
			}
			else if(line[0]=='E'){	//These records mention where to start execution
				if(line.length()>1){
					EXECADDR=CSADDR+hex_to_dec(line.substr(1));
				}
				break;
			}
		}
		CSADDR+=CSLTH;	//Next section
	}
	fin.close();

	//PASS 3
	//Writing all the opcodes to the output file
	if(!checkIfFileExists("intermediate.txt")){
		cout<<"NO FILE NAMED intermediate.txt\nPLEASE MAKE intermediate FILE IN THE SAME DIRECTORY.\n";
		return 0;
	}

	//Intermediate file used in getting instruction addresses
	fin.open("intermediate.txt",ios::in);
	fout.open("LOADER_INSTRUCTIONS.txt",ios::out);	//Write instruction codes

	CSADDR=0;
	while(getline(fin,line,'\n')){
		if(stripString(line).length()==0 || stripString(line)[0]=='.')
			continue;
		string label, opcode, operand;
		bool extendedFlag=false;
		string instruction_line=line.substr(14);
		//Get opcode, operand and label from line
		disectLine(instruction_line,label,opcode,operand,extendedFlag);
		if(opcode=="START" || opcode=="CSECT"){	//Used to set section address
			CSADDR=hex_to_dec(ESTAB[stripString(line.substr(14,10))]);
			continue;
		}
		if(OPTAB.find(opcode)!=OPTAB.end() || opcode=="WORD" || opcode=="BYTE" || opcode[0]=='='){
			//IF important instruction, get its address and length and write it.
			int address=(hex_to_dec(stripString(line.substr(0,6)))+CSADDR)*2;
			int len=stoi(stripString(line.substr(7,6)))*2;
			for(int i=address;i<address+len;i++){
				fout<<RAM[i];
			}
			fout<<'\n';
		}
	}

	fout.close();
	fin.close();

	//Write memory status to file
	fout.open("MEMORY_MAP.txt",ios::out);
	
	for(int i=0;i<memSize;i++){
		if(i%32==0){
			if(i!=0)fout<<'\n';
			string hexAddr=dec_to_hex(i/2);
			while(hexAddr.length()<6)hexAddr='0'+hexAddr;
			fout<<hexAddr<<" ";
		} else if(i%8==0){
			fout<<" ";
		}
		fout<<RAM[i];
	}

	fout.close();
}