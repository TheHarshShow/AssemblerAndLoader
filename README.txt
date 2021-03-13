RUNNING INSTRUCTIONS
Compile in LINUX G++ (USE C++17)
assembler:
1. cd to directory containing assembler.cpp and input.txt
2. g++ assembler.cpp
3. ./a.out input.txt
4. Output in object.txt

linking loader:
1. cd to folder containing linking loader
2. g++ linking_loader.cpp
3. ./a.out
4. MAIN OUTPUT in LINKER_INSTRUCTIONS.txt. Full Memory map in MEMORY_MAP.txt

Instruction format in input.txt
Every instruction should be at least 21 characters long and at max 80 characters long. First 10 characters for LABEL, next 10 for OPCODE and next 60 for OPERAND.