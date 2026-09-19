/* FOR LOOP */
	MOV W2, #1 	// W2 holds I
loop:	 		// body of the loop goes here.
			// Most of the logic is at the end
	ADD W2, W2, #1 	// I = I + 1   
	CMP W2, #10
	B.LE loop	// IF <= 10 goto loop 

/* WHILE LOOP */
loop: 	CMP W4, #5
	B.GE loopdone
	// ... other statements in the loop body ...
	B loop
loopdone: // program continues	

	
	
