#ifndef _DVB_SUB_H
#define _DVB_SUB_H
int dvbsub_init();
int dvbsub_stop();
int dvbsub_close();
#if HAVE_SPARK_HARDWARE || HAVE_DUCKBOX_HARDWARE
int dvbsub_start(int pid, bool _isEplayer = false);
#else
int dvbsub_start(int pid);
#endif
int dvbsub_pause();
int dvbsub_getpid();
void dvbsub_setpid(int pid);
#endif
