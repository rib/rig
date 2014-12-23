LOCAL_PATH := $(call my-dir)

# iconv
include $(CLEAR_VARS)

LOCAL_MODULE 			:= libiconv
LOCAL_SRC_FILES 		:= lib/libiconv.so

include $(PREBUILT_SHARED_LIBRARY)
