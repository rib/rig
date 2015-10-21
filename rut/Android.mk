LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := rut
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/*.c)
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := clib cglib android_native_app_glue
LOCAL_SHARED_LIBRARIES := libuv

include $(BUILD_STATIC_LIBRARY)

$(call import-module, libuv)
$(call import-module, android/native_app_glue)
