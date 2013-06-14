JAVA_SRC_PATH:=src/com/rcatolino/remoteclient
ARCH:=armeabi
APK:=bin/RemoteClient-debug.apk
LIB_PATH:=libs/$(ARCH)
libs=$(LIB_PATH)/libgstreamer_android.so $(LIB_PATH)/libmodgstreamer.so

GSTREAMER_TARGET:=release# Can be debug or release

GSTREAMER_SDK_ROOT:=gstreamer-sdk-$(GSTREAMER_TARGET)

.PHONY: all android-ndk clean clean-setup install jnilib setup smdp

all : $(APK)

jnilib : $(libs) $(GSTREAMER_SDK_ROOT)

$(APK) : $(libs) $(JAVA_SRC_PATH)/*.java
	@echo Building $@ because of $? in $(PWD)
	@ant debug

$(libs) : ./jni/*.c ./jni/*.mk
	@echo Building $@ because of $? in $(PWD)
	@export GSTREAMER_SDK_ROOT_ANDROID=$(PWD)/$(GSTREAMER_SDK_ROOT); ndk-build

clean :
	@export GSTREAMER_SDK_ROOT_ANDROID=$(PWD)/$(GSTREAMER_SDK_ROOT); ndk-build clean
	ant clean

install : $(APK)
	adb install -r $(APK)

setup : android-sdk android-ndk $(GSTREAMER_SDK_ROOT) smdp

android-sdk :
	@if which android 2> /dev/null;\
		then echo "sdk present";\
		else echo "No sdk present. You need to install the android ndk at \
http://developer.android.com/tools/sdk/ndk/index.html";\
		exit 1;\
		fi;
	android update project -s --path . --target 14

android-ndk :
	@if which ndk-build 2> /dev/null;\
		then echo "ndk present";\
		else echo "No ndk present. You need to install the android ndk at \
http://developer.android.com/tools/sdk/ndk/index.html";\
		exit 1;\
		fi;

smdp :
	@echo downloading smdp library
	if git clone http://github.com/rcatolino/smdp.git;\
		then cd smdp; make;\
		else cd smdp; git pull origin master; make;\
		fi;
	cp smdp/android/libs/armeabi/libsmdp.so libs/armeabi/;

$(GSTREAMER_SDK_ROOT) :
	@echo "Downloading gstreamer android-sdk"
	mkdir $(GSTREAMER_SDK_ROOT)
	wget "http://cdn.gstreamer.com/android/arm/gstreamer-sdk-android-arm-$(GSTREAMER_TARGET)-2012.11.tar.bz2"\
		-O $(GSTREAMER_SDK_ROOT).tar.bz2
	tar --directory=$(GSTREAMER_SDK_ROOT) -xjf $(GSTREAMER_SDK_ROOT).tar.bz2
	mv $(GSTREAMER_SDK_ROOT).tar.bz2 $(GSTREAMER_SDK_ROOT)
	mv $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build/gstreamer.mk{,.back}
	cp -f setup/gstreamer.mk $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build/gstreamer.mk

clean-setup :
	-rm -rf $(GSTREAMER_SDK_ROOT)
	-rm -f $(GSTREAMER_SDK_ROOT).tar.bz2
	-rm -rf smdp
