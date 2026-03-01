# Configuración de Compilador
CC       := gcc
CFLAGS   := -O3 -Wall -Wextra -D_GNU_SOURCE
LDFLAGS  := -static

# Directorios
SRC_DIR   := src
BUILD_DIR := build
BIN_NAME  := app

# Búsqueda recursiva de directorios para los headers (.h)
INCLUDE_DIRS  := $(shell find $(SRC_DIR) -type d)
INCLUDE_FLAGS := $(addprefix -I, $(INCLUDE_DIRS))

# Librerías (Orden crítico para enlace estático)
LIBS := -luring -luv -lssl -lcrypto -lz -lpthread -ldl

# Archivos de origen (.c)
SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean

all: $(BIN_NAME)

# Enlace final
$(BIN_NAME): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)
	@echo "--------------------------------------------"
	@echo "Build exitoso: $(BIN_NAME) (C Estático)"
	@strip $(BIN_NAME) # Reduce el tamaño eliminando símbolos de debug
	@ls -lh $(BIN_NAME)

# Compilación de objetos
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_NAME)