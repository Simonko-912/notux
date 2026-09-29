/*
 * Notux OS — UEFI Types (offsets verified against UEFI spec 2.10)
 * boot/efi.h
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint64_t UINTN;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint16_t UINT16;
typedef uint8_t  UINT8;
typedef uint64_t EFI_STATUS;
typedef void    *EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;

#define EFI_SUCCESS              0ULL
#define EFI_LOAD_ERROR           (1ULL|(1ULL<<63))
#define EFI_ERROR(s)             ((s)&(1ULL<<63))
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL 2u
#define EFI_FILE_MODE_READ       1ULL
#define EfiLoaderData            2u
#define AllocateAddress          2u
#define AllocateAnyPages         0u

typedef struct {
    UINT32 Data1; UINT16 Data2, Data3; UINT8 Data4[8];
} EFI_GUID;

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}}
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}}
#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}}
#define EFI_FILE_INFO_ID \
    {0x9576e92,0x6d3f,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}}

typedef struct {
    UINT32 Type; UINT32 Pad;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* 24 bytes: Sig(8)+Rev(4)+HdrSz(4)+CRC(4)+Rsvd(4) */
typedef struct {
    UINT64 Sig;
    UINT32 Rev, HdrSz, CRC, Rsvd;
} EFI_TABLE_HEADER;

typedef struct _EFI_FILE {
    UINT64 Revision;
    EFI_STATUS (*Open)   (struct _EFI_FILE*,struct _EFI_FILE**,CHAR16*,UINT64,UINT64);
    EFI_STATUS (*Close)  (struct _EFI_FILE*);
    EFI_STATUS (*Delete) (struct _EFI_FILE*);
    EFI_STATUS (*Read)   (struct _EFI_FILE*,UINTN*,void*);
    EFI_STATUS (*Write)  (struct _EFI_FILE*,UINTN*,void*);
    EFI_STATUS (*GetPos) (struct _EFI_FILE*,UINT64*);
    EFI_STATUS (*SetPos) (struct _EFI_FILE*,UINT64);
    EFI_STATUS (*GetInfo)(struct _EFI_FILE*,EFI_GUID*,UINTN*,void*);
    EFI_STATUS (*SetInfo)(struct _EFI_FILE*,EFI_GUID*,UINTN,void*);
    EFI_STATUS (*Flush)  (struct _EFI_FILE*);
} EFI_FILE_PROTOCOL;

typedef struct {
    UINT64 Size, FileSize, PhysicalSize;
    UINT8  CTime[16], LATime[16], MTime[16];
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

typedef struct _EFI_SFS {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(struct _EFI_SFS*,EFI_FILE_PROTOCOL**);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _EFI_COUT {
    void *Reset;
    EFI_STATUS (*OutputString)(struct _EFI_COUT*,CHAR16*);
    void *TestString,*QueryMode,*SetMode,*SetAttribute;
    EFI_STATUS (*ClearScreen)(struct _EFI_COUT*);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef enum {
    PixelRGBR8=0,PixelBGRR8,PixelBitMask,PixelBlt,PixelMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version, HorizontalResolution, VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT32 RMask, GMask, BMask, XMask; /* pixel bitmask */
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFO;

typedef struct {
    UINT32 MaxMode, Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFO *Info;
    UINTN  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    void *QueryMode, *SetMode, *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT32 Revision; EFI_HANDLE ParentHandle; void *SystemTable;
    EFI_HANDLE DeviceHandle; void *FilePath, *Reserved;
    UINT32 LoadOptionsSize; void *LoadOptions, *ImageBase;
    UINT64 ImageSize; UINT32 ImageCodeType, ImageDataType; void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

/* Offsets verified: AllocPool=64, GetMmap=56, ExitBS=232, OpenProt=280, LocateProt=320 */
typedef struct {
    EFI_TABLE_HEADER Hdr;               /* 0    (24) */
    void *RaiseTPL, *RestoreTPL;        /* 24   (16) */
    EFI_STATUS (*AllocatePages)(UINT32,UINT32,UINTN,EFI_PHYSICAL_ADDRESS*); /* 40 */
    void *FreePages;                    /* 48   (8)  */
    EFI_STATUS (*GetMemoryMap)(UINTN*,EFI_MEMORY_DESCRIPTOR*,UINTN*,UINTN*,UINT32*); /* 56 */
    EFI_STATUS (*AllocatePool)(UINT32,UINTN,void**); /* 64 */
    EFI_STATUS (*FreePool)(void*);      /* 72   (8)  */
    void *CE,*ST2,*WFE,*SE,*ClE,*ChE;  /* 80..120 (48) */
    void *IPI,*RPI,*UPI;               /* 128..144 (24) */
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE,EFI_GUID*,void**); /* 152 */
    void *Rsvd2, *RPN;                  /* 160,168  */
    void *LH, *LDP;                     /* 176,184  */
    void *ICT;                          /* 192      */
    void *LI, *StI, *Ex, *UI;          /* 200,208,216,224 */
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE,UINTN); /* 232 */
    void *GNMC, *Stall, *SWT;          /* 240,248,256 */
    void *CC, *DC;                      /* 264,272  */
    EFI_STATUS (*OpenProtocol)(EFI_HANDLE,EFI_GUID*,void**,EFI_HANDLE,EFI_HANDLE,UINT32); /* 280 */
    void *CP, *OPI;                     /* 288,296  */
    void *PPH, *LHB;                    /* 304,312  */
    EFI_STATUS (*LocateProtocol)(EFI_GUID*,void*,void**); /* 320 */
    void *IMPI, *UMPI;                  /* 328,336  */
    void *CalcCRC, *CopyMem, *SetMem, *CEX; /* 344.. */
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32  FirmwareRevision;
    UINT32  _pad;
    EFI_HANDLE ConInHandle;
    void *ConIn;
    EFI_HANDLE ConOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE ErrHandle;
    void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumConfigEntries;
    void *ConfigTable;
} EFI_SYSTEM_TABLE;
