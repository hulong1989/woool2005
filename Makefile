# Makefile —— 重建骨架的构建
#   macOS / Linux : make
#   Windows+MinGW : make CXX=g++    （会自动链接 ws2_32/winmm，且不引入 POSIX 兼容头）
#   Windows+MSVC  : build.bat

CXX      ?= clang++
VERSION  ?= v0.1.1
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

.PHONY: all run test smoke validate links check package screenshots client client-test clean

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

validate:
	@python3 tools/validate_data.py

links:
	@python3 tools/check_links.py

# 一键：数据校验 + 文档链接检查 + 176 项回归（回归在隔离副本中跑，不脏工作区）
check: validate links $(BIN)
	@python3 tools/run_smoke_isolated.py

# 生成可运行的 server 发布包（-O2 构建到 build-release/，输出 dist/*.tar.gz）
package: $(BIN)
	@python3 tools/package_release.py --version $(VERSION) --build

# 需要先另开终端启动 ./build/GameServer
screenshots: $(BIN)
	python3 tools/capture_screenshots.py

client: $(BIN)
	python3 client/game_client.py

client-test: $(BIN)
	python3 tools/client_test.py

clean:
	rm -rf build build-asan build-release

-include $(DEPS)
