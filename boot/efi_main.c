/*
 * Notux OS — UEFI Bootloader
 * boot/efi_main.c
 *
 * Compile: clang -target x86_64-unknown-windows -ffreestanding
 *           -fno-stack-protector -mno-red-zone -fshort-wchar -Iboot -O2
 * Link:    lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib
 *           /machine:x64 /dll
 */
#include "efi.h"
#include "boot_info.h"

/* ── Serial (COM1 115200 8N1) ─────────────────────────────── */
static inline void outb_p(uint16_t p,uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t inb_p(uint16_t p){
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static void com_init(void){
    outb_p(0x3F9,0x00); outb_p(0x3FB,0x80);
    outb_p(0x3F8,0x01); outb_p(0x3F9,0x00);
    outb_p(0x3FB,0x03); outb_p(0x3FA,0xC7);
}
static void com_putc(char c){
    if(c=='\n')com_putc('\r');
    while(!(inb_p(0x3FD)&0x20));
    outb_p(0x3F8,(uint8_t)c);
}
static void com_puts(const char *s){while(*s)com_putc(*s++);}
static void com_hex(uint64_t v){
    com_puts("0x");
    for(int i=60;i>=0;i-=4){
        int n=(int)((v>>i)&0xF);
        com_putc((char)(n<10?'0'+n:'A'+n-10));
    }
}

/* ── EFI globals ──────────────────────────────────────────── */
static EFI_SYSTEM_TABLE  *ST;
static EFI_BOOT_SERVICES *BS;

/* ── GUIDs — MUST be static const (clang windows ABI fix) ── */
static const EFI_GUID guid_gop  = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static const EFI_GUID guid_li   = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static const EFI_GUID guid_sfs  = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static const EFI_GUID guid_fi   = EFI_FILE_INFO_ID;

/* ── ELF64 ────────────────────────────────────────────────── */
#define ELF_MAGIC 0x464C457Fu
#define PT_LOAD   1u
#define EM_X64    62u

typedef struct __attribute__((packed)){
    uint32_t mag; uint8_t cls,dat,ver,abi,pad[8];
    uint16_t type,mach; uint32_t ver2;
    uint64_t entry,phoff,shoff; uint32_t flags;
    uint16_t ehsz,phesz,phnum,shesz,shnum,shstr;
} Ehdr;
typedef struct __attribute__((packed)){
    uint32_t type,flags;
    uint64_t off,vaddr,paddr,filesz,memsz,align;
} Phdr;

static void xmemset(void *d,uint8_t v,uint64_t n){
    uint8_t*p=(uint8_t*)d; while(n--)*p++=v;
}
static void xmemcpy(void *d,const void *s,uint64_t n){
    uint8_t*p=(uint8_t*)d; const uint8_t*q=(const uint8_t*)s; while(n--)*p++=*q++;
}

/* ── Framebuffer ──────────────────────────────────────────── */
static void get_fb(FramebufferInfo *fb){
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=0;
    EFI_STATUS s=BS->LocateProtocol(
        (EFI_GUID*)&guid_gop, 0, (void**)&gop);
    if(s||!gop){ com_puts("No GOP\n"); return; }
    fb->base  =(uint64_t)gop->Mode->FrameBufferBase;
    fb->size  =(uint64_t)gop->Mode->FrameBufferSize;
    fb->width =gop->Mode->Info->HorizontalResolution;
    fb->height=gop->Mode->Info->VerticalResolution;
    fb->pitch =gop->Mode->Info->PixelsPerScanLine*4;
    fb->format=(uint32_t)gop->Mode->Info->PixelFormat;
    com_puts("FB: "); com_hex(fb->base);
    com_puts(" "); com_hex(fb->width);
    com_puts("x"); com_hex(fb->height); com_puts("\n");
}

/* ── Read file from ESP ───────────────────────────────────── */
static EFI_STATUS read_file(EFI_HANDLE img, const CHAR16 *name,
                             void **buf_out, UINTN *sz_out){
    EFI_LOADED_IMAGE_PROTOCOL *li=0;
    BS->OpenProtocol(img,(EFI_GUID*)&guid_li,(void**)&li,
                     img,0,EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs=0;
    BS->OpenProtocol(li->DeviceHandle,(EFI_GUID*)&guid_sfs,(void**)&sfs,
                     img,0,EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    EFI_FILE_PROTOCOL *root=0;
    sfs->OpenVolume(sfs,&root);

    EFI_FILE_PROTOCOL *f=0;
    EFI_STATUS s=root->Open(root,&f,(CHAR16*)name,EFI_FILE_MODE_READ,0);
    root->Close(root);
    if(s){ com_puts("open failed: "); com_hex(s); com_puts("\n"); return s; }

    /* Get size */
    UINTN isz = sizeof(EFI_FILE_INFO)+256;
    EFI_FILE_INFO *fi=0;
    BS->AllocatePool(EfiLoaderData,isz,(void**)&fi);
    f->GetInfo(f,(EFI_GUID*)&guid_fi,&isz,fi);
    UINTN fsz=(UINTN)fi->FileSize;
    BS->FreePool(fi);
    com_puts("file size: "); com_hex(fsz); com_puts("\n");

    void *buf=0;
    BS->AllocatePool(EfiLoaderData,fsz,&buf);
    f->Read(f,&fsz,buf);
    f->Close(f);
    *buf_out=buf; *sz_out=fsz;
    return EFI_SUCCESS;
}

/* ── Load ELF into memory ─────────────────────────────────── */
static EFI_STATUS load_elf(void *data, uint64_t *entry_out, BootInfo *bi){
    Ehdr *eh=(Ehdr*)data;
    if(eh->mag!=ELF_MAGIC||eh->cls!=2||eh->mach!=EM_X64){
        com_puts("bad ELF\n"); return EFI_LOAD_ERROR;
    }
    com_puts("ELF entry="); com_hex(eh->entry); com_puts("\n");

    uint64_t lo=~0ULL, hi=0;
    uint8_t *b=(uint8_t*)data;

    for(uint16_t i=0;i<eh->phnum;i++){
        Phdr *ph=(Phdr*)(b + eh->phoff + (uint64_t)i*eh->phesz);
        if(ph->type!=PT_LOAD||!ph->memsz) continue;

        uint64_t ps = ph->paddr & ~0xFFFULL;
        uint64_t pe = (ph->paddr+ph->memsz+0xFFFULL) & ~0xFFFULL;
        UINTN pages = (UINTN)((pe-ps)/4096);
        com_puts("  PT_LOAD "); com_hex(ph->paddr);
        com_puts(" pages="); com_hex(pages); com_puts("\n");

        EFI_PHYSICAL_ADDRESS pa=(EFI_PHYSICAL_ADDRESS)ps;
        EFI_STATUS s=BS->AllocatePages(AllocateAddress,EfiLoaderData,pages,&pa);
        if(s){
            com_puts("  AllocAddr failed="); com_hex(s);
            com_puts(", trying AnyPages\n");
            pa=0;
            s=BS->AllocatePages(AllocateAnyPages,EfiLoaderData,pages,&pa);
            if(s){ com_puts("  FATAL: no memory\n"); return s; }
            com_puts("  placed at "); com_hex((uint64_t)pa); com_puts("\n");
        }
        xmemset((void*)(uintptr_t)pa, 0, pages*4096);
        xmemcpy((void*)(uintptr_t)(pa+(ph->paddr-ps)),
                b+ph->off, (uint64_t)ph->filesz);
        if(ps<lo) lo=ps;
        if(pe>hi) hi=pe;
    }
    bi->kernel_phys=lo; bi->kernel_virt=lo; bi->kernel_size=hi-lo;
    *entry_out=eh->entry;
    com_puts("loaded kernel phys="); com_hex(lo); com_puts("\n");
    return EFI_SUCCESS;
}

/* ── Memory map ───────────────────────────────────────────── */
static EFI_STATUS get_mmap(MemoryMapInfo *m){
    UINTN sz=0,key=0,dsz=0; UINT32 dver=0;
    BS->GetMemoryMap(&sz,0,&key,&dsz,&dver);
    sz+=4*dsz;
    EFI_MEMORY_DESCRIPTOR *map=0;
    EFI_STATUS s=BS->AllocatePool(EfiLoaderData,sz,(void**)&map);
    if(s)return s;
    s=BS->GetMemoryMap(&sz,(void*)map,&key,&dsz,&dver);
    if(s)return s;
    m->map=map; m->map_size=sz; m->map_key=key;
    m->desc_size=dsz; m->desc_ver=dver;
    return EFI_SUCCESS;
}

/* ── Entry point ──────────────────────────────────────────── */
EFI_STATUS efi_main(EFI_HANDLE Img, EFI_SYSTEM_TABLE *Sys){
    ST=Sys; BS=Sys->BootServices;
    com_init();
    com_puts("\n=== NOTUX BOOTLOADER ===\n");

    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->OutputString(ST->ConOut,(CHAR16*)L"Notux\r\n");

    static BootInfo bi;
    bi.magic=NOTUX_BOOT_MAGIC;

    /* Framebuffer */
    com_puts("Getting GOP...\n");
    get_fb(&bi.fb);

    /* Load kernel */
    com_puts("Reading notux.elf...\n");
    void *elf=0; UINTN elfsize=0;
    EFI_STATUS s=read_file(Img,(CHAR16*)L"\\notux.elf",&elf,&elfsize);
    if(s){
        ST->ConOut->OutputString(ST->ConOut,
            (CHAR16*)L"ERROR: notux.elf not found!\r\n");
        com_puts("FATAL: kernel file missing\n");
        for(;;)__asm__("hlt");
    }

    /* Parse + map ELF */
    com_puts("Loading ELF segments...\n");
    uint64_t entry=0;
    s=load_elf(elf,&entry,&bi);
    if(s){ com_puts("FATAL: ELF bad\n"); for(;;)__asm__("hlt"); }

    /* Hand the kernel a stack it owns. Allocated as EfiBootServicesData
     * so the kernel's PMM (which recycles only EfiLoaderCode,
     * EfiLoaderData, EfiBootServicesCode and EfiConventionalMemory)
     * will never hand this memory out again. The firmware's own loader
     * stack stays live while we run, so it must not be reused. */
    {
        const UINTN kstack_pages = 16;
        UINTN kstack_pa = 0;
        s = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                              kstack_pages, &kstack_pa);
        if (s) { com_puts("FATAL: kstack alloc\n"); for(;;)__asm__("hlt"); }
        bi.kstack_phys  = kstack_pa;
        bi.kstack_bytes = kstack_pages * 4096;
    }

    /* Exit boot services */
    com_puts("ExitBootServices...\n");
    s=get_mmap(&bi.mmap);
    s=BS->ExitBootServices(Img,bi.mmap.map_key);
    if(s){
        get_mmap(&bi.mmap);
        s=BS->ExitBootServices(Img,bi.mmap.map_key);
        if(s){ com_puts("FATAL: ExitBS\n"); for(;;)__asm__("hlt"); }
    }

    /* After ExitBootServices: serial still works (we own hardware) */
    com_puts("JMP>\n");

    /* Jump to kernel _start(BootInfo*) */
    typedef void (*KE)(BootInfo*) __attribute__((sysv_abi));
    ((KE)entry)(&bi);

    for(;;)__asm__("hlt");
    return EFI_SUCCESS;
}
