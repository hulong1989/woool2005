# Makefile —— 重建骨架的构建
#   macOS / Linux : make
#   Windows+MinGW : make CXX=g++    （会自动链接 ws2_32/winmm，且不引入 POSIX 兼容头）
#   Windows+MSVC  : build.bat

CXX      ?= clang++
CXXFLAGS ?= -std=c++11 -g -O0 -Wall -Wno-unused-parameter -Wno-reorder
# 自动生成头文件依赖，避免修改头文件后仍复用旧目标文件
CXXFLAGS += -MMD -MP
CPPFLAGS := -I. -Ipublic -Ires
LDFLAGS  ?=
LDLIBS   :=

SOURCES := $(wildcard *.cpp) public/public.cpp

ifeq ($(OS),Windows_NT)
  LDLIBS += -lws2_32 -lwinmm
else
  # 非 Windows 才需要 Win32 兼容层；否则 compat/windows.h 会遮蔽系统头
  CPPFLAGS += -Icompat
  SOURCES  += compat/compat.cpp
endif

BUILD ?= build
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SOURCES))
DEPS := $(OBJS:.o=.d)
BIN  := $(BUILD)/GameServer

.PHONY: all run test smoke client client-test clean

all: $(BIN)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

run: $(BIN)
	./$(BIN)

test: $(BIN)
	@printf 'addbot 冒烟\nstatus\nexit\n' | ./$(BIN)

smoke: $(BIN)
	python3 tools/smoke_test.py

client: $(BIN)
	python3 client/game_client.py

client-test: $(BIN)
	python3 tools/client_test.py

clean:
	rm -rf build build-asan

-include $(DEPS)
