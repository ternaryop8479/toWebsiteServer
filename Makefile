# 项目名称
PROJECT_NAME = to_https_server

# 库名称（静态库）
LIB_NAME = libto_https_server.a

# 目录结构
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
EXTERNAL_DIR = include/external

# 源文件
SOURCES = $(wildcard $(SRC_DIR)/**/*.cpp) $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 编译器
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I$(INCLUDE_DIR) -I$(EXTERNAL_DIR)

# 静态库工具
AR = ar
ARFLAGS = rcs

# 安装路径
INSTALL_LIB_PATH = /usr/local/lib
INSTALL_INCLUDE_PATH = /usr/local/include/

# 默认目标
.PHONY: all clean build install uninstall dirs

all: build

# 创建构建目录
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BUILD_DIR)/utils

# 编译对象文件（添加 -fPIC 以支持位置无关代码）
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# 构建静态库
$(LIB_NAME): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

build: $(LIB_NAME)

# 清理构建文件
clean:
	rm -rf $(BUILD_DIR) $(LIB_NAME)

# 安装到系统
install: build
	sudo mkdir -p $(INSTALL_LIB_PATH)
	sudo mkdir -p $(INSTALL_INCLUDE_PATH)
	sudo cp $(LIB_NAME) $(INSTALL_LIB_PATH)/
	sudo cp -r $(INCLUDE_DIR)/* $(INSTALL_INCLUDE_PATH)/
	@echo "Library installed to $(INSTALL_LIB_PATH)/$(LIB_NAME)"
	@echo "Headers installed to $(INSTALL_INCLUDE_PATH)/"

# 卸载
uninstall:
	sudo rm -f $(INSTALL_LIB_PATH)/$(LIB_NAME)
	sudo rm -rf $(INSTALL_INCLUDE_PATH)
	@echo "Library uninstalled"
