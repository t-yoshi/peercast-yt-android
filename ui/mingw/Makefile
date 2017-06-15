# WIN32: g++ �� WIN32 ���`���Ȃ����A�\�[�X�R�[�h�� WIN32 �� Windows
# �����o���Ă���̂ł����Œ�`����B

# __USE_MINGW_ANSI_STDIO: ��`����Ă���� vsnprintf �� stdio.h �Œ�`
# ����Alibmingwex.a ���̒�`�ƏՓ˂���̂Ŗ���`�ɂ���B

CPPFLAGS = -g -Wno-multichar -Wno-unused-variable -std=c++11 -DWIN32 -D_REENTRANT -D__USE_MINGW_ANSI_STDIO=0 $(INCLUDES)
LDFLAGS = -static-libgcc -static-libstdc++ -pthread
LIBS = -lws2_32

OS = $(shell uname -s | tr A-Z a-z)
ARCH = $(shell uname -p | tr A-Z a-z)

DISTARCHIVE = peercast-yt-$(OS)-$(ARCH).tar.gz
AR = ar

INCLUDES = -I../../core -I../../core/common

TARGET = peercast.exe
CORE = ../../core
SRCHTMLTAR = ../html.tar
HTML = html
SRCPUBLICTAR = ../public.tar
PUBLIC = public
SRCASSETSTAR = ../assets.tar
ASSETS = assets

LIBOBJSTATIC = libpeercast.a

SYSSOURCE = $(wildcard $(CORE)/win32/*.cpp)
CORESOURCE = $(wildcard $(CORE)/common/*.cpp)
MISSINGSOURCE = ../../missing/strdup.cpp

COREOBJ = $(notdir $(CORESOURCE:.cpp=.o))
SYSOBJ = $(notdir $(SYSSOURCE:.cpp=.o))
MISSINGOBJ = $(notdir $(MISSINGSOURCE:.cpp=.o))

LINUXSOURCE = main.cpp
LINUXOBJ = $(notdir $(LINUXSOURCE:.cpp=.o))

DEPENDFILE = .deps

.PHONY: all clean build-html deps $(SRCHTMLTAR) $(SRCPUBLICTAR) $(SRCASSETSTAR)

# Here follow the generic build rules.
all: $(TARGET) $(LIBOBJSTATIC) $(DISTARCHIVE)

deps: $(DEPENDFILE)

$(SRCHTMLTAR):
	$(MAKE) -C .. html.tar

$(HTML): $(SRCHTMLTAR)
	rm -rf $(HTML)
	tar xf $(SRCHTMLTAR)
	touch $(HTML)

$(SRCPUBLICTAR):
	$(MAKE) -C .. public.tar

$(PUBLIC): $(SRCPUBLICTAR)
	rm -rf $(PUBLIC)
	tar xf $(SRCPUBLICTAR)
	touch $(PUBLIC)

$(SRCASSETSTAR):
	$(MAKE) -C .. assets.tar

$(ASSETS): $(SRCASSETSTAR)
	rm -rf $(ASSETS)
	tar xf $(SRCASSETSTAR)
	touch $(ASSETS)

$(DISTARCHIVE): $(TARGET) $(HTML) $(PUBLIC) $(ASSETS)
	rm -rf peercast-yt
	mkdir peercast-yt
	cp -R $(HTML) $(PUBLIC) $(TARGET) $(ASSETS) ../../licenses ../../core/LICENSE peercast-yt/
	strip peercast-yt/$(TARGET)
	tar czf $(DISTARCHIVE) peercast-yt

clean:
	$(MAKE) -C .. clean
	rm -f $(LINUXOBJ) $(COREOBJ) $(SYSOBJ) $(TARGET) ${LIBOBJSTATIC} $(COREOBJSTATIC) $(DEPENDFILE)
	rm -rf $(HTML) $(PUBLIC) $(ASSETS)
	rm -rf peercast-yt/
	rm -f $(DISTARCHIVE)

$(COREOBJ): %.o : $(CORE)/common/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(SYSOBJ): %.o : $(CORE)/win32/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(MISSINGOBJ): %.o : ../../missing/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(DEPENDFILE): $(CORESOURCE) $(LINUXSOURCE)
	$(CXX) $(CPPFLAGS) -MM $(CORESOURCE) $(SYSSOURCE) $(LINUXSOURCE) $(MISSINGSOURCE)> $(DEPENDFILE)

$(LIBOBJSTATIC): $(COREOBJ) $(SYSOBJ)
	$(AR) rcs $(LIBOBJSTATIC) $(COREOBJ) $(SYSOBJ) $(MISSINGOBJ)

$(TARGET): $(LINUXOBJ) $(LIBOBJSTATIC)
	$(CXX) $(LDFLAGS) -o $@ $(LINUXOBJ) $(LIBOBJSTATIC) $(LIBS)

-include $(DEPENDFILE)