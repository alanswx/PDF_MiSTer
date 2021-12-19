
FROM misterkun/toolchain:latest

#RUN apt-get update && apt-get upgrade -y && apt-get install -y \
RUN apt-get update &&  apt-get install -y \
	libdjvulibre-dev \
 	libmupdf-dev \
	 mupdf \
	libharfbuzz-dev \
	liblcms2-dev  libtiff-dev libpng-dev libz-dev  \
	libjbig2dec0-dev \
	libpoppler-cpp-dev \
        vim

RUN cd /usr/local/src/ &&  wget "https://github.com/ccxvii/mujs/archive/refs/tags/1.2.0.tar.gz" && \
	tar zxf 1.2.0.tar.gz && ls && cd mujs-1.2.0 && make && make install

RUN cd /usr/local/src/ &&  wget "https://github.com/silnrsi/graphite/archive/refs/tags/1.3.14.tar.gz" && \
	tar zxf 1.3.14.tar.gz && ls && cd graphite-1.3.14 && mkdir build && cd build && cmake ../ -DBUILD_SHARED_LIBS:bool=off && \
	make && make install

RUN cd /usr/local/src/ &&  wget "https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.2.0.tar.gz" &&  \
	tar zxf v2.2.0.tar.gz && ls && cd openjpeg-2.2.0/ && mkdir build && cd build && \
	cmake ../ -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:bool=off && \
	make && make install

RUN cd /usr/local/src/ &&  wget "https://github.com/mm2/Little-CMS/archive/refs/tags/lcms2.8.tar.gz" &&  \
	tar zxf lcms2.8.tar.gz  && ls && cd Little-CMS-lcms2.8 && ./autogen.sh &&  \
	make && make install

RUN apt-get install -y \
	libexpat-dev \
	gperf

RUN cd /usr/local/src/ &&  wget --no-check-certificate "https://gitlab.freedesktop.org/fontconfig/fontconfig/-/archive/2.11.0/fontconfig-2.11.0.tar.gz" &&  \
	tar zxf fontconfig-2.11.0.tar.gz  && ls && cd fontconfig-2.11.0  && ./autogen.sh &&  \
	make && make install

RUN cd /usr/local/src/ && wget "https://ftp.mozilla.org/pub/security/nss/releases/NSS_3_26_RTM/src/nss-3.26-with-nspr-4.12.tar.gz" && \
	tar zxf nss-3.26-with-nspr-4.12.tar.gz && ls && cd  nss-3.26 && \
        cd nspr && ./configure --prefix /usr/ &&  make && make install && \
	cd ../nss && \
	echo "CFLAGS+=-I/usr/include/nspr/" > coreconf/Linux.mk.1 &&  \
	cp coreconf/Linux.mk coreconf/Linux.mk.2 && cat coreconf/Linux.mk.1 coreconf/Linux.mk.2 > coreconf/Linux.mk && \
        make nss_build_all  && \
        make nss_build_all  && \
        make nss_build_all  ; exit 0

RUN cd /usr/local/src/nss-3.26/nss/ && make nss_build_all ; exit 0
RUN cd /usr/local/src/nss-3.26/nss/ && make nss_build_all ; exit 0
RUN cd /usr/local/src/nss-3.26/nss/ && make nss_build_all ; exit 0
RUN cd /usr/local/src/nss-3.26/nss/ && make nss_build_all ; exit 0
RUN cd /usr/local/src/nss-3.26/nss/ && make nss_build_all ; exit 0

#	tar zxf nss-3.26-with-nspr-4.12.tar.gz && ls && cd  nss-3.26 && cd nspr && ./configure --prefix /usr/ &&  make && make install && \
#        cd nss && make nss_build_all && make install 

#        d nss && echo "CFLAGS+=-I/usr/local/include/nspr/" >> coreconf/Linux.mk && 
#        make && make install && ldconfig && make install



# need to build and install nspr first, then need to add include to the Linux.mk file /usr/local/include/nspr
# https://github.com/mm2/Little-CMS/archive/refs/tags/lcms2.8.tar.gz
# https://gitlab.freedesktop.org/fontconfig/fontconfig/-/archive/2.11.0/fontconfig-2.11.0.tar.gz

# gyp ninja-build
# /usr/include/nspr usr/local/include/nspr
# ./build.sh --static --system-nspr
