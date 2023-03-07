.SUFFIXES:
MAKEFLAGS += --no-builtin-rules
AOUT ?= histx
TESTAOUT ?= histx-test
BUILD ?= ./build
SRC ?= ./src
SRCFILES := $(shell find $(SRC) -name "*.c" ! -path './src/test*')
SRCFILES += $(shell find $(SRC) -name "*.y" -o -name "*.l")
TESTSRCFILES := $(shell find $(SRC) -name "*.c" ! -name "main.c")
TESTSRCFILES += $(shell find $(SRC) -name "*.y" -o -name "*.l")

OBJ := $(SRCFILES:%=$(BUILD)/%.o)
TESTOBJ := $(TESTSRCFILES:%=$(BUILD)/%.o)

DEP := $(OBJ:.o=.d)
LDFLAGS ?= -lsqlite3
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
	LDFLAGS += -lssl -lcrypto -lbsd
endif

DIRS := $(shell find $(SRC) -type d)
FLAGS := $(addprefix -I,$(DIRS)) -I$(BUILD)/$(SRC)/config
CFLAGS ?= $(FLAGS) -g -MMD -MP -std=c99

$(BUILD)/$(AOUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD)/$(TESTAOUT): $(TESTOBJ)
	$(CC) $(TESTOBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.l.o: %.l $(BUILD)/$(SRC)/config/config.y.o
	mkdir -pv $(dir $@)
	flex -o $(BUILD)/$<.c $<
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE -c $(BUILD)/$<.c -o $@

$(BUILD)/%.y.o: %.y
	mkdir -pv $(dir $@)
	bison -d $< -o $(BUILD)/$<.c
	$(CC) $(CFLAGS) -c $(BUILD)/$<.c -o $@

$(BUILD)/%.c.o: %.c
	mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(BUILD)/$(TESTAOUT)

.PHONY: clean

all: $(BUILD)/$(AOUT) $(BUILD)/$(TESTAOUT)

clean:
	$(RM) -r $(BUILD)

-include $(DEP)

