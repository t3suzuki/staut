#ifndef __MYFIFO_HPP__
#define __MYFIFO_HPP__

template<int QD, class T>
class MyFIFO {
private:
  volatile int wp;
  volatile int rp;
  T *que[QD];
public:
  MyFIFO() {
    wp = 0;
    rp = 0;
  };
  bool push(T *entry) {
    while (1) {
      volatile int old_wp = wp;
      int new_wp = (old_wp + 1) % QD;
      if (new_wp == rp) {
	return false; // full
      }
      bool ret = __sync_bool_compare_and_swap(&wp, old_wp, new_wp);
      if (ret) {
	que[old_wp] = entry;
	return true;
      }
    }
  }
  T *pop() {
    while (1) {
      volatile int old_rp = rp;
      int new_rp = (old_rp + 1) % QD;
      if (old_rp == wp) {
	return nullptr; // empty
      }
      bool ret = __sync_bool_compare_and_swap(&rp, old_rp, new_rp);
      if (ret) {
	return que[old_rp];
      }
    }
  }
};

#endif 
