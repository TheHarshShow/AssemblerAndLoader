/*

Environment - Linux g++ (PLEASE USE C++17)

Running instructions:
cd to the folder containing assembler.cpp, input.txt and linking_loader.cpp
1. g++ assembler.txt
2. ./a.out input.txt
The output will be available in object.txt. The intermediate file is intermediate.txt

Harsh Motwani
180101028

!!!IMPORTANT - LINE FORMAT
Every line should have between 21 and 80 columns (inclusive).
First 10 columns for LABEL, next 10 columns for OPCODE and (at max) the next 60 for OPERAND

*/

#include <bits/stdc++.h>

using namespace std;

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

//Error/Warning flags and history
//DUPLICATE SYMBOL HANDLED IN SYMTAB
bool SYNTAX_ERROR=false;
string SYNTAX_ERROR_LINES;
bool UNDEFINED_SYMBOL=false;
string UNDEFINED_SYMBOL_LINE;
bool INVALID_OPERATION_CODE=false;
string INVALID_OPERATION_CODE_LINE;
bool DUPLICATE_SYMBOL=false;
string DUPLICATE_SYMBOL_LINE;
bool ILLEGAL_EXTDEF=false;
string ILLEGAL_EXTDEF_LINE;
bool DISPLACEMENT_OUT_OF_BOUNDS=false;
string DISPLACEMENT_OUT_OF_BOUNDS_STRING;
bool LITTAB_VALUES_NOT_DEFINED=false;

//Table mapping symbols to their addresses and error flags. Predefined symbols are registers. 
//Secondary map for section name.
unordered_map<string, unordered_map<string,string>> SYMTAB = {
	{"A",{{"PREDEFINED","0"}}},
	{"X",{{"PREDEFINED","1"}}},
	{"L",{{"PREDEFINED","2"}}},
	{"B",{{"PREDEFINED","3"}}},
	{"S",{{"PREDEFINED","4"}}},
	{"T",{{"PREDEFINED","5"}}},
	{"F",{{"PREDEFINED","6"}}},
	{"PC",{{"PREDEFINED","8"}}},
	{"SW",{{"PREDEFINED","9"}}}
};

//Literal Table
static unordered_map<string,string> LITTAB;

//EXTREF hashmap
static unordered_set<string> EXTREF_SYMBOLS;

//Modification Records
static vector<string> MODIF_RECORDS;

//Program name to length maps
static unordered_map<string,int> progNames;
string progName="";

//Base value - by default 0
int base_register_value;

//Remove spaces from beginning and end of string
string stripString(string s){
	//Removing spaces from end
	while(s.length()>0 && s[s.length()-1]==' ')s.pop_back();
	if(s.length()==0)return "";
	//Removing spaces from beginning
	int i=0;
	for(;i<s.length();i++){
		if(s[i]!=' ')break;
	}
	return s.substr(i);
}

//Retrieve file name without extension so output file name can be created
string getFileWithoutExtension(string s){
	for(int i=(int)s.length()-1;i>=0;i--){
		if(s[i]=='.')return s.substr(0,i);
	}
	return s;
}

//Extract opcode, label and oeprand from a line
/*

!!!IMPORTANT
Every line should have between 21 and 80 columns (inclusive).
First 10 columns for LABEL, next 10 columns for OPCODE and (at max) the next 60 for OPERAND

*/
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

//Used to get length of instruction/reserved space in RESW, RESB and BYTE
string int_to_string(int x){
	if(x==0)return "0";
	string s;
	while(x>0){
		s+=x%10+'0';
		x/=10;
	}
	reverse(s.begin(),s.end());
	return s;
}

//Check if string only contains digits
bool isInteger(string s){
	for(int i=0;i<s.length();i++){
		if(!isdigit(s[i]))
			return false;
	}
	return true;
}

//Break a byte into its hexadecimal parts. Used for converting C'<string>' into hexadecimal characters
string getHexBreakup(string s){
	string final;
	for(int i=0;i<s.length();i++){
		//Break into two hexadecimal codes
		int x=(s[i]>>4);
		int y=s[i]-(x<<4);
		final+=dec_to_hex(x);
		final+=dec_to_hex(y);
	}
	return final;
}
//Used to generate line if either label, opcode or operand are changed. Used in hexadecimal string padding.
string createLine(string label, string opcode, string operand){
	while(label.length()<10)label+=' ';
	while(opcode.length()<10)opcode+=' ';
	while(operand.length()<60)operand+=' ';
	return label+opcode+operand;
}

//Check out error/warning flags
void errorHandling(){

	//Errors:
	cout<<"\nTERMINATED WITH THE FOLLOWING ERRORS:\n";
	if(UNDEFINED_SYMBOL){
		cout<<"UNDEFINED_SYMBOL at lines: "<<UNDEFINED_SYMBOL_LINE<<'\n';
	}
	if(INVALID_OPERATION_CODE){
		cout<<"INVALID_OPERATION_CODE at lines: "<<'\n'<<INVALID_OPERATION_CODE_LINE;
	}
	if(SYNTAX_ERROR){
		cout<<"SYNTAX_ERROR in the following lines:\n"<<SYNTAX_ERROR_LINES<<"Notes for SYNTAX:\n1) All lines should be between 21 and 80 columns (inclusive) in length.\n2) 10 columns for LABEL, 10 columns for OPCODE and 60 columns for OPERAND.\n3) BYTE operand should be inside X\'\' or C\'\'\n";
	}
	if(DUPLICATE_SYMBOL){
		cout<<"DUPLICATE_SYMBOL: "<<DUPLICATE_SYMBOL_LINE<<'\n';
	}
	if(ILLEGAL_EXTDEF){
		cout<<"ILLEGAL_EXTDEF: "<<ILLEGAL_EXTDEF_LINE<<'\n';
	}
	if(DISPLACEMENT_OUT_OF_BOUNDS){
		cout<<"DISPLACEMENT_OUT_OF_BOUNDS: "<<DISPLACEMENT_OUT_OF_BOUNDS_STRING<<'\n';
	}
	if(LITTAB_VALUES_NOT_DEFINED){
		cout<<"LITTAB_VALUES_NOT_DEFINED: "<<"PLEASE USE LTORG BEFORE CSECT\n";
	}

}

