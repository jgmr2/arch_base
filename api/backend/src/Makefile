CXX      := g++

# 1. Ajuste de Includes: Quitamos jsoncpp (usaremos nlohmann-json que es header-only)
# Agregamos los flags de optimización para Beast.
CXXFLAGS := -std=c++17 -Wall -Wextra -O3 \
            -I/usr/local/include \
            -ffunction-sections -fdata-sections

# 2. LDFLAGS: Eliminamos Drogon, Trantor, c-ares, etc.
# Solo dejamos lo que Beast necesita para funcionar en Alpine.
# Nota: En Alpine, Boost suele requerir -lboost_system.
LDFLAGS  := -Wl,--gc-sections -s \
            -static-libstdc++ \
            -static-libgcc \
            -lboost_system \
            -lssl \
            -lcrypto \
            -lpthread \
            -ldl

SRC_DIR   := .
BUILD_DIR := ../build
OBJ_DIR   := $(BUILD_DIR)/obj

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TARGET := $(BUILD_DIR)/app

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "-------------------------------------------------------"
	@echo "✓ Binario NEXUS (Boost.Beast) optimizado en: $(TARGET)"
	@echo "✓ Protocolo: Unix Domain Sockets (/var/run/nexus/)"
	@echo "✓ Dependencias: Boost.Asio, OpenSSL, Pthread"
	@echo "-------------------------------------------------------"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean