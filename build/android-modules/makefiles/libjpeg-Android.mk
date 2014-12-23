LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= libjpeg
LOCAL_SRC_FILES     	:= lib/libjpeg.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)
