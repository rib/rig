include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(srcdir)/Include \
	-I$(srcdir)/Src \
	-I$(srcdir)/edid \
	-I$(srcdir)/tinyxml

if RIG_DEBUG
AM_CPPFLAGS += -DDEBUG -DOVR_BUILD_DEBUG
endif

noinst_LTLIBRARIES = libOVR.la

libOVR_la_SOURCES = \
	edid/edid.cpp \
	tinyxml/tinyxml2.cpp \
	Src/Net/OVR_BitStream.cpp \
	Src/Net/OVR_Unix_Socket.cpp \
	Src/Net/OVR_NetworkPlugin.cpp \
	Src/Net/OVR_PacketizedTCPSocket.cpp \
	Src/Net/OVR_RPC1.cpp \
	Src/Net/OVR_Session.cpp \
	Src/Net/OVR_Socket.cpp \
	Src/Service/Service_NetClient.cpp \
	Src/Service/Service_NetSessionCommon.cpp \
	Src/Tracking/Tracking_SensorStateReader.cpp \
	Src/Displays/OVR_Display.cpp \
	Src/Displays/OVR_Linux_Display.cpp \
	Src/Displays/OVR_Linux_SDKWindow.cpp \
	Src/CAPI/CAPI_DistortionRenderer.cpp \
	Src/CAPI/CAPI_HSWDisplay.cpp \
	Src/CAPI/CAPI_FrameTimeManager.cpp \
	Src/CAPI/CAPI_HMDRenderState.cpp \
	Src/CAPI/CAPI_HMDState.cpp \
	Src/CAPI/CAPI_LatencyStatistics.cpp \
	Src/Kernel/OVR_Alg.cpp \
	Src/Kernel/OVR_Allocator.cpp \
	Src/Kernel/OVR_Atomic.cpp \
	Src/Kernel/OVR_CRC32.cpp \
	Src/Kernel/OVR_DebugHelp.cpp \
	Src/Kernel/OVR_File.cpp \
	Src/Kernel/OVR_FileFILE.cpp \
	Src/Kernel/OVR_Lockless.cpp \
	Src/Kernel/OVR_Log.cpp \
	Src/Kernel/OVR_Math.cpp \
	Src/Kernel/OVR_RefCount.cpp \
	Src/Kernel/OVR_SharedMemory.cpp \
	Src/Kernel/OVR_Std.cpp \
	Src/Kernel/OVR_String.cpp \
	Src/Kernel/OVR_String_FormatUtil.cpp \
	Src/Kernel/OVR_String_PathUtil.cpp \
	Src/Kernel/OVR_SysFile.cpp \
	Src/Kernel/OVR_System.cpp \
	Src/Kernel/OVR_ThreadsPthread.cpp \
	Src/Kernel/OVR_ThreadCommandQueue.cpp \
	Src/Kernel/OVR_Timer.cpp \
	Src/Kernel/OVR_UTF8Util.cpp \
	Src/Util/Util_Interface.cpp \
	Src/Util/Util_LatencyTest2Reader.cpp \
	Src/Util/Util_Render_Stereo.cpp \
	Src/Util/Util_SystemInfo.cpp \
	Src/Util/Util_SystemGUI.cpp \
	Src/OVR_CAPI.cpp \
	Src/OVR_SerialFormat.cpp \
	Src/OVR_JSON.cpp \
	Src/OVR_Profile.cpp \
	Src/OVR_Stereo.cpp

gl_specific_source = \
	Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp \
	Src/CAPI/GL/CAPI_GL_HSWDisplay.cpp \
	Src/CAPI/GL/CAPI_GL_Util.cpp \
	Src/CAPI/GL/CAPI_GLE.cpp
