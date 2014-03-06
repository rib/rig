LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= harfbuzz
LOCAL_SRC_FILES     	:= lib/libharfbuzz.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/harfbuzz

include $(PREBUILT_SHARED_LIBRARY)
