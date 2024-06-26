include ../../../../build.mak
include ../../../../build/common.mak

ifneq ($(findstring android,$(TARGET_NAME)),)
    OS=android
else
    ifneq ($(findstring ios,$(TARGET_NAME)),)
	OS=ios
    endif
endif
SWIG_FLAGS=-I../../../../pjlib/include \
		   -I../../../../pjlib-util/include \
		   -I../../../../pjmedia/include \
		   -I../../../../pjsip/include \
		   -I../../../../pjnath/include \
		   -I../../../../pjnat64/include -c++
SRC_DIR=../../../../pjsip/include
SRCS=$(SRC_DIR)/pjsua2/endpoint.hpp $(SRC_DIR)/pjsua2/types.hpp

SWIG_FLAGS += -w312

PROJ_NAME=pjsua2xamarin
OUT_DIR=$(PROJ_NAME)/$(PROJ_NAME)/pjsua2
NAMESPACE=$(PROJ_NAME).pjsua2

ARCH=$(TARGET_ARCH)

ifeq ($(OS),android)
    LIBPJSUA2_DIR=$(PROJ_NAME)/$(PROJ_NAME).Android/lib/$(ARCH)
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.so
    SUP_CLASS_DIR=$(PROJ_NAME)/$(PROJ_NAME).Android/org/pjsip
else
    ifeq ($(OS),ios)
        LIBPJSUA2_DIR=$(PROJ_NAME)/$(PROJ_NAME).iOS/lib/$(ARCH)
        SWIG_FLAGS += -dllimport "__Internal"
    else
        LIBPJSUA2_DIR=$(PROJ_NAME)/lib
    endif
    LIBPJSUA2=$(LIBPJSUA2_DIR)/libpjsua2.a
endif

ifeq ($(OS),darwin)
	PJNAT64_LDLIBS=-Wl,-all_load $(PJNAT64_LDLIB)
else
	PJNAT64_LDLIBS=-Wl,--whole-archive $(PJNAT64_LDLIB) -Wl,--no-whole-archive
endif

# Build settings
MY_CFLAGS	 := $(PJ_CXXFLAGS) -I$(PJDIR)/pjnat64/include $(CFLAGS)
MY_LDFLAGS	 := $(PJ_LDXXFLAGS) -L$(PJDIR)/pjnat64/lib $(PJ_LDXXLIBS) $(PJNAT64_LDLIBS) $(LDFLAGS)

.PHONY: all install uninstall

all: $(LIBPJSUA2) sample

$(LIBPJSUA2): $(OUT_DIR)/pjsua2_wrap.o
	mkdir -p $(LIBPJSUA2_DIR)
ifeq ($(OS),android)
	mkdir -p $(SUP_CLASS_DIR)
	$(PJ_CXX) -shared -o $(LIBPJSUA2) $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS) $(MY_LDFLAGS)
	# copy libc++_shared.so manually
	cp -f ${STD_CPP_LIB} $(LIBPJSUA2_DIR)
else
	$(AR) $(LIBPJSUA2) $(AR_FLAGS) $(OUT_DIR)/pjsua2_wrap.o $(PJ_LIBXX_FILES)
endif
ifneq (,$(findstring PJMEDIA_VIDEO_DEV_HAS_ANDROID=1,$(ANDROID_CFLAGS)))
	@echo "Copying Android camera helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-videodev/android/PjCamera*.java $(SUP_CLASS_DIR)/
endif
ifneq (,$(findstring PJMEDIA_AUDIO_DEV_HAS_OBOE=1,$(OBOE_CFLAGS)))
	@echo "Copying Android Oboe audio device helper components..."
	cp $(PJDIR)/pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java $(SUP_CLASS_DIR)/
endif

$(OUT_DIR)/pjsua2_wrap.o: $(OUT_DIR)/pjsua2_wrap.cpp
	$(PJ_CXX) -c $(OUT_DIR)/pjsua2_wrap.cpp -o $(OUT_DIR)/pjsua2_wrap.o \
		$(MY_CFLAGS)

$(OUT_DIR)/pjsua2_wrap.cpp: ../pjsua2.i ../symbols.i Makefile $(SRCS)
	mkdir -p $(OUT_DIR)
	swig $(SWIG_FLAGS) -namespace $(NAMESPACE) -csharp -o $(OUT_DIR)/pjsua2_wrap.cpp ../pjsua2.i

sample: sample.cs
	@echo "Copying sample code..."
	cp sample.cs $(PROJ_NAME)/$(PROJ_NAME)

clean distclean realclean:
	rm -rf ./$(OUT_DIR)/*
	rm -rf ./$(LIBPJSUA2_DIR)/*
	rm -rf ./$(SUP_CLASS_DIR)/*.java

install:

uninstall:
