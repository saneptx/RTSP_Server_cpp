CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread
INCLUDES = -I.
LIBS = -lpthread -llog4cpp

# 源文件
REACTOR_SOURCES = $(wildcard reactor/*.cc)
MEDIA_SOURCES = $(wildcard media/*.cc)
MAIN_SOURCE = RtspServer.cc

# 目标文件
REACTOR_OBJECTS = $(REACTOR_SOURCES:.cc=.o)
MEDIA_OBJECTS = $(MEDIA_SOURCES:.cc=.o)
MAIN_OBJECT = $(MAIN_SOURCE:.cc=.o)

# 可执行文件
TARGET = rtsp_server

# 默认目标
all: $(TARGET)

# 编译多线程服务器
$(TARGET): $(REACTOR_OBJECTS) $(MEDIA_OBJECTS) $(MAIN_OBJECT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# 编译规则
%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -f $(REACTOR_OBJECTS) $(MEDIA_OBJECTS) $(MAIN_OBJECT) $(TARGET)

# 运行
run: $(TARGET)
	./$(TARGET)

# 调试版本
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all clean run debug 