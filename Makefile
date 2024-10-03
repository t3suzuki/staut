
nvmev:
	insmod ./nvmevirt/nvmev.ko memmap_start=7G memmap_size=256M cpus=6,7

rr:
	sudo ./fio/fio -direct=1 -rw=randread --bs=512 -size=128M -numjobs=1 -filename=/dev/nvme0n1 -name hoge -thread --group_reporting -iodepth 1 -ioengine libaio --time_based --timeout 5
