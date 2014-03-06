LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= protobuf-c
LOCAL_SRC_FILES     	:= lib/libprotobuf-c.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)
