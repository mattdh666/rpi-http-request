CXXFLAGS = -Wall -O3 -g -I..
LDFLAGS =
LIBS =
TARGET = demo

SRCS = Demo.cpp HttpRequest.cpp HttpResponse.cpp HttpException.cpp
OBJS = $(SRCS:.cpp=.o)


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
