include ..\Makefile.inc

TARGET = ..\..\targets\objs

all		:	$(TARGET)\allocator.o $(TARGET)\pagemanager.o $(TARGET)\kernelallocator.o $(TARGET)\new.o \
			$(TARGET)\swappermanager.o $(TARGET)\vmaareaaruct.o $(TARGET)\pagerefcount.o
			
$(TARGET)\allocator.o	:	Allocator.cpp $(INCLUDE)\Allocator.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)\pagemanager.o	:	PageManager.cpp $(INCLUDE)\PageManager.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@
	
$(TARGET)\kernelallocator.o	:	KernelAllocator.cpp $(INCLUDE)\KernelAllocator.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@
	
$(TARGET)\new.o	:	New.cpp $(INCLUDE)\New.h
	$(CC) $(CFLAGS) -fcheck-new -I"$(INCLUDE)" -c $< -o $@

$(TARGET)\swappermanager.o	:	SwapperManager.cpp $(INCLUDE)\SwapperManager.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)\vmaareaaruct.o	:	VmAreaStruct.cpp $(INCLUDE)\VmAreaStruct.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@

$(TARGET)\pagerefcount.o	:	PageRefCount.cpp $(INCLUDE)\PageRefCount.h
	$(CC) $(CFLAGS) -I"$(INCLUDE)" -c $< -o $@
