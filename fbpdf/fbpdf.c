/*
 * FBPDF LINUX FRAMEBUFFER PDF VIEWER
 *
 * Copyright (C) 2009-2016 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include "draw.h"
#include "doc.h"
#include "joystick.h"
#include "events.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

#define PAGESTEPS	8
#define MAXZOOM		1000
#define MARGIN		1
#define CTRLKEY(x)	((x) - 96)
#define ISMARK(x)	(isalpha(x) || (x) == '\'' || (x) == '`')

static struct doc *doc;
static fbval_t *pbuf;		/* current page */
static int srows, scols;	/* screen dimentions */
static int prows, pcols;	/* current page dimensions */
static int prow, pcol;		/* page position */
static int srow, scol;		/* screen position */

static struct termios termios;
static char filename[256];
static int mark[128];		/* mark page number */
static int mark_row[128];	/* mark head position */
static int num = 1;		/* page number */
static int numdiff;		/* G command page number difference */
static int zoom = 150;
static int zoom_def = 150;	/* default zoom */
static int rotate;
static int count;
static int invert;		/* invert colors? */

static void printloading()
{
	printf("\x1b[H");
	printf("LOADING:     file:%s  page:%d(%d)  zoom:%d%% \x1b[K\r",
		filename, num, doc_pages(doc), zoom);
	fflush(stdout);
}
static void draw(void)
{
	int bpp = FBM_BPP(fb_mode());
	int i;
	fbval_t *rbuf = malloc(scols * sizeof(rbuf[0]));
	for (i = srow; i < srow + srows; i++) {
		int cbeg = MAX(scol, pcol);
		int cend = MIN(scol + scols, pcol + pcols);
		memset(rbuf, 0, scols * sizeof(rbuf[0]));
		if (i >= prow && i < prow + prows && cbeg < cend) {
			memcpy(rbuf + cbeg - scol,
				pbuf + (i - prow) * pcols + cbeg - pcol,
				(cend - cbeg) * sizeof(rbuf[0]));
		}
		memcpy(fb_mem(i - srow), rbuf, scols * bpp);
	}
	free(rbuf);
}

static int loadpage(int p)
{
	int i;
	if (p < 1 || p > doc_pages(doc))
		return 1;
	prows = 0;
	free(pbuf);
	num = p;
	printloading();
	pbuf = doc_draw(doc, p, zoom, rotate, &prows, &pcols);
	if (invert) {
		for (i = 0; i < prows * pcols; i++)
			pbuf[i] = pbuf[i] ^ 0xffffffff;
	}
	prow = -prows / 2;
	pcol = -pcols / 2;
	return 0;
}

static void zoom_page(int z)
{
	int _zoom = zoom;
	zoom = MIN(MAXZOOM, MAX(1, z));
	if (!loadpage(num))
		srow = srow * zoom / _zoom;
}

static void setmark(int c)
{
	if (ISMARK(c)) {
		mark[c] = num;
		mark_row[c] = srow * 100 / zoom;
	}
}

static void jmpmark(int c, int offset)
{
	if (c == '`')
		c = '\'';
	if (ISMARK(c) && mark[c]) {
		int dst = mark[c];
		int dst_row = offset ? mark_row[c] * zoom / 100 : 0;
		setmark('\'');
		if (!loadpage(dst))
			srow = offset ? dst_row : prow;
	}
}

static int readkey(void)
{
	unsigned char b;
	if (read(0, &b, 1) <= 0)
		return -1;
	return b;
}

static int getcount(int def)
{
	int result = count ? count : def;
	count = 0;
	return result;
}

static void printinfo(void)
{
	printf("\x1b[H");
	printf("FBPDF:     file:%s  page:%d(%d)  zoom:%d%% \x1b[K\r",
		filename, num, doc_pages(doc), zoom);
	fflush(stdout);
}


static void term_setup(void)
{
	struct termios newtermios;
	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~ICANON;
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &newtermios);
	printf("\x1b[?25l");		/* hide the cursor */
	printf("\x1b[2J");		/* clear the screen */
	fflush(stdout);
}

static void term_cleanup(void)
{
	tcsetattr(0, 0, &termios);
	printf("\x1b[?25h\n");		/* show the cursor */
}

