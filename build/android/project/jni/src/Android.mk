LOCAL_PATH := $(call my-dir)

RIG_PATH := $(LOCAL_PATH)/../../../../..
include $(RIG_PATH)/cogl/Makefile.sources
include $(RIG_PATH)/rut/Makefile.sources
include $(RIG_PATH)/rig/Makefile.sources

# Shared utilities...

# C utility library
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := clib
LOCAL_C_INCLUDES := $(RIG_PATH)/clib/src $(RIG_PATH)/clib/src/sfmt
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG -DSFMT_MEXP=19937 -DHAVE_CONFIG_H
LOCAL_SRC_FILES := \
    $(subst $(LOCAL_PATH)/,, \
    	$(filter-out %win32.c, \
	  	$(wildcard $(RIG_PATH)/clib/src/*.c) \
	 ) \
	$(wildcard $(RIG_PATH)/clib/src/sfmt/*.c) \
     )

include $(BUILD_STATIC_LIBRARY)

# GPU graphics utility library
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := cogl
LOCAL_C_INCLUDES := $(LOCAL_PATH)/cogl $(RIG_PATH) \
    $(RIG_PATH)/cogl $(RIG_PATH)/cogl/driver $(RIG_PATH)/cogl/driver/nop $(RIG_PATH)/cogl/driver/gl $(RIG_PATH)/cogl/winsys \
    $(RIG_PATH)/clib/src
LOCAL_CFLAGS := -DHAVE_CONFIG_H -DCG_COMPILATION -DCG_GLES2_LIBNAME=\"libGLESv3.so\" -DCG_ENABLE_DEBUG -DCG_GL_DEBUG -DCG_OBJECT_DEBUG
LOCAL_SRC_FILES := \
    $(filter-out %.h, \
	$(subst $(LOCAL_PATH)/,, \
	    $(addprefix $(RIG_PATH)/cogl/,$(cogl_sources_c)) \
	    $(addprefix $(RIG_PATH)/cogl/,$(cogl_uv_sources_c)) \
	    $(addprefix $(RIG_PATH)/cogl/,$(cogl_driver_gles_sources)) \
	    $(addprefix $(RIG_PATH)/cogl/,$(cogl_egl_sources_c)) \
	    $(addprefix $(RIG_PATH)/cogl/,$(cogl_egl_android_sources_c)) \
	) \
    )

LOCAL_STATIC_LIBRARIES := clib
LOCAL_SHARED_LIBRARIES := libuv

include $(BUILD_STATIC_LIBRARY)

# Rig utility library
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rut
LOCAL_C_INCLUDES := $(RIG_PATH) $(RIG_PATH)/rut $(RIG_PATH)/clib/src
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := \
    $(filter-out %.h, \
	$(subst $(LOCAL_PATH)/,, \
	    $(addprefix $(RIG_PATH)/rut/,$(rut_common_sources)) \
	    $(addprefix $(RIG_PATH)/rut/,fmemopen.c) \
	 ) \
     )

LOCAL_STATIC_LIBRARIES := clib cogl
LOCAL_SHARED_LIBRARIES := libuv

include $(BUILD_STATIC_LIBRARY)

# Slave
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rig-slave
LOCAL_C_INCLUDES := $(RIG_PATH) $(RIG_PATH)/clib/src $(RIG_PATH)/rut $(RIG_PATH)/rig $(RIG_PATH)/rig/protobuf-c-rpc
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG -DICU_DATA_DIR=\"/icu\" -DU_DISABLE_RENAMING=1
LOCAL_SRC_FILES := \
	$(filter-out %.h, \
	    $(subst $(LOCAL_PATH)/,, \
			    $(addprefix $(RIG_PATH)/rig/,$(rig_common_sources)) \
			    $(addprefix $(RIG_PATH)/rig/,$(rig_slave_sources)) \
	     ) \
	 )
LOCAL_STATIC_LIBRARIES := clib rut cogl
LOCAL_SHARED_LIBRARIES := protobuf-c icuuc fontconfig freetype harfbuzz harfbuzz-icu

LOCAL_LDLIBS := -ldl -lGLESv3 -llog -landroid

include $(BUILD_SHARED_LIBRARY)

$(call import-module, protobuf-c)
$(call import-module, icu)
$(call import-module, libuv)
$(call import-module, fontconfig)
$(call import-module, freetype)
$(call import-module, harfbuzz)
