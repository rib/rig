LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        := sdl2
LOCAL_SRC_FILES     := lib/libSDL2-2.0.so
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/include/SDL2
LOCAL_LDLIBS	    := -llog -landroid

include $(PREBUILT_SHARED_LIBRARY)
