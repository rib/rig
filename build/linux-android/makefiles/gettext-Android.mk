LOCAL_PATH := $(call my-dir)

# libintl
include $(CLEAR_VARS)

LOCAL_MODULE        	:= libintl
LOCAL_SRC_FILES     	:= lib/libintl.so
LOCAL_SHARED_LIBRARIES 	:= libiconv
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, libiconv)
