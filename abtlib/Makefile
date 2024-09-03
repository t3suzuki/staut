
N_CORE ?= 1
USE_PREEMPT ?= 1

FLAGS = -O3 -g -march=native
FLAGS += -fpic
FLAGS += -DN_CORE=$(N_CORE)
FLAGS += -DUSE_PREEMPT=$(USE_PREEMPT)

#ABT_PATH ?= /home/tomoya-s/mountpoint/tomoya-s/staut/argobots/install
#ABT_PATH ?= /home/tomoya-s/mountpoint/tomoya-s/staut/ppopp21-preemption-artifact/argobots/install
ABT_PATH ?= /home/tomoya-s/mountpoint/tomoya-s/staut/abtlib/ppopp21-preemption-artifact/argobots/install

all:
	g++ $(FLAGS) -c mylib.c  -fno-exceptions -I $(ABT_PATH)/include -fpermissive
	gcc $(FLAGS) -shared -o mylib.so mylib.o hook.c zpoline.c load_debug.c -I $(ABT_PATH)/include -L $(ABT_PATH)/lib -labt -lopcodes -lstdc++
	gcc $(FLAGS) -shared -o debug.so debug.c
