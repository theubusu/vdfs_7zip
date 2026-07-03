CODE=src/*.cpp include/7zip/CPP/Windows/TimeUtils.cpp
INCLUDE=-lOleAut32 -lUser32 -Iinclude/7zip
DEFINE=-D_CRT_SECURE_NO_WARNINGS
OUTDIR=bin
ifeq ($(DEBUG),1)
CFLAGS=-O0 -g
else
CFLAGS=-O3 -ffunction-sections -fdata-sections
endif

ifeq ($(DEBUG),1)
DEFINE += -DDEBUG
endif

all: x64 x86

x64:
	clang $(CODE) --target=x86_64-windows-msvc -shared $(INCLUDE) $(DEFINE) $(CFLAGS) -o $(OUTDIR)/fmt7z-x64.dll

x86:
	clang $(CODE) --target=i386-windows-msvc -shared $(INCLUDE) $(DEFINE) $(CFLAGS) -o $(OUTDIR)/fmt7z-x86.dll