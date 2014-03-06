LOCAL_PATH := $(call my-dir)

# A dummy module just to be able to include the cairo headers...
include $(CLEAR_VARS)

LOCAL_MODULE        	:= cairo-includes
LOCAL_SRC_FILES     	:=
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cairo

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= cairo
LOCAL_SRC_FILES     	:= lib/libcairo.so
LOCAL_SHARED_LIBRARIES 	:= pixman
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cairo

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, pixman)
