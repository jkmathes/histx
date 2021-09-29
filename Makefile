AOUT ?= histx
BUILD ?= ./build
SRC ?= ./src
SRCFILES := $(shell find $(SRC) -name *.c)
OBJ := $(SRCFILES:%=$(BUILD)/%.o)
DEP := $(OBJ:.o=.d)
LDFLAGS ?= -lsqlite3
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
	LDFLAGS += -lssl -lcrypto
endif

DIRS := $(shell find $(SRC) -type d)
FLAGS := $(addprefix -I,$(DIRS))
CFLAGS ?= $(FLAGS) -g -MMD -MP -std=c99

$(BUILD)/$(AOUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.c.o: %.c
	mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

all: $(BUILD)/$(AOUT)

clean:
	$(RM) -r $(BUILD)

-include $(DEP)

