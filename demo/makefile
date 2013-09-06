RPI_LIB = httprequest

CXXFLAGS = -Wall -O3 -g -I..
LDFLAGS = -L..
LIBS = -l$(RPI_LIB)
TARGET = demo

SRCS = Demo.cpp
OBJS = $(SRCS:.cpp=.o)


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
