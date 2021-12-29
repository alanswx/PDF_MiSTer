
#include <linux/input.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/inotify.h>

#include <sys/poll.h>
#include <linux/input.h>
#include <stdlib.h>
#define NUMDEV 30
#define NUMBUTTONS         32
#define UINPUT_NAME "MiSTer virtual input"
int mfd = -1;


static struct pollfd pool[NUMDEV + 3];

typedef struct
{
	uint16_t vid, pid;
	char     idstr[256];
	char     mod;

	uint8_t  led;
	uint8_t  mouse;
	uint8_t  axis_edge[256];
	int8_t   axis_pos[256];

	uint8_t  num;
	uint8_t  has_map;
	uint32_t map[NUMBUTTONS];
	int      map_shown;

	uint8_t  osd_combo;

	uint8_t  has_mmap;
	uint32_t mmap[NUMBUTTONS];
	uint16_t jkmap[1024];
	int      stick_l[2];
	int      stick_r[2];

	uint8_t  has_kbdmap;
	uint8_t  kbdmap[256];

	uint16_t guncal[4];

	int      accx, accy;
	int      startx, starty;
	int      lastx, lasty;
	int      quirk;

	int      misc_flags;
	int      paddle_val;
	int      spinner_prev;
	int      spinner_acc;
	int      spinner_prediv;
	int      spinner_dir;
	int      spinner_accept;
	int      old_btn;
	int      ds_mouse_emu;

	int      lightgun_req;
	int      lightgun;

	int      timeout;
	char     mac[64];

	int      bind;
	char     devname[32];
	char     id[80];
	char     name[128];
	char     sysfs[512];
} devInput;

static devInput input[NUMDEV] = {};

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

static int check_devs()
{
	int result = 0;
	int length, i = 0;
	char buffer[BUF_LEN];
	length = read(mfd, buffer, BUF_LEN);

	if (length < 0)
	{
		printf("ERR: read\n");
		return 0;
	}

	while (i<length)
	{
		struct inotify_event *event = (struct inotify_event *) &buffer[i];
		if (event->len)
		{
			if (event->mask & IN_CREATE)
			{
				result = 1;
				if (event->mask & IN_ISDIR)
				{
					printf("The directory %s was created.\n", event->name);
				}
				else
				{
					printf("The file %s was created.\n", event->name);
				}
			}
			else if (event->mask & IN_DELETE)
			{
				result = 1;
				if (event->mask & IN_ISDIR)
				{
					printf("The directory %s was deleted.\n", event->name);
				}
				else
				{
					printf("The file %s was deleted.\n", event->name);
				}
			}
			/*
			else if ( event->mask & IN_MODIFY )
			{
				result = 1;
				if ( event->mask & IN_ISDIR )
				{
					printf( "The directory %s was modified.\n", event->name );
				}
				else
				{
					printf( "The file %s was modified.\n", event->name );
				}
			}
			*/
		}
		i += EVENT_SIZE + event->len;
	}

	return result;
}

#define test_bit(bit, array)  (array [bit / 8] & (1 << (bit % 8)))
int open_input_devices()
{
	int yalv;
	uint8_t key_b[KEY_MAX/8 + 1];

	int timeout = 1000;
	//printf("Open up to %d input devices.\n", NUMDEV);
	for (int i = 0; i < NUMDEV; i++)
	{
		pool[i].fd = -1;
		pool[i].events = 0;
	}

	memset(input, 0, sizeof(input));

	int n = 0;
	DIR *d = opendir("/dev/input");
	if (d)
	{
		struct dirent *de;
		while ((de = readdir(d)))
		{
			if (!strncmp(de->d_name, "event", 5) || !strncmp(de->d_name, "mouse", 5))
			{
				memset(&input[n], 0, sizeof(input[n]));
				sprintf(input[n].devname, "/dev/input/%s", de->d_name);
				int fd = open(input[n].devname, O_RDWR | O_CLOEXEC);
				//printf("open(%s): %d\n", input[n].devname, fd);
				//printf("open(%s): %d\n", input[n].name, fd);

				if (fd > 0)
				{
					pool[n].fd = fd;
					pool[n].events = POLLIN;
					input[n].mouse = !strncmp(de->d_name, "mouse", 5);

					char uniq[32] = {};
					if (!input[n].mouse)
					{
						struct input_id id;
						memset(&id, 0, sizeof(id));
						ioctl(pool[n].fd, EVIOCGID, &id);
						input[n].vid = id.vendor;
						input[n].pid = id.product;

						ioctl(pool[n].fd, EVIOCGUNIQ(sizeof(uniq)), uniq);
						ioctl(pool[n].fd, EVIOCGNAME(sizeof(input[n].name)), input[n].name);
						//input[n].led = has_led(pool[n].fd);
					}

					//skip our virtual device
#if 0
					if (!strcmp(input[n].name, UINPUT_NAME))
					{
						close(pool[n].fd);
						pool[n].fd = -1;
						//fprintf(stderr,"skipping MiSTer input\n");
						continue;
					}
#endif

					n++;
					if (n >= NUMDEV) break;
				}
			}
			
		}
		closedir(d);



		for (int i = 0; i < n; i++)
		{
			//printf("opened %d(%2d): %s (%04x:%04x) %d \"%s\" \"%s\"\n", i, input[i].bind, input[i].devname, input[i].vid, input[i].pid, input[i].quirk, input[i].id, input[i].name);
		}

	}
}

