#CC         = gcc
#AR         = ar
#CFLAGS    += -O3 -Wall -Wpointer-arith -g
#AR         = ar
#CFLAGS    += -O3 -Wall -Wpointer-arith -g
CC         = arm-none-eabi-gcc
AR         = arm-none-eabi-ar
CFLAGS     = -Og -mcpu=cortex-m0plus -mthumb -Wall -Wno-format -Wno-unused-function -Wno-maybe-uninitialized -ffunction-sections -fdata-sections
SRC_DIR    = source
BUILD_DIR  = build
OBJ_DIR    = $(BUILD_DIR)/obj
LDFLAGS   +=
SOURCES   := $(wildcard $(SRC_DIR)/*.c)
OBJECTS   := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET     = $(BUILD_DIR)/lib/libfatfs.a

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $?

$(OBJECTS): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

clean:
	rm -f $(OBJECTS) *~ $(TARGET)
