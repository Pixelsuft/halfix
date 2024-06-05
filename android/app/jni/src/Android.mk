LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
SDL_TTF_PATH := ../SDL_ttf

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/$(SDL_TTF_PATH)/include $(LOCAL_PATH)/../../../../include

# Add your application source files here...
LOCAL_SRC_FILES := main.c pc.c util.c state.c io.c drive.c ini.c host/net-none.c cpu/access.c cpu/trace.c cpu/seg.c cpu/cpu.c cpu/mmu.c cpu/ops/ctrlflow.c cpu/smc.c cpu/decoder.c cpu/eflags.c cpu/prot.c cpu/opcodes.c cpu/ops/arith.c cpu/ops/io.c cpu/ops/string.c cpu/ops/stack.c cpu/ops/misc.c cpu/ops/bit.c cpu/softfloat.c cpu/fpu.c cpu/ops/simd.c hardware/dma.c hardware/cmos.c hardware/pit.c hardware/pic.c hardware/kbd.c hardware/vga.c hardware/ide.c hardware/pci.c hardware/apic.c hardware/ioapic.c hardware/fdc.c hardware/acpi.c display-sdl2.c ui-mobile.c

LOCAL_CFLAGS := -DSDL2_BUILD -DMOBILE_BUILD

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
