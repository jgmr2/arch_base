# Configuración de Compilador
CC       := gcc
CFLAGS   := -O3 -Wall -Wextra -D_GNU_SOURCE
LDFLAGS  := -static -L/usr/lib 

# Directorios
SRC_DIR   := src
BUILD_DIR := build
BIN_NAME  := app

# Búsqueda de headers
INCLUDE_DIRS  := $(shell find $(SRC_DIR) -type d)
# Agregamos /usr/include por si los headers de pgcommon/port están ahí
INCLUDE_FLAGS := $(addprefix -I, $(INCLUDE_DIRS)) -I/usr/include/postgresql -I/usr/include

# --- CORRECCIÓN LIBRERÍAS ---
# Usamos --start-group para que el linker busque repetidamente entre ellas 
# hasta resolver pg_md5_encrypt y demás funciones compartidas.
PG_LIBS := -Wl,--start-group -lpq -lpgcommon -lpgport -Wl,--end-group

# El orden ahora es: Postgres -> IO/Async -> SSL -> Sistema
LIBS := $(PG_LIBS) -luring -luv -lssl -lcrypto -lz -lpthread -ldl -lm

# Archivos de origen
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
	@strip $(BIN_NAME) 
	@ls -lh $(BIN_NAME)

# Compilación de objetos
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_NAME)