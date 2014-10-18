LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := rig
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/protobuf-c-rpc
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG -DICU_DATA_DIR=\"/icu\" -DU_DISABLE_RENAMING=1
LOCAL_SRC_FILES := \
    $(wildcard $(LOCAL_PATH)/*.c) \
    $(wildcard $(LOCAL_PATH)/components/*.c) \
    $(wildcard $(LOCAL_PATH)/protobuf-c-rpc/*.c)
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH) -I$(LOCAL_PATH)/protobuf-c-rpc

LOCAL_STATIC_LIBRARIES := clib cogl rut
LOCAL_SHARED_LIBRARIES := protobuf-c icuuc fontconfig freetype harfbuzz harfbuzz-icu

include $(BUILD_STATIC_LIBRARY)

$(call import-module, clib)
$(call import-module, cogl)
$(call import-module, rut)
$(call import-module, protobuf-c)
$(call import-module, icu)
$(call import-module, libuv)
$(call import-module, fontconfig)
$(call import-module, freetype)
$(call import-module, harfbuzz)
