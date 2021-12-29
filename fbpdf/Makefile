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
	$(CC) -o $@ $^ $(LDFLAGS) -pthread -lmupdf -lmupdf-third -lm -lmujs  -l:libopenjp2.a -l:libjbig2dec.a -l:libjpeg.a -lz -l:libharfbuzz.a  -lfreetype -lstdc++ -l:libgraphite2.a

# djvu support
fbdjvu: fbpdf.o djvulibre.o draw.o  events.o
	$(CXX) -o $@ $^ $(LDFLAGS) -ldjvulibre -ljpeg -lm -lpthread

# pdf support using poppler
poppler.o: poppler.c
	$(CXX) -c $(CFLAGS) `pkg-config --cflags poppler-cpp` $<
fbpdf2: fbpdf.o poppler.o draw.o  events.o
	$(CXX) -o $@ $^ $(LDFLAGS)  -l:libpoppler-cpp.a -l:libpoppler.a  -lpthread -lfreetype -lpng -l:libjpeg.a -l:libopenjp2.a -L/usr/local/src/nss-3.26/dist/Linux5.4_arm_gcc_glibc_PTH_DBG.OBJ/lib/ \
	-l:libcertdb.a \
	-l:libcerthi.a \
	-l:libcrmf.a \
	-l:libcryptohi.a \
	-l:libfreebl.a \
	-l:libjar.a \
	-l:libnss.a \
	-l:libnssb.a \
	-l:libnssckfw.a \
	-l:libnssdev.a \
	-l:libnsspki.a \
	-l:libnsssysinit.a \
	-l:libpk11wrap.a \
	-l:libpkcs12.a \
	-l:libpkcs7.a \
	-l:libsmime.a \
	-l:libsoftokn.a \
	-l:libsqlite.a \
	-l:libssl.a \
	-l:libpkixtop.a \
	-l:libpkixutil.a \
	-l:libpkixstore.a \
	-l:libpkixcertsel.a \
	-l:libpkixpki.a \
	-l:libpkixsystem.a \
	-l:libpkixmodule.a \
	-l:libpkixparams.a \
	-l:libpkixpki.a \
	-l:libpkixresults.a \
	-l:libpkixcrlsel.a \
	-l:libpkixchecker.a \
	-l:libnssutil.a \
	-l:libplc4.a \
	-l:libplds4.a \
	-l:libnspr4.a \
	-l:libnssb.a \
	-l:libcerthi.a \
	-l:liblcms2.a \
	-ltiff -ldl  -lstdc++ \
	-l:libfontconfig.a \
	-l:libexpat.a
#	`pkg-config --static --libs poppler-cpp`
#
#libpoppler-cpp.so.0 => /usr/lib/arm-linux-gnueabihf/libpoppler-cpp.so.0 (0xff788000)
#libpoppler.so.64 => /usr/lib/arm-linux-gnueabihf/libpoppler.so.64 (0xff5d9000)
#libstdc++.so.6 => /usr/lib/arm-linux-gnueabihf/libstdc++.so.6 (0xff4cd000)
#libm.so.6 => /lib/arm-linux-gnueabihf/libm.so.6 (0xff455000)
#libgcc_s.so.1 => /lib/arm-linux-gnueabihf/libgcc_s.so.1 (0xff42c000)
#libc.so.6 => /lib/arm-linux-gnueabihf/libc.so.6 (0xff33e000)
#/lib/ld-linux-armhf.so.3 (0xfffc7000)
#liblcms2.so.2 => /usr/lib/arm-linux-gnueabihf/liblcms2.so.2 (0xff2f5000)
#libtiff.so.5 => /usr/lib/arm-linux-gnueabihf/libtiff.so.5 (0xff28d000)
#libjpeg.so.62 => /usr/lib/arm-linux-gnueabihf/libjpeg.so.62 (0xff253000)
#libpng16.so.16 => /usr/lib/arm-linux-gnueabihf/libpng16.so.16 (0xff223000)
#libz.so.1 => /lib/arm-linux-gnueabihf/libz.so.1 (0xff201000)
#libnss3.so => /usr/lib/arm-linux-gnueabihf/libnss3.so (0xff13b000)
#libnssutil3.so => /usr/lib/arm-linux-gnueabihf/libnssutil3.so (0xff10f000)
#libsmime3.so => /usr/lib/arm-linux-gnueabihf/libsmime3.so (0xff0e6000)
#libssl3.so => /usr/lib/arm-linux-gnueabihf/libssl3.so (0xff0a3000)
#libplds4.so => /usr/lib/arm-linux-gnueabihf/libplds4.so (0xff090000)
#libplc4.so => /usr/lib/arm-linux-gnueabihf/libplc4.so (0xff07c000)
#libnspr4.so => /usr/lib/arm-linux-gnueabihf/libnspr4.so (0xff047000)
#libopenjp2.so.7 => /usr/lib/arm-linux-gnueabihf/libopenjp2.so.7 (0xff00f000)
#libfontconfig.so.1 => /usr/lib/arm-linux-gnueabihf/libfontconfig.so.1 (0xfefd7000)
#libfreetype.so.6 => /usr/lib/arm-linux-gnueabihf/libfreetype.so.6 (0xfef5e000)
#libpthread.so.0 => /lib/arm-linux-gnueabihf/libpthread.so.0 (0xfef3a000)
#liblzma.so.5 => /lib/arm-linux-gnueabihf/liblzma.so.5 (0xfef0e000)
#libjbig.so.0 => /usr/lib/arm-linux-gnueabihf/libjbig.so.0 (0xfeef3000)
#libdl.so.2 => /lib/arm-linux-gnueabihf/libdl.so.2 (0xfeee0000)
#librt.so.1 => /lib/arm-linux-gnueabihf/librt.so.1 (0xfeeca000)
#libexpat.so.1 => /lib/arm-linux-gnueabihf/libexpat.so.1 (0xfeea2000)
#