static void sigcont(int sig)
{
	term_setup();
}

static int reload(void)
{
	doc_close(doc);
	doc = doc_open(filename);
	if (!doc || !doc_pages(doc)) {
		fprintf(stderr, "\nfbpdf: cannot open <%s>\n", filename);
		return 1;
	}
	if (!loadpage(num))
		draw();
	return 0;
}

static int rmargin(void)
{
	int ret = 0;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = pcols - 1;
		while (j > ret && pbuf[i * pcols + j] == FB_VAL(255, 255, 255))
			j--;
		if (ret < j)
			ret = j;
	}
	return ret;
}

static int lmargin(void)
{
	int ret = pcols;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = 0;
		while (j < ret && pbuf[i * pcols + j] == FB_VAL(255, 255, 255))
			j++;
		if (ret > j)
			ret = j;
	}
	return ret;
}

static void mainloop_new(void)
{
    const char *device;
    int js;
    struct js_event event;
    struct axis_state axes[3] = {0};
    size_t axis;

    int step = srows / PAGESTEPS;
    int hstep = scols / PAGESTEPS;
    int c;
    int done=0;
    struct timeval tv;
    fd_set fds;

    term_setup();
    signal(SIGCONT, sigcont);
    loadpage(num);
    srow = prow;
    scol = -scols / 2;

    int err = open_input_devices();

    // default to width
    zoom_page(pcols ? zoom * scols / pcols : zoom);
    draw();

    while (!done) {
      struct input_event ev;
      err = read_input_devices(&ev,1000);
      if (err==1) {
         //fprintf(stderr,"ev.code: %d ev.value %d ev.type %d\n",ev.code,ev.value,ev.type);
     	 if (ev.type==EV_ABS) {
		 // code is the axis
		 // value is the direction
                if (ev.code==1) {
                    if (ev.value>200 || ev.value<-200) {
                         if (ev.value>200)
                            srow += step * getcount(1);
                         if (ev.value<-200){
                            srow -= step * getcount(1);
			   }

			 // try to make us lock to the viewport
			 if (srow < prow) srow=prow;
			 if (prow+srow>-srows) srow = prows - srows+ prow;
		    }
               } else if (ev.code==0) {
		       if (ev.value>200 || ev.value<-200) {

                        if (ev.value>200)
                            scol += hstep * getcount(1);
                        if (ev.value<-200)
                            scol -= hstep * getcount(1);

			 if (scol < pcol) scol=pcol;
			 if (pcol+scol>-scols) scol = pcols - scols+ pcol;
                   }
               }
	 } else if (ev.type==EV_KEY) {
		 // ev.code >= 256 are joystick buttons
		 switch (ev.code) {
			 case KEY_ESC:  // ESC
				 done=1;
			 break;
			case KEY_HOME:
				srow = prow;
			break;
			case KEY_END:
			srow = prow + prows - srows;
			break;
                         case BTN_A:
                             if (!loadpage(num + getcount(1)))
                                 srow = prow;
                         break;
                         case BTN_B:
                             if (!loadpage(num - getcount(1)))
                                 srow = prow;
                         break;
                         case BTN_X:
                             if (!loadpage(num + getcount(10)))
                                 srow = prow;
                         break;
                         case BTN_Y:
                             if (!loadpage(num - getcount(10)))
                                 srow = prow;
                         break;
                         case BTN_TL:
                             zoom_page(prows ? zoom * srows / prows : zoom);
                         break;
                         case BTN_TR:
                             zoom_page(pcols ? zoom * scols / pcols : zoom);
                         break;
		 }
	 }
	srow = MAX(prow - srows + MARGIN, MIN(prow + prows - MARGIN, srow));
	scol = MAX(pcol - scols + MARGIN, MIN(pcol + pcols - MARGIN, scol));
	draw();
     }
   }

   term_cleanup();

}
static void mainloop(void)
{
    const char *device;
    int js;
    struct js_event event;
    struct axis_state axes[3] = {0};
    size_t axis;

    int step = srows / PAGESTEPS;
    int hstep = scols / PAGESTEPS;
    int c;
    int done=0;
    struct timeval tv;
    fd_set fds;

    // set timeout to zero for select
    tv.tv_sec=0;
    tv.tv_usec=0;

    term_setup();
    signal(SIGCONT, sigcont);
    loadpage(num);
    srow = prow;
    scol = -scols / 2;

    // open joystick 0 
    device = "/dev/input/js0";
    js = open(device, O_RDONLY);
    if (js == -1)
        perror("Could not open joystick");

    // default to width
    zoom_page(pcols ? zoom * scols / pcols : zoom);
    draw();

    while (!done) {
        // setup the descriptors for select, checking on data read available for 
	// STDIN and joystick (js)
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO,&fds);
        FD_SET(js,&fds);
        //select(js+1,&fds,NULL,NULL,&tv);
        select(js+1,&fds,NULL,NULL,NULL); // block forever

        // check to see if the joystick fd has data
        if (FD_ISSET(js,&fds) && read_event(js, &event) == 0) {
          switch (event.type)
          {
            case JS_EVENT_BUTTON:
                if (event.value) {
                    switch(event.number) {
                         case 0:
                             if (!loadpage(num + getcount(1)))
                                 srow = prow;
                         break;
                         case 1:
                             if (!loadpage(num - getcount(1)))
                                 srow = prow;
                         break;
                         case 2:
                             if (!loadpage(num + getcount(10)))
                                 srow = prow;
                         break;
                         case 3:
                             if (!loadpage(num - getcount(10)))
                                 srow = prow;
                         break;
                         case 4:
                             zoom_page(prows ? zoom * srows / prows : zoom);
                         break;
                         case 5:
                             zoom_page(pcols ? zoom * scols / pcols : zoom);
                         break;
                    }
               }
		//printf("\x1b[H");
                //printf("Button %u %s\n", event.number, event.value ? "pressed" : "released");
                break;
            case JS_EVENT_AXIS:
                axis = get_axis_state(&event, axes);
                if (axis==0) {
                    if (axes[0].y>4 || axes[0].y<-4) {
                         if (axes[0].y>4)
                            srow += step * getcount(1);
                         if (axes[0].y<-4){
                            srow -= step * getcount(1);
			   }

			 // try to make us lock to the viewport
			 if (srow < prow) srow=prow;
			 if (prow+srow>-srows) srow = prows - srows+ prow;
                    } else if (axes[0].x>4 || axes[0].x<-4) {

                        if (axes[0].x>4)
                            scol += hstep * getcount(1);
                        if (axes[0].x<-4)
                            scol -= hstep * getcount(1);

			 if (scol < pcol) scol=pcol;
			 if (pcol+scol>-scols) scol = pcols - scols+ pcol;
                   }
               }
	
                //if (axis < 3)
		//{printf("\x1b[H");
                //    printf("Axis %zu at (%6d, %6d)\n", axis, axes[axis].x, axes[axis].y);
		//}
                break;
            default:
                /* Ignore init events. */
                break;

        }
	srow = MAX(prow - srows + MARGIN, MIN(prow + prows - MARGIN, srow));
	scol = MAX(pcol - scols + MARGIN, MIN(pcol + pcols - MARGIN, scol));
	draw();
#if 0
	printf("\x1b[H");
	printf("INFO:  step %d hstep %d scol %d  pcol %d  pcols %d srow %d prow %d prows %d srows %d \x1b[K\r",
			step,hstep,scol,pcol,pcols,srow,prow,prows,srows);
	fflush(stdout);
#endif    

	}
	// keyboard handling from STDIN
	if (FD_ISSET(0,&fds)) {
                //fprintf(stderr,"inside keyboard\n");
	        c = readkey();
		/*
		if (c==0x1b) {
			// if we get 1b without a second key, it is escape
			// we need to check for blocking
	        	c = readkey();
			// next key should be the special key
			printf("got 1b\n");
			printf("second key: %x\n",c);
			exit(0);
		}
		*/
		if (c == 'q') {
			done=1;
			break;
		}
		if (c == 'e' && reload())
			break;
		switch (c) {	/* commands that do not require redrawing */
		case 'o':
			numdiff = num - getcount(num);
			break;
		case 'Z':
			count *= 10;
			zoom_def = getcount(zoom);
			break;
		case 'i':
			printinfo();
			break;
		case 27:
			count = 0;
			break;
		case 'm':
			setmark(readkey());
			break;
		case 'd':
			sleep(getcount(1));
			break;
		default:
			if (isdigit(c))
				count = count * 10 + c - '0';
		}
		switch (c) {	/* commands that require redrawing */
		case CTRLKEY('f'):
		case 'J':
			if (!loadpage(num + getcount(1)))
				srow = prow;
			break;
		case CTRLKEY('b'):
		case 'K':
			if (!loadpage(num - getcount(1)))
				srow = prow;
			break;
		case 'G':
			setmark('\'');
			if (!loadpage(getcount(doc_pages(doc) - numdiff) + numdiff))
				srow = prow;
			break;
		case 'O':
			numdiff = num - getcount(num);
			setmark('\'');
			if (!loadpage(num + numdiff))
				srow = prow;
			break;
		case 'z':
			count *= 10;
			zoom_page(getcount(zoom_def));
			break;
		case 'w':
			zoom_page(pcols ? zoom * scols / pcols : zoom);
			break;
		case 'W':
			if (lmargin() < rmargin())
				zoom_page(zoom * (scols - hstep) /
					(rmargin() - lmargin()));
			break;
		case 'f':
			zoom_page(prows ? zoom * srows / prows : zoom);
			break;
		case 'r':
			rotate = getcount(0);
			if (!loadpage(num))
				srow = prow;
			break;
		case '`':
		case '\'':
			jmpmark(readkey(), c == '`');
			break;
		case 'j':
			srow += step * getcount(1);
			break;
		case 'k':
			srow -= step * getcount(1);
			break;
		case 'l':
			scol += hstep * getcount(1);
			break;
		case 'h':
			scol -= hstep * getcount(1);
			break;
		case 'H':
			srow = prow;
			break;
		case 'L':
			srow = prow + prows - srows;
			break;
		case 'M':
			srow = prow + prows / 2 - srows / 2;
			break;
		case 'C':
			scol = -scols / 2;
			break;
		case ' ':
		case CTRLKEY('d'):
			srow += srows * getcount(1) - step;
			break;
		case 127:
		case CTRLKEY('u'):
			srow -= srows * getcount(1) - step;
			break;
		case '[':
			scol = pcol;
			break;
		case ']':
			scol = pcol + pcols - scols;
			break;
		case '{':
			scol = pcol + lmargin() - hstep / 2;
			break;
		case '}':
			scol = pcol + rmargin() + hstep / 2 - scols;
			break;
		case CTRLKEY('l'):
			break;
		case 'I':
			invert = !invert;
			loadpage(num);
			break;
		default:	/* no need to redraw */
			continue;
		}
		srow = MAX(prow - srows + MARGIN, MIN(prow + prows - MARGIN, srow));
		scol = MAX(pcol - scols + MARGIN, MIN(pcol + pcols - MARGIN, scol));
		draw();
#if 0
	printf("\x1b[H");
	printf("KEYS:  key %c %x \x1b[K\r",
			c,c);
	fflush(stdout);
#endif
	}
    }
    term_cleanup();
    close(js);
}

static char *usage =
	"usage: fbpdf [-r rotation] [-z zoom x10] [-p page] filename";

int main(int argc, char *argv[])
{
	int i = 1;
	if (argc < 2) {
		puts(usage);
		return 1;
	}
	strcpy(filename, argv[argc - 1]);
	doc = doc_open(filename);
	if (!doc || !doc_pages(doc)) {
		fprintf(stderr, "fbpdf: cannot open <%s>\n", filename);
		return 1;
	}
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch (argv[i][1]) {
		case 'r':
			rotate = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			break;
		case 'z':
			zoom = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]) * 10;
			break;
		case 'p':
			num = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			break;
		}
	}
	printinfo();
	if (fb_init(getenv("FBDEV")))
		return 1;
	srows = fb_rows();
	scols = fb_cols();
	if (FBM_BPP(fb_mode()) != sizeof(fbval_t))
		fprintf(stderr, "fbpdf: fbval_t doesn't match fb depth\n");
	else{
		mainloop_new();
	}
	fb_free();
	free(pbuf);
	if (doc)
		doc_close(doc);
	return 0;
}
