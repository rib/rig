LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= libxml2
LOCAL_SRC_FILES     	:= lib/libxml2.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/libxml2

include $(PREBUILT_SHARED_LIBRARY)