int read_input_devices(struct input_event *ev,int timeout)
{
	int return_value = poll(pool, NUMDEV + 3, timeout);
	//fprintf(stderr,"return_value: %d\n",return_value);
	if (!return_value) return -1;

	if (return_value < 0)
	{
		//printf("ERR: poll\n");
		return -1;
	}

	if ((pool[NUMDEV].revents & POLLIN) && check_devs())
	{
		//printf("Close all devices.\n");
		for (int i = 0; i < NUMDEV; i++) if (pool[i].fd >= 0)
		{
			ioctl(pool[i].fd, EVIOCGRAB, 0);
			close(pool[i].fd);
		}
		return 0;
	}

	for (int pos = 0; pos < NUMDEV; pos++)
	{
		int i = pos;
		if ((pool[i].fd >= 0) && (pool[i].revents & POLLIN))
		{
			if (!input[i].mouse)
			{
				memset(ev, 0, sizeof(struct input_event));
				if (read(pool[i].fd, ev, sizeof(struct input_event)) == sizeof(struct input_event))
				{
					/*
					fprintf(stderr,"ev->code: %d ev->value %d ev->type %d\n",ev->code,ev->value,ev->type);

					switch(ev->type)
					{

						case EV_SYN:
							fprintf(stderr,"SYN: ev->code: %d\n",ev->code);
  //- Used as markers to separate events. Events may be separated in time or in
  //  space, such as with the multitouch protocol.
						break;
						case EV_MSC:
							fprintf(stderr,"MSC: ev->code: %d\n",ev->code);
  //- Used to describe miscellaneous input data that do not fit into other types.
						break;
						case EV_SW:
							fprintf(stderr,"SW: ev->code: %d\n",ev->code);
  //- Used to describe binary state input switches.
						break;
						case EV_LED:
							fprintf(stderr,"LED: ev->code: %d\n",ev->code);
  //- Used to turn LEDs on devices on and off.
						break;
						case EV_SND:
							fprintf(stderr,"SND: ev->code: %d\n",ev->code);
  //- Used to output sound to devices.
						break;

						case EV_REP:
  //- Used for autorepeating devices.
							fprintf(stderr,"REP: ev->code: %d\n",ev->code);
						break;

						case EV_FF:
							fprintf(stderr,"FF: ev->code: %d\n",ev->code);
  //- Used to send force feedback commands to an input device.
						break;

						case EV_PWR:
							fprintf(stderr,"PWR: ev->code: %d\n",ev->code);
  //- A special type for power button and switch input.
						break;

						case EV_FF_STATUS:
						//   - Used to receive force feedback device status.
							fprintf(stderr,"FF_STATUS: ev->code: %d\n",ev->code);
						break;

						case EV_KEY:
  //- Used to describe state changes of keyboards, buttons, or other key-like
   // devices.
							fprintf(stderr,"KEY: ev->code: %d\n",ev->code);
						break;
						case EV_ABS:
							fprintf(stderr,"ABS: ev->code: %d\n",ev->code);
  //- Used to describe absolute axis value changes, e.g. describing the
   // coordinates of a touch on a touchscreen.
						break;
						case EV_REL:
							fprintf(stderr,"REL: ev->code: %d\n",ev->code);
  //- Used to describe relative axis value changes, e.g. moving the mouse 5 units
  //  to the left.
						break;
					}
					*/
					int dev = i;
					//fprintf(stderr,"got: %d\n",ev->type);
					return 1;
				}
			}
		}

	}
}

#if 0
int main(int argc, char *argv[])
{
int err = open_input_devices();
while (1) {
 struct input_event ev;
 err = read_input_devices(&ev,1000);
 if (err==1) {
     fprintf(stderr,"ev.code: %d ev.value %d ev.type %d\n",ev.code,ev.value,ev.type);
     if (ev.type==EV_KEY && ev.code==1) exit(0);

 }
}
}
#endif
