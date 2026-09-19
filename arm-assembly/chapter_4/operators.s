/* AND */
// mask off the higher order byte
	AND W6, W6, #0xFF000000
	// shift the byte down to the 
	// low order position.
	LSR W6, W6, #24
