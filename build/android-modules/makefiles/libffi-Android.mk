LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= libffi
LOCAL_SRC_FILES     	:= lib/libffi.so
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/lib/libffi-3.0.13/include

include $(PREBUILT_SHARED_LIBRARY)
