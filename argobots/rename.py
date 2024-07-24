import os, re

dir_path = "."

pth = [
    "sched_yield",
    "pthread_create",
    "pthread_join",
    "pthread_mutex_init",
    "pthread_mutex_destroy",
    "pthread_mutex_lock",
    "pthread_mutex_unlock",
    "pthread_cond_init",
    "pthread_cond_destroy",
    "pthread_cond_signal",
    "pthread_cond_broadcast",
    "pthread_cond_wait",
    "pthread_cond_timedwait",
    "pthread_barrier_init",
    "pthread_barrier_destroy",
    "pthread_barrier_wait",
    "pthread_self",
    "pthread_setname_np",
    "pthread_setaffinity_np",
    "pthread_getaffinity_np",
    "pthread_exit",
    "pthread_kill",
    "pthread_sigmask",
    ]

files = os.listdir(dir_path)
for f in files:
    pattern = r'\.c$'
    res = re.search(pattern, f)
    if res:
        for func in pth:
            os.system("sed -i s/{}/real_{}/ {}".format(func, func, f))
