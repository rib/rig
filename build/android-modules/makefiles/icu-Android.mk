LOCAL_PATH := $(call my-dir)

#libicuuc
include $(CLEAR_VARS)

LOCAL_MODULE        	:= icuuc
LOCAL_SRC_FILES     	:= lib/libicuuc.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)

#libicudata
include $(CLEAR_VARS)

LOCAL_MODULE        	:= icudata
LOCAL_SRC_FILES     	:= lib/libicudata.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)
