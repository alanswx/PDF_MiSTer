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
	zoom = MIN(MAXZOOM, MAX(50, z));
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

#define NONE         0xFF
#define LCTRL        0x000100
#define LSHIFT       0x000200
#define LALT         0x000400
#define LGUI         0x000800
#define RCTRL        0x001000
#define RSHIFT       0x002000
#define RALT         0x004000
#define RGUI         0x008000
#define MODMASK      0x00FF00


#define OSD          0x010000  // to be used by OSD, not the core itself
#define OSD_OPEN     0x020000  // OSD key not forwarded to core, but queued in arm controller
#define CAPS_TOGGLE  0x040000  // caps lock toggle behaviour
#define EXT          0x080000
#define EMU_SWITCH_1 0x100000
#define EMU_SWITCH_2 0x200000

#define UPSTROKE     0x400000


static uint32_t modifier = 0;
uint32_t get_key_mod()
{
	return modifier & MODMASK;
}
static const int ev2ps2[] =
{
	NONE, //0   KEY_RESERVED
	0x76, //1   KEY_ESC
	0x16, //2   KEY_1
	0x1e, //3   KEY_2
	0x26, //4   KEY_3
	0x25, //5   KEY_4
	0x2e, //6   KEY_5
	0x36, //7   KEY_6
	0x3d, //8   KEY_7
	0x3e, //9   KEY_8
	0x46, //10  KEY_9
	0x45, //11  KEY_0
	0x4e, //12  KEY_MINUS
	0x55, //13  KEY_EQUAL
	0x66, //14  KEY_BACKSPACE
	0x0d, //15  KEY_TAB
	0x15, //16  KEY_Q
	0x1d, //17  KEY_W
	0x24, //18  KEY_E
	0x2d, //19  KEY_R
	0x2c, //20  KEY_T
	0x35, //21  KEY_Y
	0x3c, //22  KEY_U
	0x43, //23  KEY_I
	0x44, //24  KEY_O
	0x4d, //25  KEY_P
	0x54, //26  KEY_LEFTBRACE
	0x5b, //27  KEY_RIGHTBRACE
	0x5a, //28  KEY_ENTER
	LCTRL | 0x14, //29  KEY_LEFTCTRL
	0x1c, //30  KEY_A
	0x1b, //31  KEY_S
	0x23, //32  KEY_D
	0x2b, //33  KEY_F
	0x34, //34  KEY_G
	0x33, //35  KEY_H
	0x3b, //36  KEY_J
	0x42, //37  KEY_K
	0x4b, //38  KEY_L
	0x4c, //39  KEY_SEMICOLON
	0x52, //40  KEY_APOSTROPHE
	0x0e, //41  KEY_GRAVE
	LSHIFT | 0x12, //42  KEY_LEFTSHIFT
	0x5d, //43  KEY_BACKSLASH
	0x1a, //44  KEY_Z
	0x22, //45  KEY_X
	0x21, //46  KEY_C
	0x2a, //47  KEY_V
	0x32, //48  KEY_B
	0x31, //49  KEY_N
	0x3a, //50  KEY_M
	0x41, //51  KEY_COMMA
	0x49, //52  KEY_DOT
	0x4a, //53  KEY_SLASH
	RSHIFT | 0x59, //54  KEY_RIGHTSHIFT
	0x7c, //55  KEY_KPASTERISK
	LALT | 0x11, //56  KEY_LEFTALT
	0x29, //57  KEY_SPACE
	0x58, //58  KEY_CAPSLOCK
	0x05, //59  KEY_F1
	0x06, //60  KEY_F2
	0x04, //61  KEY_F3
	0x0c, //62  KEY_F4
	0x03, //63  KEY_F5
	0x0b, //64  KEY_F6
	0x83, //65  KEY_F7
	0x0a, //66  KEY_F8
	0x01, //67  KEY_F9
	0x09, //68  KEY_F10
	EMU_SWITCH_2 | 0x77, //69  KEY_NUMLOCK
	EMU_SWITCH_1 | 0x7E, //70  KEY_SCROLLLOCK
	0x6c, //71  KEY_KP7
	0x75, //72  KEY_KP8
	0x7d, //73  KEY_KP9
	0x7b, //74  KEY_KPMINUS
	0x6b, //75  KEY_KP4
	0x73, //76  KEY_KP5
	0x74, //77  KEY_KP6
	0x79, //78  KEY_KPPLUS
	0x69, //79  KEY_KP1
	0x72, //80  KEY_KP2
	0x7a, //81  KEY_KP3
	0x70, //82  KEY_KP0
	0x71, //83  KEY_KPDOT
	NONE, //84  ???
	NONE, //85  KEY_ZENKAKU
	0x61, //86  KEY_102ND
	0x78, //87  KEY_F11
	0x07, //88  KEY_F12
	NONE, //89  KEY_RO
	NONE, //90  KEY_KATAKANA
	NONE, //91  KEY_HIRAGANA
	NONE, //92  KEY_HENKAN
	NONE, //93  KEY_KATAKANA
	NONE, //94  KEY_MUHENKAN
	NONE, //95  KEY_KPJPCOMMA
	EXT | 0x5a, //96  KEY_KPENTER
	RCTRL | EXT | 0x14, //97  KEY_RIGHTCTRL
	EXT | 0x4a, //98  KEY_KPSLASH
	0xE2, //99  KEY_SYSRQ
	RALT | EXT | 0x11, //100 KEY_RIGHTALT
	NONE, //101 KEY_LINEFEED
	EXT | 0x6c, //102 KEY_HOME
	EXT | 0x75, //103 KEY_UP
	EXT | 0x7d, //104 KEY_PAGEUP
	EXT | 0x6b, //105 KEY_LEFT
	EXT | 0x74, //106 KEY_RIGHT
	EXT | 0x69, //107 KEY_END
	EXT | 0x72, //108 KEY_DOWN
	EXT | 0x7a, //109 KEY_PAGEDOWN
	EXT | 0x70, //110 KEY_INSERT
	EXT | 0x71, //111 KEY_DELETE
	NONE, //112 KEY_MACRO
	NONE, //113 KEY_MUTE
	NONE, //114 KEY_VOLUMEDOWN
	NONE, //115 KEY_VOLUMEUP
	NONE, //116 KEY_POWER
	NONE, //117 KEY_KPEQUAL
	NONE, //118 KEY_KPPLUSMINUS
	0xE1, //119 KEY_PAUSE
	NONE, //120 KEY_SCALE
	NONE, //121 KEY_KPCOMMA
	NONE, //122 KEY_HANGEUL
	NONE, //123 KEY_HANJA
	NONE, //124 KEY_YEN
	LGUI | EXT | 0x1f, //125 KEY_LEFTMETA
	RGUI | EXT | 0x27, //126 KEY_RIGHTMETA
	NONE, //127 KEY_COMPOSE
	NONE, //128 KEY_STOP
	NONE, //129 KEY_AGAIN
	NONE, //130 KEY_PROPS
	NONE, //131 KEY_UNDO
	NONE, //132 KEY_FRONT
	NONE, //133 KEY_COPY
	NONE, //134 KEY_OPEN
	NONE, //135 KEY_PASTE
	NONE, //136 KEY_FIND
	NONE, //137 KEY_CUT
	NONE, //138 KEY_HELP
	NONE, //139 KEY_MENU
	NONE, //140 KEY_CALC
	NONE, //141 KEY_SETUP
	NONE, //142 KEY_SLEEP
	NONE, //143 KEY_WAKEUP
	NONE, //144 KEY_FILE
	NONE, //145 KEY_SENDFILE
	NONE, //146 KEY_DELETEFILE
	NONE, //147 KEY_XFER
	NONE, //148 KEY_PROG1
	NONE, //149 KEY_PROG2
	NONE, //150 KEY_WWW
	NONE, //151 KEY_MSDOS
	NONE, //152 KEY_SCREENLOCK
	NONE, //153 KEY_DIRECTION
	NONE, //154 KEY_CYCLEWINDOWS
	NONE, //155 KEY_MAIL
	NONE, //156 KEY_BOOKMARKS
	NONE, //157 KEY_COMPUTER
	NONE, //158 KEY_BACK
	NONE, //159 KEY_FORWARD
	NONE, //160 KEY_CLOSECD
	NONE, //161 KEY_EJECTCD
	NONE, //162 KEY_EJECTCLOSECD
	NONE, //163 KEY_NEXTSONG
	NONE, //164 KEY_PLAYPAUSE
	NONE, //165 KEY_PREVIOUSSONG
	NONE, //166 KEY_STOPCD
	NONE, //167 KEY_RECORD
	NONE, //168 KEY_REWIND
	NONE, //169 KEY_PHONE
	NONE, //170 KEY_ISO
	NONE, //171 KEY_CONFIG
	NONE, //172 KEY_HOMEPAGE
	NONE, //173 KEY_REFRESH
	NONE, //174 KEY_EXIT
	NONE, //175 KEY_MOVE
	NONE, //176 KEY_EDIT
	NONE, //177 KEY_SCROLLUP
	NONE, //178 KEY_SCROLLDOWN
	NONE, //179 KEY_KPLEFTPAREN
	NONE, //180 KEY_KPRIGHTPAREN
	NONE, //181 KEY_NEW
	NONE, //182 KEY_REDO
	NONE, //183 KEY_F13
	NONE, //184 KEY_F14
	NONE, //185 KEY_F15
	NONE, //186 KEY_F16
	EMU_SWITCH_1 | 1, //187 KEY_F17
	EMU_SWITCH_1 | 2, //188 KEY_F18
	EMU_SWITCH_1 | 3, //189 KEY_F19
	EMU_SWITCH_1 | 4, //190 KEY_F20
	NONE, //191 KEY_F21
	NONE, //192 KEY_F22
	NONE, //193 KEY_F23
	0x5D, //194 U-mlaut on DE mapped to backslash
	NONE, //195 ???
	NONE, //196 ???
	NONE, //197 ???
	NONE, //198 ???
	NONE, //199 ???
	NONE, //200 KEY_PLAYCD
	NONE, //201 KEY_PAUSECD
	NONE, //202 KEY_PROG3
	NONE, //203 KEY_PROG4
	NONE, //204 KEY_DASHBOARD
	NONE, //205 KEY_SUSPEND
	NONE, //206 KEY_CLOSE
	NONE, //207 KEY_PLAY
	NONE, //208 KEY_FASTFORWARD
	NONE, //209 KEY_BASSBOOST
	NONE, //210 KEY_PRINT
	NONE, //211 KEY_HP
	NONE, //212 KEY_CAMERA
	NONE, //213 KEY_SOUND
	NONE, //214 KEY_QUESTION
	NONE, //215 KEY_EMAIL
	NONE, //216 KEY_CHAT
	NONE, //217 KEY_SEARCH
	NONE, //218 KEY_CONNECT
	NONE, //219 KEY_FINANCE
	NONE, //220 KEY_SPORT
	NONE, //221 KEY_SHOP
	NONE, //222 KEY_ALTERASE
	NONE, //223 KEY_CANCEL
	NONE, //224 KEY_BRIGHT_DOWN
	NONE, //225 KEY_BRIGHT_UP
	NONE, //226 KEY_MEDIA
	NONE, //227 KEY_SWITCHVIDEO
	NONE, //228 KEY_DILLUMTOGGLE
	NONE, //229 KEY_DILLUMDOWN
	NONE, //230 KEY_DILLUMUP
	NONE, //231 KEY_SEND
	NONE, //232 KEY_REPLY
	NONE, //233 KEY_FORWARDMAIL
	NONE, //234 KEY_SAVE
	NONE, //235 KEY_DOCUMENTS
	NONE, //236 KEY_BATTERY
	NONE, //237 KEY_BLUETOOTH
	NONE, //238 KEY_WLAN
	NONE, //239 KEY_UWB
	NONE, //240 KEY_UNKNOWN
	NONE, //241 KEY_VIDEO_NEXT
	NONE, //242 KEY_VIDEO_PREV
	NONE, //243 KEY_BRIGHT_CYCLE
	NONE, //244 KEY_BRIGHT_AUTO
	NONE, //245 KEY_DISPLAY_OFF
	NONE, //246 KEY_WWAN
	NONE, //247 KEY_RFKILL
	NONE, //248 KEY_MICMUTE
	NONE, //249 ???
	NONE, //250 ???
	NONE, //251 ???
	NONE, //252 ???
	NONE, //253 ???
	NONE, //254 ???
	NONE  //255 ???
};
uint32_t get_ps2_code(uint16_t key)
{
	if (key > 255) return NONE;
	return ev2ps2[key];
}

