LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := cglib
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../ $(LOCAL_PATH) $(LOCAL_PATH)/driver $(LOCAL_PATH)/driver/nop $(LOCAL_PATH)/driver/gl $(LOCAL_PATH)/winsys
LOCAL_CFLAGS := -DHAVE_CONFIG_H -DCG_COMPILATION -DCG_GLES2_LIBNAME=\"libGLESv3.so\" -DCG_ENABLE_DEBUG -DCG_GL_DEBUG -DCG_OBJECT_DEBUG
LOCAL_SRC_FILES := \
    $(wildcard $(LOCAL_PATH)/*.c) \
    $(wildcard $(LOCAL_PATH)/winsys/*.c) \
    $(wildcard $(LOCAL_PATH)/driver/nop/*.c) \
    $(wildcard $(LOCAL_PATH)/driver/gl/*.c) \
    $(wildcard $(LOCAL_PATH)/driver/gl/gles/*.c)

LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/../

LOCAL_STATIC_LIBRARIES := clib
LOCAL_SHARED_LIBRARIES := libuv

include $(BUILD_STATIC_LIBRARY)

$(call import-module, libuv)
