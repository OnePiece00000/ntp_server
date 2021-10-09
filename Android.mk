LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE = ntp_server

LOCAL_SRC_FILES = long2time.cpp \
                  main.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_INIT_RC := ntp_server.rc

LOCAL_CFLAGS += -Wall -Wno-unused-parameter

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog 

LOCAL_SHARED_LIBRARIES := \
            libcutils \
            liblog \
            libandroidfw \
            libutils
            



include $(BUILD_EXECUTABLE)