void makeModificationRecord(int recordStartAddress, int len, char sign, string variable){
	string modifRecord = "M";
	string modifRecordStartAddress=dec_to_hex(recordStartAddress);
	while(modifRecordStartAddress.length()<6)modifRecordStartAddress='0'+modifRecordStartAddress;
	string modifRecordLen=dec_to_hex(len);
	while(modifRecordLen.length()<2)modifRecordLen='0'+modifRecordLen;
	modifRecord+=modifRecordStartAddress+modifRecordLen;
	if(sign=='+' || sign=='-')modifRecord+=sign+variable;
	MODIF_RECORDS.push_back(modifRecord);
}

string writeTextRecord(int cur_start_addr, int LOCCTR, string textRecord){
	string cur_start_addr_string = dec_to_hex(cur_start_addr);
	while(cur_start_addr_string.length()<6)cur_start_addr_string='0'+cur_start_addr_string;
	string record_len=dec_to_hex(LOCCTR-cur_start_addr);
	while(record_len.length()<2)record_len='0'+record_len;
	return "T"+cur_start_addr_string+record_len+textRecord;
}

//Evaluate expressions for EQU, WORD and BASE. In the case of WORD, EXTREF symbols can also be used. Not so in the case of BASE and EQU
int evaluateExpression(string s,bool sameProgramFlag,int recordStartAddress, int len){
	//First converting to postfix - NO BRACKETS NEEDED THOUGH
	//Precedence of operations
	map<char,int> precedence = {
		{'*',1},{'/',1},{'+',0},{'-',0}
	};
	vector<string> postFix;
	stack<char> stac;		//Classic stack algorithm
	string temp;
	for(int i=0;i<s.length();i++){
		if(s[i]!='*' && s[i]!='/' && s[i]!='+' && s[i]!='-'){
			temp+=s[i];
		} else {
			postFix.push_back(temp);
			temp="";
			while(!stac.empty() && precedence[s[i]]<precedence[stac.top()]){
				char c=stac.top();
				stac.pop();
				temp+=c;
				postFix.push_back(temp);
				temp="";
			}
			stac.push(s[i]);
		}
	}
	if(!temp.empty()){
		postFix.push_back(temp);
		temp="";
	}
	while(!stac.empty()){
		char c=stac.top();
		temp+=c;
		stac.pop();
		postFix.push_back(temp);
		temp="";
	}

	//Evaluate postfix
	stack<string> stac2;

	for(int i=0;i<postFix.size();i++){
		if(postFix[i]!="+" && postFix[i]!="-" && postFix[i]!="*" && postFix[i]!="/"){
			temp=postFix[i];
			stac2.push(temp);
		} else {
			//Top two elements
			string t2=stac2.top();
			stac2.pop();
			string t1=stac2.top();
			stac2.pop();
			int x=0, y=0;
			bool xFound=false, yFound=false;	//To check if they are in the same program or should be externally fetched.
			//If it is a declared symbol
			if(SYMTAB.find(t1)!=SYMTAB.end()){
				if(SYMTAB[t1].find(progName)!=SYMTAB[t1].end()){
					//Same program
					xFound=true;
					x=hex_to_dec(SYMTAB[t1][progName]);
				} else if(SYMTAB[t1].find("PREDEFINED")!=SYMTAB[t1].end()){
					xFound=true;	//Register values
					x=hex_to_dec(SYMTAB[t1]["PREDEFINED"]);
				} else if(EXTREF_SYMBOLS.find(t1)==EXTREF_SYMBOLS.end()){
					UNDEFINED_SYMBOL=true;	//Not even in EXTERNAL REFERENCE symbols. Current program doesn't have access.
					return -1;
				}
			} else if(isInteger(t1)){
				//If it is an integer.
				xFound=true;
				x=stoi(t1);
			} else {
				UNDEFINED_SYMBOL=true;	//Nowhere has it been declared. It isn't even an integer.
				return -1;
			}
			//If it is a declared symbol
			if(SYMTAB.find(t2)!=SYMTAB.end()){
				if(SYMTAB[t2].find(progName)!=SYMTAB[t2].end()){
					//Same program
					yFound=true;
					y=hex_to_dec(SYMTAB[t2][progName]);
				} else if(SYMTAB[t2].find("PREDEFINED")!=SYMTAB[t2].end()){
					yFound=true;	//Register Values
					y=hex_to_dec(SYMTAB[t2]["PREDEFINED"]);
				} else if(EXTREF_SYMBOLS.find(t2)==EXTREF_SYMBOLS.end()){
					UNDEFINED_SYMBOL=true;	//Not even in EXTERNAL REFERENCE symbols. Current program doesn't have access.
					return -1;
				}
			} else if(isInteger(t2)){
				//If it is an integer
				yFound=true;
				y=stoi(t2);
			} else {
				UNDEFINED_SYMBOL=true;	//Nowhere has it been declared. It isn't even an integer.
				return -1;
			}
			//For EQU and BASE, the labels should be declared in the same section. Therefore a flag is passed to indicate this.
			if(sameProgramFlag && (!xFound || !yFound)){
				UNDEFINED_SYMBOL=true;
				return -1;
			}

			switch(postFix[i][0]){
				case '+':{
					stac2.push(int_to_string(x+y));
					//Make modification records if the symbol is not found
					if(!xFound){
						makeModificationRecord(recordStartAddress,len,'+',t1);
					}
					if(!yFound){
						makeModificationRecord(recordStartAddress,len,'+',t2);
					}
					break;
				}
				case '-':{
					stac2.push(int_to_string(x-y));
					//Make modification records if the symbol is not found
					if(!xFound){
						makeModificationRecord(recordStartAddress,len,'+',t1);
					}
					if(!yFound){
						makeModificationRecord(recordStartAddress,len,'-',t2);
					}
					break;
				}
				case '*':{
					stac2.push(int_to_string(x*y));
					//EXTREF not allowed here
					if(!xFound || !yFound){
						SYNTAX_ERROR=true;
						return -1;
					}
					break;
				}
				case '/':{
					stac2.push(int_to_string(x/y));
					//EXTREF not allowed here
					if(!xFound || !yFound){
						SYNTAX_ERROR=true;
						return -1;
					}
					break;
				}
			}
		}
	}
	if(isInteger(stac2.top())){
		return stoi(stac2.top());
	} else {
		//If the value is a symbol
		if(SYMTAB.find(stac2.top())==SYMTAB.end()){
			UNDEFINED_SYMBOL=true;	//If not in SYMTAB, UNDEFINED SYMBOL
			return -1;
		} else if(SYMTAB[stac2.top()].find(progName)!=SYMTAB[stac2.top()].end()){
			//If in SYMTAB and in current section
			return hex_to_dec(SYMTAB[stac2.top()][progName]);
		} else if(EXTREF_SYMBOLS.find(stac2.top())!=EXTREF_SYMBOLS.end()){
			//If in EXTREF SYMBOLS
			if(sameProgramFlag){
				//FOR BASE and EQU, this isn't allowed
				UNDEFINED_SYMBOL=true;
				return -1;
			}
			makeModificationRecord(recordStartAddress,len,'+',stac2.top());
			return 0;
		} else {
			//If outside scope, undefined
			UNDEFINED_SYMBOL=true;
			return -1;
		}
	}
}

