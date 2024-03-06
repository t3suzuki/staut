N_CORE ?= 1
ND ?= 2

ABT_PATH ?= ../ppopp21-preemption-artifact/argobots/install
MYLIB = ../mylib

FLAGS = -O3 -g -march=native
FLAGS += -fpic
FLAGS += -DN_CORE=$(N_CORE) -DND=$(ND)
FLAGS += -I$(MYLIB)
FLAGS += -I$(ABT_PATH)/include

all:
	g++ $(FLAGS) -c $(MYLIB)/nvme.cc  -fno-exceptions
	gcc $(FLAGS) nvme.o abt_backup.c -lstdc++ -o abt_backup
	gcc $(FLAGS) nvme.o abt_restore.c -lstdc++ -o abt_restore

run:
	rm -rf my.dat
	DRIVE_IDS=0000:0f:00.0_0000:0e:00.0 ./abt_backup
	DRIVE_IDS=0000:0f:00.0_0000:0e:00.0 ./abt_restore

backup:
	DRIVE_IDS=0000:0f:00.0_0000:0e:00.0 ./abt_backup

restore:
	DRIVE_IDS=0000:0f:00.0_0000:0e:00.0 ./abt_restore

clean:
	rm -f *~
	rm -f abt_backup abt_restore nvme.o
