AOUT ?= histx
TESTAOUT ?= histx-test
BUILD ?= ./build
SRC ?= ./src
SRCFILES := $(shell find $(SRC) -name "*.c" ! -path './src/test*')
TESTSRCFILES := $(shell find $(SRC) -name *.c ! -name "main.c")

OBJ := $(SRCFILES:%=$(BUILD)/%.o)
TESTOBJ := $(TESTSRCFILES:%=$(BUILD)/%.o)

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

$(BUILD)/$(TESTAOUT): $(TESTOBJ)
	$(CC) $(TESTOBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.c.o: %.c
	mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(BUILD)/$(TESTAOUT)

.PHONY: clean

all: $(BUILD)/$(AOUT) $(BUILD)/$(TESTAOUT)

clean:
	$(RM) -r $(BUILD)

-include $(DEP)

