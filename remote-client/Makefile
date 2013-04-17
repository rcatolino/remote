JAVA_SRC_PATH:=src/com/rcatolino/remoteclient
ARCH:=armeabi
APK:=bin/RemoteClient-debug.apk
LIB_PATH:=libs/$(ARCH)
libs=$(LIB_PATH)/libgstreamer_android.so $(LIB_PATH)/libmodgstreamer.so

GSTREAMER_SDK_ROOT:=gstreamer-sdk

.PHONY: all install jnilib setup clean-setup android-ndk

all : $(APK)

jnilib : $(libs) $(GSTREAMER_SDK_ROOT)

$(APK) : $(libs) $(JAVA_SRC_PATH)/*.java
	@echo Building $@ because of $? in $(PWD)
	@ant debug

$(libs) : ./jni/*.c ./jni/*.mk
	@echo Building $@ because of $? in $(PWD)
	@export GSTREAMER_SDK_ROOT_ANDROID=$(PWD)/$(GSTREAMER_SDK_ROOT); ndk-build

install : $(APK)
	adb install -r $(APK)

setup : android-ndk $(GSTREAMER_SDK_ROOT)

android-ndk :
	@if which ndk-build 2> /dev/null;\
		then echo "ndk present";\
		else echo "No ndk present. You need to install the android ndk at \
http://developer.android.com/tools/sdk/ndk/index.html";\
		exit 1;\
		fi;

$(GSTREAMER_SDK_ROOT) :
	@echo "Downloading gstreamer android-sdk"
	mkdir $(GSTREAMER_SDK_ROOT)
	wget "http://cdn.gstreamer.com/android/arm/gstreamer-sdk-android-arm-release-2012.11.tar.bz2"\
		-O $(GSTREAMER_SDK_ROOT).tar.bz2
	tar --directory=$(GSTREAMER_SDK_ROOT) -xjf $(GSTREAMER_SDK_ROOT).tar.bz2
	mv $(GSTREAMER_SDK_ROOT).tar.bz2 $(GSTREAMER_SDK_ROOT)
	mv $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build/gstreamer.mk{,.back}
	cp -f setup/gstreamer.mk $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build/gstreamer.mk

clean-setup :
	-rm -rf $(GSTREAMER_SDK_ROOT)
	-rm -f $(GSTREAMER_SDK_ROOT).tar.bz2