int main(int argc, char ** argv){

	//Maintaining current address, start address and calculating program length
	int LOCCTR=0,start_address=0,programLength=0;
	string label="", opcode="", operand="";
	bool extendedFlag=false;
	//File input and output streams.
	fstream fin, fout;
	if(argc<2){
		cout<<"Please specify SIC assembly language file (as a .txt file) as argument!!!\n";
		return 0;
	}
	//Open file passed as argument.
	fin.open(argv[1], ios::in);
	//Intermediate file
	fout.open("intermediate.txt", ios::out);
	string line;

	//PASS 1
	while(getline(fin,line,'\n')){
		//If line is empty, skip it.
		if(stripString(line).length()==0){
			continue;
		} else if(stripString(line)[0]=='.'){
			//Comments are still printed
			fout<<"              "<<line<<'\n';
			continue;
		}

		//Extract opcode, operand and label from line.
		if(!disectLine(line,label,opcode,operand,extendedFlag)){
			if(stripString(line).length()!=0){
				//Invalid input format
				SYNTAX_ERROR=true;
				SYNTAX_ERROR_LINES+=line+'\n';
				errorHandling();
				return 0;
			}
			continue;
		}
		//Handle End statement. Address still printed in intermediate file.
		if(opcode=="END"){
			string hexLOCCTR=dec_to_hex(LOCCTR);
			while(hexLOCCTR.length()<6){
				hexLOCCTR='0'+hexLOCCTR;
			}
			fout<<hexLOCCTR<<" 0      "<<line<<'\n';
			break;
		}

		//Get program name and start_address (by default zero) from first line if it is START.
		if(opcode=="START"){
			progName=label;
			start_address=hex_to_dec(operand);
			while(operand.length()<6)operand='0'+operand;
			LOCCTR=start_address;
			//Intermediate file lines start with instruction address and intruction length/reserved space. 6 and 7 characters reserved respectively.
			fout<<operand<<" 0      "<<line<<'\n';
			continue;
		}
		//New section starts at CSECT. New program name. New start address.
		if(opcode=="CSECT"){
			//Map old program name to its length
			progNames[progName]=LOCCTR-start_address;
			progName=label;		//New program name
			start_address=0;	//new start address is 0
			LOCCTR=0;
			fout<<'\n';	//One line gap
			EXTREF_SYMBOLS.clear();
			for(auto u:LITTAB){
				//If any literal from the previous program isn't assigned a value, it's an error
				if(u.second=="00000000"){
					LITTAB_VALUES_NOT_DEFINED=true;
					errorHandling();
					return 0;
				}
			}
		}
		if(opcode=="EXTREF"){
			//Identify individual symbols
			vector<string> definitions;
			string temporary="";
			for(int i=0;i<operand.length();i++){
				if(operand[i]!=','){
					temporary+=operand[i];
				} else {
					definitions.push_back(stripString(temporary));
					temporary="";
				}
			}
			if(temporary.length()){
				definitions.push_back(stripString(temporary));
				temporary="";
			}
			for(int i=0;i<definitions.size();i++){
				EXTREF_SYMBOLS.insert(definitions[i]);
			}
		}

		//Get hex value of LOCCTR
		string hexLOCCTR=dec_to_hex(LOCCTR);
		//Fix length of string
		while(hexLOCCTR.length()<6){
			hexLOCCTR='0'+hexLOCCTR;
		}

		//If line is not comment
		if(stripString(line)[0]!='.'){
			//If there is a label, add that label to SYMTAB. If duplicate (in the same section), set error flag.
			if(label!=""){
				//If either not in symtable at all or irrelevant to current section.
				if(SYMTAB.find(label)==SYMTAB.end() || (SYMTAB[label].find(progName)==SYMTAB[label].end() \
					&& SYMTAB[label].find("PREDEFINED")==SYMTAB[label].end() && EXTREF_SYMBOLS.find(label)==EXTREF_SYMBOLS.end())){
					if(opcode=="EQU"){
						//If LOCCTR value is to be used
						if(operand=="*"){
							SYMTAB[label][progName]=dec_to_hex(LOCCTR);
						} else {
							//Please check input parameters of evaluate expression.
							//This means that only symbols within the section are relevant.
							int expressionValue=evaluateExpression(operand,true,0,0);
							if(expressionValue==-1){
								if(SYNTAX_ERROR){
									SYNTAX_ERROR_LINES+=line+'\n';
								}
								if(UNDEFINED_SYMBOL){
									UNDEFINED_SYMBOL_LINE+=line+'\n';
								}
								errorHandling();
								return 0;
							}
							SYMTAB[label][progName]=dec_to_hex(expressionValue);
						}
					} else {
						//Just store the address otherwise
						SYMTAB[label][progName]=dec_to_hex(LOCCTR);
					}
				} else {
					DUPLICATE_SYMBOL=true;
					DUPLICATE_SYMBOL_LINE+=label+" ";
					errorHandling();
					return 0;
				}
			}
			if(OPTAB.find(opcode)!=OPTAB.end()){
				//Writing address and length of different types of instructions into intermediate file
				if(opcode=="COMPR" || opcode=="CLEAR" || opcode=="TIXR"){
					//Format 2 instructions
					LOCCTR+=2;
					fout<<hexLOCCTR<<" 2      ";
				} else {
					if(extendedFlag){
						//Format 4
						LOCCTR+=4;
						fout<<hexLOCCTR<<" 4      ";
					} else {
						//Format 3
						LOCCTR+=3;
						fout<<hexLOCCTR<<" 3      ";
					}
				}
				//Literal
				if(operand[0]=='='){
					if(LITTAB.find(operand)==LITTAB.end()){
						LITTAB[operand]="00000000";	//Default value 00000000
					}
				}

			} else {
				if(opcode=="WORD"){
					LOCCTR+=3;						//One word=3 bytes
					fout<<hexLOCCTR<<" 3      ";	//Same reason as above
				}
				else if(opcode=="RESW"){
					LOCCTR+=3*stoi(operand);		//One word=3 bytes
					string lengthOfOperand = int_to_string(3*stoi(operand));
					while(lengthOfOperand.length()<7)lengthOfOperand+=' ';	//Padding to make length 7 bytes.
					fout<<hexLOCCTR<<' '<<lengthOfOperand;					//Printing in intermediate file.
				}
				else if(opcode=="RESB"){
					LOCCTR+=stoi(operand);
					string lengthOfOperand = int_to_string(stoi(operand));
					while(lengthOfOperand.length()<7)lengthOfOperand+=' ';
					fout<<hexLOCCTR<<' '<<lengthOfOperand;					//Printing number of reserved bytes in intermediate file
				}
				else if(opcode=="BYTE"){
					if(operand.length()<3){			  //Minimum characters=3 (C'' or X'')
						//Set syntax error flag
						SYNTAX_ERROR=true;
						SYNTAX_ERROR_LINES+=line+'\n';
						errorHandling();
						return 0;
					} else if(operand[0]=='X' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
						int intermediate=operand.length()-3;
						if(intermediate%2){
							//If odd length apply padding
							cout<<"HEXADECIMAL BYTE OPERAND HAS ODD LENGTH. PADDING WITH 0.\n";
							operand.pop_back();
							operand+="0\'";
							intermediate++;
							line=createLine(label,opcode,operand);
						}
						intermediate = intermediate/2;
						LOCCTR+=intermediate;
						string lengthOfOperand = int_to_string(intermediate);
						while(lengthOfOperand.length()<7)lengthOfOperand+=' ';
						fout<<hexLOCCTR<<' '<<lengthOfOperand;
					} else if(operand[0]=='C' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
						LOCCTR+=operand.length()-3;	//Length of string in bytes
						string lengthOfOperand = int_to_string(operand.length()-3);
						while(lengthOfOperand.length()<7)lengthOfOperand+=' ';
						fout<<hexLOCCTR<<' '<<lengthOfOperand;
					} else {					//First character should either be C or X.
						SYNTAX_ERROR=true;
						SYNTAX_ERROR_LINES+=line+'\n';
						errorHandling();
						return 0;
					}
				} else if(opcode=="LTORG"){
					fout<<hexLOCCTR<<" 0      "<<line<<'\n';	//Write LTORG command into intermediate file
					for(auto u:LITTAB){		//Assign addresses to all Literal values in LITTAB
						if(u.second!="00000000"){
							continue;
						}
						operand=u.first;
						operand=operand.substr(1);
						if(operand.length()<3){			  //Minimum characters=3 (C'' or X'')
							//Set syntax error flag
							SYNTAX_ERROR=true;
							SYNTAX_ERROR_LINES+=line+'\n';
							errorHandling();
							return 0;
						} else if(operand[0]=='X' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
							int intermediate=operand.length()-3;
							if(intermediate%2){
								//If odd length, apply padding
								cout<<"HEXADECIMAL BYTE OPERAND HAS ODD LENGTH. PADDING WITH 0.\n";
								operand.pop_back();
								operand+="0\'";
								intermediate++;
							}
							intermediate = intermediate/2;
							//Save into LITTAB
							LITTAB[u.first]=dec_to_hex(LOCCTR);
							line=createLine("*",u.first,"");
							string hex=dec_to_hex(LOCCTR);
							while(hex.length()<6)hex='0'+hex;
							string len=int_to_string(intermediate);
							while(len.length()<7)len+=' ';
							fout<<hex<<' '<<len<<line<<'\n';	//Write into intermediate
							LOCCTR+=intermediate;
						} else if(operand[0]=='C' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
							//Save into LITTAB
							LITTAB[u.first]=dec_to_hex(LOCCTR);
							line=createLine("*",u.first,"");
							string hex=dec_to_hex(LOCCTR);
							while(hex.length()<6)hex='0'+hex;
							string len=int_to_string(operand.length()-3);
							while(len.length()<7)len+=' ';
							fout<<hex<<' '<<len<<line<<'\n';	//Write into intermediate
							LOCCTR+=operand.length()-3;
						} else {					//First character should either be C or X.
							SYNTAX_ERROR=true;
							SYNTAX_ERROR_LINES+=line+'\n';
							errorHandling();
							return 0;
						}
					}
					continue;
				} else if(opcode=="EQU" || opcode=="EXTREF" || opcode=="EXTDEF" || opcode=="BASE" || opcode=="CSECT"){
					fout<<hexLOCCTR<<" 0      ";
				} else {	//Unknown opcode. Not comment either.
					INVALID_OPERATION_CODE=true;
					INVALID_OPERATION_CODE_LINE+=line+'\n';
					errorHandling();
					return 0;
				}			
			}
		} else line=stripString(line);	//If comment, remove leading and trailing spaces.
		fout<<line<<'\n';
	}

	//Literal pool at the end of the program.
	for(auto u:LITTAB){
		if(u.second!="00000000"){
			continue;
		}
		operand=u.first;
		operand=operand.substr(1);
		if(operand.length()<3){			  //Minimum characters=3 (C'' or X'')
			//Set syntax error flag
			SYNTAX_ERROR=true;
			SYNTAX_ERROR_LINES+=line+'\n';
			errorHandling();
			return 0;
		} else if(operand[0]=='X' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
			int intermediate=operand.length()-3;
			if(intermediate%2){
				//If odd, apply padding
				cout<<"HEXADECIMAL BYTE OPERAND HAS ODD LENGTH. PADDING WITH 0.\n";
				operand.pop_back();
				operand+="0\'";
				intermediate++;
			}
			intermediate = intermediate/2;
			//Write to LITTAB
			LITTAB[u.first]=dec_to_hex(LOCCTR);
			line=createLine("*",u.first,"");
			string hex=dec_to_hex(LOCCTR);
			while(hex.length()<6)hex='0'+hex;
			string len=int_to_string(intermediate);
			while(len.length()<7)len+=' ';
			fout<<hex<<' '<<len<<line<<'\n';
			LOCCTR+=intermediate;
		} else if(operand[0]=='C' && operand[1]=='\'' && operand[operand.length()-1]=='\''){
			//Write to LITTAB
			LITTAB[u.first]=dec_to_hex(LOCCTR);
			line=createLine("*",u.first,"");
			string hex=dec_to_hex(LOCCTR);
			while(hex.length()<6)hex='0'+hex;
			string len=int_to_string(operand.length()-3);
			while(len.length()<7)len+=' ';
			fout<<hex<<' '<<len<<line<<'\n';
			LOCCTR+=operand.length()-3;
		} else {					//First character should either be C or X.
			SYNTAX_ERROR=true;
			SYNTAX_ERROR_LINES+=line+'\n';
			errorHandling();
			return 0;
		}
	}
	//Map section name with section length
	progNames[progName]=LOCCTR-start_address;

	//Close streams. For safety.
	fin.close();
	fout.close();

	//PASS 2
	//Read from intermediate file and write to object file.
	fin.open("intermediate.txt", ios::in);
	fout.open("object.txt", ios::out);

	bool CSECT_flag=false;	//Flag used to write end symbol
	int cur_start_addr=0;	//Current start address
	string textRecord;	//To store current text record

	while(getline(fin,line,'\n')){
		//If empty of comment, continue.
		if(stripString(line)=="" || stripString(line)[0]=='.'){
			continue;
		}
		string addr=line.substr(0,6);	//Address of instruction
		string len=line.substr(7,6);	//Length of instruction
		line=line.substr(14);			//The instruction line
		disectLine(line,label,opcode,operand,extendedFlag);	//Obtain label, opcode and operand from line
		//Get first section name and starting address
		if(opcode=="START"){
			progName=label;
			//Create Header record;
			string newProgName=progName;
			while(newProgName.length()<6)newProgName+=' ';	//Fix length
			start_address=hex_to_dec(operand);
			cur_start_addr=start_address;
			LOCCTR=start_address;	//Start counting at start address
			EXTREF_SYMBOLS.clear();
			string startAddressString=operand;
			while(startAddressString.length()<6)startAddressString='0'+startAddressString;	//Fix length
			int progLength=progNames[progName];	//Length of first section
			string progLengthString=dec_to_hex(progLength);
			while(progLengthString.length()<6)progLengthString='0'+progLengthString;	//Fix length
			fout<<'H'<<newProgName<<startAddressString<<progLengthString<<'\n';	//Write header record
		}
		//New section
		if(opcode=="CSECT"){
			//Write current text record. Wrap up previous section
			if(textRecord.length()){
				//Write current record into object code
				fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
				textRecord="";	//Ready for next instruction
			}
			//Write down modification records
			for(auto u:MODIF_RECORDS){
				fout<<u<<'\n';
			}
			MODIF_RECORDS.clear();	//Empty the list for future records
			//If previous section was first section.
			if(!CSECT_flag){
				//Write end record
				string startAddressString=int_to_string(start_address);
				while(startAddressString.length()<6)startAddressString='0'+startAddressString;
				fout<<'E'<<startAddressString<<'\n';
			} else {
				fout<<"E\n";
			}
			fout<<'\n';	//One blank line
			CSECT_flag=true;
			start_address=0;	//Start at address 0
			LOCCTR=start_address;
			cur_start_addr=start_address;
			EXTREF_SYMBOLS.clear();	//New EXTREF SYMBOLS need to be filled
			progName=label;
			string newProgName=progName;
			while(newProgName.length()<6)newProgName+=' ';	//Fix length
			int progLength=progNames[progName];
			string progLengthString=dec_to_hex(progLength);
			while(progLengthString.length()<6)progLengthString='0'+progLengthString;	//Fix length
			fout<<'H'<<newProgName<<"000000"<<progLengthString<<'\n';	//Start address is 000000
		}

		if(opcode=="EXTDEF"){
			//Identify individual symbols
			vector<string> definitions;
			string temporary="";
			for(int i=0;i<operand.length();i++){
				if(operand[i]!=','){
					temporary+=operand[i];
				} else {
					definitions.push_back(stripString(temporary));
					temporary="";
				}
			}
			if(temporary.length()){
				definitions.push_back(stripString(temporary));
				temporary="";
			}
			//Write define record
			fout<<'D';
			for(int i=0;i<definitions.size();i++){
				if(SYMTAB.find(definitions[i])==SYMTAB.end() || SYMTAB[definitions[i]].find(progName)==SYMTAB[definitions[i]].end()){
					//The symbol must be defined in this program. ERROR
					ILLEGAL_EXTDEF=true;
					ILLEGAL_EXTDEF_LINE+=line+'\n';
					errorHandling();
					return 0;
				} else {
					//Write into define record
					string temp=definitions[i];
					while(temp.length()<6)temp+=' ';
					string addrString = SYMTAB[definitions[i]][progName];
					while(addrString.length()<6)addrString='0'+addrString;
					fout<<temp<<addrString;
				}
			}
			fout<<'\n';
			continue;
		} else if(opcode=="EXTREF"){
			//Identify individual symbols
			vector<string> definitions;
			string temporary="";
			for(int i=0;i<operand.length();i++){
				if(operand[i]!=','){
					temporary+=operand[i];
				} else {
					definitions.push_back(stripString(temporary));
					temporary="";
				}
			}
			if(temporary.length()){
				definitions.push_back(stripString(temporary));
				temporary="";
			}
			//Write Refer record
			fout<<'R';
			for(int i=0;i<definitions.size();i++){
				temporary=definitions[i];
				EXTREF_SYMBOLS.insert(temporary);
				while(temporary.length()<6)temporary+=' ';
				fout<<temporary;
			}
			fout<<'\n';
			continue;
		} else if(opcode=="BASE"){
			//Helps the assembler compute base relative displacement
			int expressionValue=evaluateExpression(operand,true,0,0);
			if(expressionValue==-1){
				//Refer to evaluateExpression() for meaning
				if(SYNTAX_ERROR){
					SYNTAX_ERROR_LINES+=line+'\n';
					errorHandling();
					return 0;
				}
				if(UNDEFINED_SYMBOL){
					UNDEFINED_SYMBOL_LINE+=line+'\n';
					errorHandling();
					return 0;
				}
			}
			base_register_value=expressionValue;
		} else if(OPTAB.find(opcode)!=OPTAB.end()){
			if(opcode=="COMPR" || opcode=="CLEAR" || opcode=="TIXR"){
				//Format 2
				string instruction=OPTAB[opcode];
				vector<string> registers;
				string temporary;
				for(int i=0;i<operand.length();i++){
					if(operand[i]!=','){
						temporary+=operand[i];
					} else {
						registers.push_back(temporary);
						temporary="";
					}
				}
				if(temporary.length()){
					registers.push_back(temporary);
				}
				if(opcode=="COMPR" && (registers.size()!=2 || SYMTAB.find(registers[0])==SYMTAB.end() \
					|| SYMTAB.find(registers[1])==SYMTAB.end() || SYMTAB[registers[0]].find("PREDEFINED")==SYMTAB[registers[0]].end() \
					|| SYMTAB[registers[1]].find("PREDEFINED")==SYMTAB[registers[1]].end())){
					//COMPR has two operands and both must be predefined registers
					SYNTAX_ERROR=true;
					SYNTAX_ERROR_LINES+=line;
					errorHandling();
					return 0;
				} else if(opcode!="COMPR" && (registers.size()!=1 || SYMTAB.find(registers[0])==SYMTAB.end() \
					|| SYMTAB[registers[0]].find("PREDEFINED")==SYMTAB[registers[0]].end())){
					//For the other two, one register operand
					SYNTAX_ERROR=true;
					SYNTAX_ERROR_LINES+=line;
					errorHandling();
					return 0;
				}
				if(opcode=="COMPR"){
					//Add the register identification values to the instruction
					instruction+=SYMTAB[registers[0]]["PREDEFINED"];
					instruction+=SYMTAB[registers[1]]["PREDEFINED"];	
				} else {
					//Add the register identification values to the instruction
					instruction+=SYMTAB[registers[0]]["PREDEFINED"];
					instruction+='0';
				}
				if(LOCCTR+2-cur_start_addr>30){
					//Write text record
					fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
					textRecord=instruction;
					cur_start_addr=LOCCTR;
				} else {
					textRecord+=instruction;
				}
				LOCCTR+=2;	//2 BYTE instruction
			} else {
				if(extendedFlag){
					string instruction=OPTAB[opcode];
					//To help with flags;
					int integer_instruction=hex_to_dec(instruction)*(1<<24);
					integer_instruction+=(1<<20);	//Extended
					if(operand[0]=='#'){
						//Immediate i=1, n=0
						integer_instruction+=(1<<24);
						operand=operand.substr(1);
					} else if(operand[0]=='@'){
						//Indirect i=0, n=1
						integer_instruction+=(1<<25);
						operand=operand.substr(1);
					} else {
						//Otherwise i=0 and n=0
						integer_instruction+=(1<<25)+(1<<24);
					}
					for(int i=0;i<operand.length();i++){
						if(operand[i]==','){
							//Indexed e=1
							integer_instruction+=(1<<23);
							operand=operand.substr(0,i);
							break;
						}
					}
					if(operand.length()>0 && isInteger(operand)){
						//If operand is an integer
						integer_instruction+=stoi(operand);
					} else if(LITTAB.find(operand)!=LITTAB.end()){
						//If it is a literal
						int value=hex_to_dec(LITTAB[operand]);
						integer_instruction+=value;
					} else if(SYMTAB.find(operand)!=SYMTAB.end()){
						//If operand found
						if(SYMTAB[operand].find(progName)!=SYMTAB[operand].end()){
							//If in same section
							integer_instruction+=hex_to_dec(SYMTAB[operand][progName]);
							//The first modification record format is to be used here since the symbol is in the same program
							makeModificationRecord(LOCCTR+1,5,'/',operand);
						} else if(EXTREF_SYMBOLS.find(operand)==EXTREF_SYMBOLS.end()){
							//If not even in EXTREF
							UNDEFINED_SYMBOL=true;
							UNDEFINED_SYMBOL_LINE+=line+'\n';
							errorHandling();
							return 0;
						} else if(EXTREF_SYMBOLS.find(operand)!=EXTREF_SYMBOLS.end()){
							//Make modification record in new format since symbol is in EXTREF
							makeModificationRecord(LOCCTR+1,5,'+',operand);
						}
					}
					//Make string instruction
					instruction=dec_to_hex(integer_instruction);
					while(instruction.length()<8)instruction='0'+instruction;
					if(hex_to_dec(addr)+4-cur_start_addr>30){
						fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<endl;	//Write the record
						textRecord=instruction;	//Make new text record.
						cur_start_addr=LOCCTR;
					} else {
						//Otherwise append to previous record
						textRecord+=instruction;
					}
					LOCCTR+=4;	//4-Byte instruction
				} else {
					//For program counter relative addressing, PC value is required
					int PC_value=LOCCTR+3;
					string instruction = OPTAB[opcode];
					int integer_instruction=hex_to_dec(instruction)*(1<<16);	//To help with flags
					if(operand.length()!=0){
						if(operand[0]=='#'){
							//Immediate i=1, n=0
							integer_instruction+=(1<<16);
							operand=operand.substr(1);
						} else if(operand[0]=='@'){
							//Indirect i=0, n=1
							integer_instruction+=(1<<17);
							operand=operand.substr(1);
						} else {
							//Otherwise both 1
							integer_instruction+=(1<<16)+(1<<17);
						}
						for(int i=0;i<operand.length();i++){
							if(operand[i]==','){
								//Indexed
								integer_instruction+=(1<<15);
								operand=operand.substr(0,i);
								break;
							}
						}
					} else {
						//Both i and n, 1 otherwise
						integer_instruction+=(1<<16)+(1<<17);
					}
					if(operand.length()==0){
						//For example RSUB
						instruction=dec_to_hex(integer_instruction);
						while(instruction.length()<6)instruction='0'+instruction;	//Fix length
					} else if(isInteger(operand)){	//If operand is a constant integet
						integer_instruction+=stoi(operand);
						instruction=dec_to_hex(integer_instruction);
						while(instruction.length()<6)instruction='0'+instruction;	//fix length
					} else if(LITTAB.find(operand)!=LITTAB.end()){	//If literal
						//Get the address of the literal
						int value=hex_to_dec(LITTAB[operand]);
						if((value>PC_value && value-PC_value<=2047) || (PC_value>value && PC_value-value<=2048)){
							//Check for PC relative
							int disp=value-PC_value;
							if(disp<0){
								//Two's complement - 12 bit
								disp=-disp;
								disp=((((~disp)&(0xFFF))+1)&(0xFFF));
							}
							integer_instruction+=disp;
							integer_instruction+=(1<<13);	//p=1
						} else if(value>=base_register_value && value-base_register_value<4096){
							//If PC didn't work, try base
							int disp=value-base_register_value;
							integer_instruction+=disp;
							integer_instruction+=(1<<14);	//b=1
						} else {
							//If none worked, error
							DISPLACEMENT_OUT_OF_BOUNDS=true;
							DISPLACEMENT_OUT_OF_BOUNDS_STRING+=line+'\n';
							errorHandling();
							return 0;
						}
						instruction=dec_to_hex(integer_instruction);
						while(instruction.length()<6)instruction='0'+instruction;	//Fix length
					} else if(SYMTAB.find(operand)!=SYMTAB.end() && SYMTAB[operand].find(progName)!=SYMTAB[operand].end()){
						//If in SYMTAB
						//First attempt with PC
						int value=hex_to_dec(SYMTAB[operand][progName]);

						if((value>=PC_value && value-PC_value<=2047) || (PC_value>value && PC_value-value<=2048)){
							int disp=value-PC_value;
							if(disp<0){
								//Two's complement
								disp=-disp;
								disp=((((~disp)&(0xFFF))+1)&(0xFFF));
							}
							integer_instruction+=disp;
							integer_instruction+=(1<<13);	//p=1
						} else if(value>=base_register_value && value-base_register_value<4096){
							//Try base register
							int disp=value-base_register_value;
							integer_instruction+=disp;
							integer_instruction+=(1<<14);	//b=1
						} else {
							//None worked
							DISPLACEMENT_OUT_OF_BOUNDS=true;
							DISPLACEMENT_OUT_OF_BOUNDS_STRING+=line+'\n';
							errorHandling();
							return 0;
						}
						instruction=dec_to_hex(integer_instruction);
						while(instruction.length()<6)instruction='0'+instruction;	//Fix length
					} else {
						//In format 3 instructions, EXTREF is not possible
						UNDEFINED_SYMBOL=true;
						UNDEFINED_SYMBOL_LINE=line;
						errorHandling();
						return 0;
					}

					if(LOCCTR+3-cur_start_addr>30){
						//Write old record into object file and generate new record
						fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
						textRecord=instruction;
						cur_start_addr=hex_to_dec(addr);
					} else {
						textRecord+=instruction;	//Just append instruction into previous record
					}
					LOCCTR+=3;
				}
			}
		} else {
			if(opcode=="RESW" || opcode=="RESB"){
				//Calculate requied space
				int total=(opcode=="RESW")?(stoi(operand)*3):stoi(operand);
				if(textRecord.length()){
					//Write current record into object file
					fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
					textRecord="";	//Generate new record
				}
				//Skip requied space
				LOCCTR+=total;
				cur_start_addr=LOCCTR;
			} else if(opcode=="WORD"){
				if(operand.length()==0){
					operand="0";
				}
				int expressionValue=evaluateExpression(operand,false,LOCCTR,6);
				if(expressionValue==-1){
					//Please refer to evaluateExpression to know what this means
					if(SYNTAX_ERROR){
						SYNTAX_ERROR_LINES+=line+'\n';
						errorHandling();
						return 0;
					}
				}
				string instruction = dec_to_hex(expressionValue);
				while(instruction.length()<6)instruction='0'+instruction;	//Fix length
				if(LOCCTR+3-cur_start_addr>30){
					//Write record to object file
					fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
					textRecord=instruction;
					cur_start_addr=hex_to_dec(addr);
				} else {
					textRecord+=instruction;
				}
				LOCCTR+=3;
			} else if(opcode=="BYTE" || label=="*"){
				//If literal
				if(label=="*"){
					operand=opcode.substr(1);
				}
				string instruction=operand;
				if(operand[0]=='X'){
					//If hexadecimal, just write the hexadecimal string to file.
					//Remove X'' from string.
					instruction.pop_back();	//Remove last '
					if(instruction.length()>=2)instruction=instruction.substr(2); //Remove X'
					else instruction="";
				} else if(operand[0]=='C'){
					//If string, break into hexadecimal parts using getHexBreakup.
					instruction.pop_back();
					if(instruction.length()>=2){
						instruction=instruction.substr(2);
						instruction=getHexBreakup(instruction);
					}
					else instruction="";
				}
				if(hex_to_dec(addr)+instruction.length()/2-cur_start_addr > 30){
					//Write to object file
					fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
					textRecord=instruction;
					cur_start_addr=hex_to_dec(addr);
				} else {
					textRecord+=instruction;
				}
				//#bytes=#hex_characters/2
				LOCCTR+=instruction.length()/2;
			}
		}
	}
	//Write final text record
	if(textRecord.length()){
		fout<<writeTextRecord(cur_start_addr,LOCCTR,textRecord)<<'\n';
		textRecord="";
	}
	//Final modification records
	for(auto u:MODIF_RECORDS){
		fout<<u<<'\n';
	}
	MODIF_RECORDS.clear();
	//End record
	if(!CSECT_flag){
		string startAddressString=int_to_string(start_address);
		while(startAddressString.length()<6)startAddressString='0'+startAddressString;
		fout<<'E'<<startAddressString<<'\n';
	} else {
		fout<<"E\n";
	}
}