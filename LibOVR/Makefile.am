include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(srcdir)/Include \
	-I$(srcdir)/Src \
	-I$(top_srcdir)/edid \
	-I$(top_srcdir)/tinyxml

if RIG_DEBUG
AM_CPPFLAGS += -DDEBUG -DOVR_BUILD_DEBUG
endif

noinst_LTLIBRARIES = libOVR.la

libOVR_la_SOURCES = \
	$(top_srcdir)/edid/edid.cpp \
	$(top_srcdir)/tinyxml/tinyxml2.cpp \
	$(srcdir)/Src/Net/OVR_BitStream.cpp \
	$(srcdir)/Src/Net/OVR_Unix_Socket.cpp \
	$(srcdir)/Src/Net/OVR_NetworkPlugin.cpp \
	$(srcdir)/Src/Net/OVR_PacketizedTCPSocket.cpp \
	$(srcdir)/Src/Net/OVR_RPC1.cpp \
	$(srcdir)/Src/Net/OVR_Session.cpp \
	$(srcdir)/Src/Net/OVR_Socket.cpp \
	$(srcdir)/Src/Service/Service_NetClient.cpp \
	$(srcdir)/Src/Service/Service_NetSessionCommon.cpp \
	$(srcdir)/Src/Tracking/Tracking_SensorStateReader.cpp \
	$(srcdir)/Src/Displays/OVR_Display.cpp \
	$(srcdir)/Src/Displays/OVR_Linux_Display.cpp \
	$(srcdir)/Src/Displays/OVR_Linux_SDKWindow.cpp \
	$(srcdir)/Src/CAPI/CAPI_DistortionRenderer.cpp \
	$(srcdir)/Src/CAPI/CAPI_HSWDisplay.cpp \
	$(srcdir)/Src/CAPI/CAPI_FrameTimeManager.cpp \
	$(srcdir)/Src/CAPI/CAPI_HMDRenderState.cpp \
	$(srcdir)/Src/CAPI/CAPI_HMDState.cpp \
	$(srcdir)/Src/CAPI/CAPI_LatencyStatistics.cpp \
	$(srcdir)/Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp \
	$(srcdir)/Src/CAPI/GL/CAPI_GL_HSWDisplay.cpp \
	$(srcdir)/Src/CAPI/GL/CAPI_GL_Util.cpp \
	$(srcdir)/Src/CAPI/GL/CAPI_GLE.cpp \
	$(srcdir)/Src/Kernel/OVR_Alg.cpp \
	$(srcdir)/Src/Kernel/OVR_Allocator.cpp \
	$(srcdir)/Src/Kernel/OVR_Atomic.cpp \
	$(srcdir)/Src/Kernel/OVR_CRC32.cpp \
	$(srcdir)/Src/Kernel/OVR_DebugHelp.cpp \
	$(srcdir)/Src/Kernel/OVR_File.cpp \
	$(srcdir)/Src/Kernel/OVR_FileFILE.cpp \
	$(srcdir)/Src/Kernel/OVR_Lockless.cpp \
	$(srcdir)/Src/Kernel/OVR_Log.cpp \
	$(srcdir)/Src/Kernel/OVR_Math.cpp \
	$(srcdir)/Src/Kernel/OVR_RefCount.cpp \
	$(srcdir)/Src/Kernel/OVR_SharedMemory.cpp \
	$(srcdir)/Src/Kernel/OVR_Std.cpp \
	$(srcdir)/Src/Kernel/OVR_String.cpp \
	$(srcdir)/Src/Kernel/OVR_String_FormatUtil.cpp \
	$(srcdir)/Src/Kernel/OVR_String_PathUtil.cpp \
	$(srcdir)/Src/Kernel/OVR_SysFile.cpp \
	$(srcdir)/Src/Kernel/OVR_System.cpp \
	$(srcdir)/Src/Kernel/OVR_ThreadsPthread.cpp \
	$(srcdir)/Src/Kernel/OVR_ThreadCommandQueue.cpp \
	$(srcdir)/Src/Kernel/OVR_Timer.cpp \
	$(srcdir)/Src/Kernel/OVR_UTF8Util.cpp \
	$(srcdir)/Src/Util/Util_Interface.cpp \
	$(srcdir)/Src/Util/Util_LatencyTest2Reader.cpp \
	$(srcdir)/Src/Util/Util_Render_Stereo.cpp \
	$(srcdir)/Src/Util/Util_SystemInfo.cpp \
	$(srcdir)/Src/Util/Util_SystemGUI.cpp \
	$(srcdir)/Src/OVR_CAPI.cpp \
	$(srcdir)/Src/OVR_SerialFormat.cpp \
	$(srcdir)/Src/OVR_JSON.cpp \
	$(srcdir)/Src/OVR_Profile.cpp \
	$(srcdir)/Src/OVR_Stereo.cpp
