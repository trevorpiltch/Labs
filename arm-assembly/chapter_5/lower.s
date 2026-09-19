//
// Assembler program to convert to 
// all lower case.
// 
// X0-X2 - parameters to Linux function services
// X3 - address of output string
// X4 - address of input string
// W5 - current character being processed
// X8 - linux function nummber
//
.global _start	// Provide program starting address to linker
_start: LDR	X4, =instr	// start of input string	
	LDR	X3, =outstr	// address of ouput string
// The loop is until byte pointed to by X1 is non-zero
loop: 	LDRB	W5, [X4], #1 	// load character and inc pointer
// If W5 > 'Z' then goto cont
	CMP 	W5, #'Z'	
	B.GT 	cont
// Else if W5C > 'A' then goto end if'
	CMP 	W5, #'A'
	B.LT 	cont
// If we got here then the letter is upper case, so convert it
	SUB	W5, W5, #('A'-'a')
cont: // end if
	STRB    W5, [X3], #1    // store character to output str
        CMP     W5, #0          // stop on hitting a null character
        B.NE    loop            // loop if character isn't null
// Setup the parameters to print our hex number
// and then call Linux to do it.
        MOV     X0, #1          // 1 = StdOut
        LDR     X1, =outstr     // string to print
        SUB     X2, X3, X1      // get the len by sub'ing the pointers
        MOV     X8, #64         // Linux write system call
        SVC     0               // Call  Linux to ouput the string
// Setup the parameters to exit the program
// and then call Linux to do it.
        MOV     X0, #0          // Use 0 return code
        MOV     X8, #93         // Service code 93 terminates
        SVC     0               // Call Linux to terminate the program
.data
instr:          .asciz "THIS is OUR tEST sTRING THAT WE WILL CONVERT.\n"
outstr:         .fill   255, 1, 0
