JAVA_SRC_PATH:=src/com/rcatolino/remoteclient
ARCH:=armeabi
APK:=bin/RemoteClient-debug.apk
LIB_PATH:=libs/$(ARCH)
NDK_OPT:=
smdplib=$(LIB_PATH)/libsmdp.so

SMDP:=../smdp/android/libs/armeabi/libsmdp.so

.PHONY: all android-ndk clean clean-setup install jnilib setup smdp

all : $(APK)

jnilib : $(smdplib)

$(APK) : $(smdplib) $(JAVA_SRC_PATH)/*.java
	@echo Building $@ because of $? in $(PWD)
	@ant debug

clean :
	ant clean

install : $(APK)
	adb install -r $(APK)

debug :
	ndk-gdb --start --force

setup : android-sdk android-ndk smdp

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

smdp : $(smdplib)

$(smdplib) : $(SMDP) $(LIB_PATH)
	@echo installing smdp library
	cp $(SMDP) $(LIB_PATH)/;
	cp -r ../smdp/android/src/ ./

$(LIB_PATH) :
	mkdir $(LIB_PATH)

$(SMDP) :
	@echo building smdp library
	cd ../smdp && make > /dev/null

clean-setup :
	-rm -rf src/com/rcatolino/smdp
	-rm -f $(smdplib)
