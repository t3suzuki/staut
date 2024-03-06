
N_CORE ?= 1
ND ?= 1
USE_PREEMPT ?= 1
USE_IO_URING ?= 0
WRITE_DEG ?= 8
READ_DEG ?= 256

FLAGS = -O3 -g -march=native
FLAGS += -fpic
FLAGS += -DN_CORE=$(N_CORE) -DND=$(ND)
FLAGS += -DUSE_PREEMPT=$(USE_PREEMPT)
FLAGS += -DUSE_IO_URING=$(USE_IO_URING)
#FLAGS += -DDEBUG=1
FLAGS += -DWRITE_DEG=$(WRITE_DEG)
FLAGS += -DREAD_DEG=$(READ_DEG)

#ABT_PATH ?= $(HOME)/argobots/install
ABT_PATH ?= /home/tomoya-s/work/github/ppopp21-preemption-artifact/argobots/install

all:
	g++ $(FLAGS) -c nvme.cc  -fno-exceptions -I $(ABT_PATH)/include
	g++ $(FLAGS) -c mylib.c  -fno-exceptions -I $(ABT_PATH)/include -fpermissive
	gcc $(FLAGS) -shared -o mylib.so mylib.o hook.c zpoline.c load_debug.c nvme.o myfs.c -I $(ABT_PATH)/include -L $(ABT_PATH)/lib -labt -lopcodes -lstdc++ -luring
	gcc $(FLAGS) -shared -o debug.so debug.c

pth:
	g++ $(FLAGS) -DUSE_PTHPTH=1 -c nvme.cc  -fno-exceptions
	gcc $(FLAGS) -DUSE_PTHPTH=1 -shared -o pthpth.so pthpth.c hook.c zpoline.c load_debug.c nvme.o myfs.c -I $(ABT_PATH)/include -lopcodes -lstdc++ 
	gcc $(FLAGS) -DUSE_PTHPTH=1 -shared -o debug.so debug.c

patch:
	./e9patch/e9compile.sh pref_yield.c

nvme: nvme.cc
	g++ -g nvme.cc -o nvme

clean:
	-rm -f *.o *.so

# perf record -e cpu/mem-loads,ldlat=400/P


SPDK_PATH=./spdk


LIBS = -L$(SPDK_PATH)/build/lib -Wl,--whole-archive -Wl,--no-as-needed -lspdk_bdev_malloc -lspdk_bdev_null -lspdk_bdev_nvme -lspdk_bdev_passthru -lspdk_bdev_lvol -lspdk_bdev_raid -lspdk_bdev_error -lspdk_bdev_gpt -lspdk_bdev_split -lspdk_bdev_delay -lspdk_bdev_zone_block -lspdk_blobfs_bdev -lspdk_blobfs -lspdk_blob_bdev -lspdk_lvol -lspdk_blob -lspdk_nvme -lspdk_bdev_aio -lspdk_bdev_ftl -lspdk_ftl -lspdk_bdev_virtio -lspdk_virtio -lspdk_vfio_user -lspdk_accel_ioat -lspdk_ioat -lspdk_scheduler_dynamic -lspdk_env_dpdk -lspdk_scheduler_dpdk_governor -lspdk_scheduler_gscheduler -lspdk_sock_posix -lspdk_event -lspdk_event_bdev -lspdk_bdev -lspdk_notify -lspdk_dma -lspdk_event_accel -lspdk_accel -lspdk_event_vmd -lspdk_vmd -lspdk_event_sock -lspdk_sock -lspdk_event_iobuf -lspdk_init -lspdk_thread -lspdk_trace -lspdk_rpc -lspdk_jsonrpc -lspdk_json -lspdk_util -lspdk_log -Wl,--no-whole-archive $(SPDK_PATH)/build/lib/libspdk_env_dpdk.a -Wl,--whole-archive $(SPDK_PATH)/dpdk/build/lib/librte_bus_pci.a $(SPDK_PATH)/dpdk/build/lib/librte_cryptodev.a $(SPDK_PATH)/dpdk/build/lib/librte_dmadev.a $(SPDK_PATH)/dpdk/build/lib/librte_eal.a $(SPDK_PATH)/dpdk/build/lib/librte_ethdev.a $(SPDK_PATH)/dpdk/build/lib/librte_hash.a $(SPDK_PATH)/dpdk/build/lib/librte_kvargs.a $(SPDK_PATH)/dpdk/build/lib/librte_mbuf.a $(SPDK_PATH)/dpdk/build/lib/librte_mempool.a $(SPDK_PATH)/dpdk/build/lib/librte_mempool_ring.a $(SPDK_PATH)/dpdk/build/lib/librte_net.a $(SPDK_PATH)/dpdk/build/lib/librte_pci.a $(SPDK_PATH)/dpdk/build/lib/librte_power.a $(SPDK_PATH)/dpdk/build/lib/librte_rcu.a $(SPDK_PATH)/dpdk/build/lib/librte_ring.a $(SPDK_PATH)/dpdk/build/lib/librte_telemetry.a $(SPDK_PATH)/dpdk/build/lib/librte_vhost.a -Wl,--no-whole-archive -lnuma -ldl -pthread -lrt -luuid -lssl -lcrypto -lm -laio

#LIBS = -L $(SPDK_PATH)/build/lib/ -lspdk_nvme -lspdk_event -lspdk_env_dpdk -lspdk_log -lspdk_thread -lspdk_init -lspdk_rpc -lspdk_json -lspdk_jsonrpc -lspdk_trace -lspdk_blob_bdev -lspdk_bdev -lspdk_dma -lspdk_notify -lspdk_blobfs  -lspdk_blob -lspdk_util -L $(SPDK_PATH)/dpdk/build/lib -lrte_mempool_ring -lrte_eal -lrte_pci -lrte_telemetry -lrte_bus_pci -lrte_kvargs -lrte_ring -lrte_mempool  -luuid

build:
	g++ spdk.c -I $(SPDK_PATH)/build/include/ -fpermissive $(LIBS)

set:
	echo 0 | sudo tee -a /proc/sys/vm/mmap_min_addr

tools:
	gcc -O3 readperf.c -o readperf
	gcc -O3 readetest.c -o readetest
