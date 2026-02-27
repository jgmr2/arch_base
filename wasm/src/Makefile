# Nombre del compilador
CC = emcc

# Directorios
SRC_DIR = .
OUT_DIR = ../dist

# Configuración limpia (Sin libharu ni dependencias de PDF)
# EXPORTED_RUNTIME_METHODS: Ideal para pasar arrays de puntos (floats) a WASM.
EM_FLAGS = -O3 \
           --bind \
           -s WASM=1 \
           -s MODULARIZE=1 \
           -s EXPORT_ES6=1 \
           -s ALLOW_MEMORY_GROWTH=1 \
           -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','getValue','setValue','HEAPF32','HEAPU8']" \
           -s EXPORTED_FUNCTIONS="['_malloc', '_free']" \
           -s NO_EXIT_RUNTIME=1 \
           -s ENVIRONMENT='web'

# Buscar todos los archivos .cpp en el directorio actual
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
# Generar nombres de salida .js en el directorio dist
TARGETS = $(patsubst $(SRC_DIR)/%.cpp, $(OUT_DIR)/%.js, $(SOURCES))

.PHONY: all clean

all: $(TARGETS)

# Regla de compilación simplificada
$(OUT_DIR)/%.js: $(SRC_DIR)/%.cpp
	@mkdir -p $(OUT_DIR)
	$(CC) $< $(EM_FLAGS) -o $@

clean:
	rm -rf $(OUT_DIR)