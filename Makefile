DEF= -DCOMPILE_DATE=\"$(DATE)\" -DCOMPILE_TIME=\"$(TIME)\" -DVERSION=\"$(VER)\"

PROJ_ROOT =/home/dc/benchmark
INC = -I$(PROJ_ROOT)/
LIB = -lpthread -ldl -lz -lm 

C_FLAGS =  -O0  $(DEF) -g  -Wall  -pg
#C_FLAGS =  -O0  $(DEF) -g -fomit-frame-pointer -Wall  
CXX             = g++ 
RANLIB          = ranlib
AR              = ar
SRCS = $(wildcard *.cpp module/*.cpp )
OBJS = $(patsubst %.cpp,%.o,$(SRCS))
TARGET = benchmark 
OK = \\e[1m\\e[32m OK \\e[m
FAILURE = \\e[1m\\e[31m FAILURE \\e[m
all:$(TARGET)

$(TARGET):$(OBJS)
		@echo -ne Linking $(TARGET) ... 
		@$(CXX) $(C_FLAGS)  $(INC) -o $@ $^ $(LIB) && echo  -e $(OK) || echo -e $(FAILURE)
		@rm -f *.o
%.o:%.cpp
		@echo -ne Compiling $<  ... 
		@$(CXX) $(C_FLAGS)  $(INC) -c -o $@ $< && echo  -e $(OK) || echo -e $(FAILURE)

	
clean:
	@rm -f *.o 
	@rm -f ./$(TARGET)
install:clean all
	strip $(TARGET) 
