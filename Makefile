CXX = g++
LDFLAGS = -lglfw -lGL -lGLU

TARGET = fase3
SRC = Projeto_Flight_Sim.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
