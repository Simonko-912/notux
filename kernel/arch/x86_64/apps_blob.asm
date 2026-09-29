;; Notux OS — embedded user-application payload
;; kernel/arch/x86_64/apps_blob.asm
;;
;; Incbin of build/apps_blob.bin produced by tools/make_apps_blob.py.
;; The kernel writes these binaries into #/bin on first boot.

section .rodata
global apps_blob_start
global apps_blob_end

apps_blob_start:
incbin "build/apps_blob.bin"
apps_blob_end: