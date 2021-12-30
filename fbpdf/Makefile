PREFIX = .
CC = cc
CFLAGS = -Wall -O2 -I$(PREFIX)/include
LDFLAGS = -L$(PREFIX)/lib

all: fbpdf fbpdf2 fbdjvu
%.o: %.c doc.h
	$(CC) -c $(CFLAGS) $<
clean:
	-rm -f *.o fbpdf fbdjvu fbpdf2

# pdf support using mupdf
fbpdf: fbpdf.o mupdf.o draw.o  events.o
	$(CC) -o $@ $^ $(LDFLAGS) -pthread -lmupdf -lm -lmujs  -l:libopenjp2.a -l:libjbig2dec.a -l:libjpeg.a -lz -l:libharfbuzz.a  -lfreetype -lstdc++ -l:libgraphite2.a

# djvu support
fbdjvu: fbpdf.o djvulibre.o draw.o  events.o
	$(CXX) -o $@ $^ $(LDFLAGS) -ldjvulibre -ljpeg -lm -lpthread

# pdf support using poppler
poppler.o: poppler.c
	$(CXX) -c $(CFLAGS) `pkg-config --cflags poppler-cpp` $<

fbpdf2: fbpdf.o poppler.o draw.o  events.o
	$(CXX) -o $@ $^ $(LDFLAGS)  -l:libpoppler-cpp.a -l:libpoppler.a  -lpthread -lfreetype -lpng -l:libjpeg.a -l:libopenjp2.a \
	-l:liblcms2.a \
	-ltiff -ldl  -lstdc++ \
	-l:libfontconfig.a \
	-l:libuuid.a \
	-l:libexpat.a
