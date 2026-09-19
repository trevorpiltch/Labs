ifdef DEBUG
DEBUGFLGS = -g
else 
DEBUGFLGS = 
endif 

# Define the default target
all: $(TARGET)

# Define the target executable
$(TARGET): $(TARGET).o
	ld -o $@ $^

# Define the object file target
$(TARGET).o: $(TARGET).s
	as $(DEBUGFLGS) -o $@ $<

# Define clean target to remove generated files
clean:
	rm -f $(TARGET) $(TARGET).o

