LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        := cpufeatures
LOCAL_SRC_FILES     := libcpufeatures.a
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/jni

include $(PREBUILT_STATIC_LIBRARY)
