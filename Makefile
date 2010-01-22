CC	:=	gcc

CFLAGS	:=	-Wall -O2 -s

ifneq (,$(findstring MINGW,$(shell uname -s)))
	exeext		:= .exe
endif

ifneq (,$(findstring Linux,$(shell uname -s)))
	CFLAGS += -static
endif

ifneq (,$(findstring Darwin,$(shell uname -s)))
	SDK	:=	/Developer/SDKs/MacOSX10.4u.sdk
	CFLAGS += -mmacosx-version-min=10.4 -isysroot $(SDK) -Wl,-syslibroot,$(SDK) -arch i386 -arch ppc
endif

tools	:=	$(patsubst %.c,%$(exeext),$(wildcard *.c)) \
		$(patsubst %.cpp,%$(exeext),$(wildcard *.cpp))


all:	$(tools)

clean:
	@rm -f $(tools)

gbfs.exe	:	gbfs.c
	$(CC) $< -o $@ $(CFLAGS) -liberty

%$(exeext)	: %.c
	$(CC) $< -o $@ $(CFLAGS)

%$(exeext)	: %.cpp
	$(CXX) $< -o $@ $(CFLAGS)

install:
	cp  $(tools) $(PREFIX)
