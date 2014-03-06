LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= fontconfig
LOCAL_SRC_FILES     	:= lib/libfontconfig.so
LOCAL_SHARED_LIBRARIES 	:= libxml2
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, libxml2)
