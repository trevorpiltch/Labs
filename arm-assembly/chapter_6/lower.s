//
// Assembler program to convert a string to
// all lower case.
//
// X1 - address of output string
// X0 - address of input string
// X4 - original output string for length calc.
// W5 - current character being processed
// 
.global tolower
tolower: MOV	 X4, X1
loop: LDRB W5, [X0], #1
// If W5 > 'Z' then goto cont
	CMP 	W5, #'Z'
	B.GT 	cont
// Else if W5 < 'A' then goto end if
	CMP 	W5, #'A'
	B.LT 	cont
// if we got here then the letter is upper case, 
// so convert it
	SUB	W5, W5, #('A'-'a')
cont: 
	STRB	W5, [X1], #1
	CMP	W5, #0
	B.NE 	loop
	SUB	X0, X1, X4
	RET

