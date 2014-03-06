LOCAL_PATH := $(call my-dir)

# glib
include $(CLEAR_VARS)

LOCAL_MODULE    		:= glib
LOCAL_SRC_FILES 		:= lib/libglib-2.0.so
LOCAL_SHARED_LIBRARIES 	:= libiconv libintl
LOCAL_LDLIBS 			:= -llog

LOCAL_EXPORT_CFLAGS := 				\
	-I$(LOCAL_PATH)/include/glib-2.0	\
	-I$(LOCAL_PATH)/lib/glib-2.0/include	\
	$(NULL)

include $(PREBUILT_SHARED_LIBRARY)

# gobject
include $(CLEAR_VARS)

LOCAL_MODULE    		:= gobject
LOCAL_SRC_FILES 		:= lib/libgobject-2.0.so
LOCAL_SHARED_LIBRARIES 	:= glib libffi

LOCAL_EXPORT_CFLAGS := 				\
	-I$(LOCAL_PATH)/include/glib-2.0	\
	$(NULL)

include $(PREBUILT_SHARED_LIBRARY)

# gmodule
include $(CLEAR_VARS)

LOCAL_MODULE    		:= gmodule
LOCAL_SRC_FILES 		:= lib/libgmodule-2.0.so
LOCAL_SHARED_LIBRARIES 	:= glib
LOCAL_LDLIBS			:= -ldl

LOCAL_EXPORT_CFLAGS := 				\
	-I$(LOCAL_PATH)/include/glib-2.0	\
	$(NULL)

include $(PREBUILT_SHARED_LIBRARY)

# gthread
include $(CLEAR_VARS)

LOCAL_MODULE    		:= gthread
LOCAL_SRC_FILES 		:= lib/libgthread-2.0.so
LOCAL_SHARED_LIBRARIES 	:= glib

LOCAL_EXPORT_CFLAGS := 				\
	-I$(LOCAL_PATH)/include/glib-2.0	\
	$(NULL)

include $(PREBUILT_SHARED_LIBRARY)

# gio
include $(CLEAR_VARS)

LOCAL_MODULE 			:= gio
LOCAL_SRC_FILES 		:= lib/libgio-2.0.so
LOCAL_SHARED_LIBRARIES 	:= glib gobject
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/glib-2.0

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, libiconv)
$(call import-module, gettext)
$(call import-module, libffi)