static void mainloop_new(void)
{
    int step = srows / PAGESTEPS;
    int hstep = scols / PAGESTEPS;
    int done=0;

    struct timeval nowtime;

    term_setup();
    signal(SIGCONT, sigcont);

    loadpage(num);
    srow = prow;
    scol = -scols / 2;
    draw();

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
//
// REMOVE THE ABS code, because mister sends us arrow keys for the joystick, all nicely mapped
#if 0
		 // code is the axis
		 // value is the y direction
                if (ev.code==1) {
                    if (ev.value>110 || ev.value<-110) {
                         if (ev.value>110)
                            srow += step * getcount(1);
                         if (ev.value<-110){
                            srow -= step * getcount(1);
			   }

			 // try to make us lock to the viewport
			 if (srow < prow) srow=prow;
			 if (prow+srow>-srows) srow = prows - srows+ prow;
		    }
               } else if (ev.code==0) {
		       if (ev.value>110 || ev.value<-110) {

                        if (ev.value>110)
                            scol += hstep * getcount(1);
                        if (ev.value<-110)
                            scol -= hstep * getcount(1);

			 if (scol < pcol) scol=pcol;
			 if (pcol+scol>-scols) scol = pcols - scols+ pcol;
                   }
               }
#endif
	 } else if (ev.type==EV_KEY) {

		 if (ev.code <256) {
			uint32_t ps2code = get_ps2_code(ev.code);
			if (ev.value) modifier |= ps2code;
			else modifier &= ~ps2code;
		 }


		int shift = (get_key_mod() & LSHIFT) || (get_key_mod() & RSHIFT);
                int ctrl  = (get_key_mod() & (LCTRL | RCTRL));
		 // ev.code >= 256 are joystick buttons
		
		 //  Check time
	         gettimeofday(&nowtime,NULL);
		 struct timeval result;
		timersub(&nowtime,&ev.time,&result);
		double time_in_mill = (result.tv_sec)*1000+(result.tv_usec)/1000;
		if (time_in_mill < 500)
		 switch (ev.code) {
			 case KEY_HOME:
                         if (!loadpage(1  ))
                                srow = prow;
			 break;
			 case KEY_END:
                         if (!loadpage(getcount(doc_pages(doc) ) ))
                                srow = prow;
			 break;
			 case KEY_ENTER:  
			 	if (shift) {  // previous screen
					srow = prow;
				}
				else {  // next screen
					srow = prow + prows - srows;
				}
			 break;
			 case KEY_ESC:  // ESC
				 done=1;
			 break;
			case KEY_UP:
			 	if (shift && ctrl) {
                         	   if (!loadpage(1  ))
                               	 	srow = prow;

				} else { // up arrow - scroll up
                            	    srow -= step * getcount(1);

					 // try to make us lock to the viewport
			 	    if (srow < prow) srow=prow;
				}
			 break;
			case KEY_DOWN:
			 	if (shift && ctrl) {
                         		if (!loadpage(getcount(doc_pages(doc) ) ))
                               	 		srow = prow;

				} else { // up arrow - scroll up
	                            srow += step * getcount(1);

					 // try to make us lock to the viewport
				    if (prow+srow>-srows) srow = prows - srows+ prow;
				}
			 break;
			case KEY_LEFT:
#if 0
                            scol -= hstep * getcount(1);

			     if (scol < pcol) scol=pcol;
			     if (pcol+scol>-scols) scol = pcols - scols+ pcol;
#else
                             if (!loadpage(num - getcount(1)))
                                 srow = prow;
#endif
			 break;
			case KEY_RIGHT:
#if 0
                            scol += hstep * getcount(1);
			     if (scol < pcol) scol=pcol;
		   	    if (pcol+scol>-scols) scol = pcols - scols+ pcol;
#else

                             if (!loadpage(num + getcount(1)))
                                 srow = prow;
#endif
			 break;
			case KEY_PAGEUP:
			 	if (shift && ctrl) {
                         		if (!loadpage(1  ))
                               	 		srow = prow;

				}
				else if (ctrl) {
                        	     if (!loadpage(num - getcount(1)))
                               		  srow = prow;
				}  else {
					srow = prow;
				}
			break;
			case KEY_PAGEDOWN:
				if (shift & ctrl) {
                         		if (!loadpage(getcount(doc_pages(doc) ) ))
                                		srow = prow;
				} else if (ctrl) {
                             	   if (!loadpage(num + getcount(1)))
                                 	srow = prow;
				} else {
					srow = prow + prows - srows;
				}
			break;
			case KEY_MINUS:
				if (ctrl) {
				   zoom_page(zoom - 75);
				}
			break;
			case KEY_EQUAL:
			 	if (ctrl) {
				   zoom_page(zoom + 75);
				}
			break;

#if 0
// remove this code, because we are going to use <esc> and <enter> sent by the mister
// all nicely mapped
			// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/input-event-codes.h#L381
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
			 case BTN_SELECT:
			 	rotate = rotate +90;
				if (rotate>=360) rotate=0;
				if (!loadpage(num))
					srow = prow;
				break;
				/*
			 case BTN_TR2:
			 	rotate = rotate -90;
				if (!loadpage(num))
					srow = prow;
				break;
				*/
#endif
		 }
	 }
	srow = MAX(prow - srows + MARGIN, MIN(prow + prows - MARGIN, srow));
	scol = MAX(pcol - scols + MARGIN, MIN(pcol + pcols - MARGIN, scol));

	draw();
	
     }
   }

   term_cleanup();

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
