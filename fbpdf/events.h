#include <linux/input.h>
#include <sys/inotify.h>

#include <sys/poll.h>
#include <linux/input.h>

int open_input_devices();
int read_input_devices(struct input_event *ev,int timeout);
