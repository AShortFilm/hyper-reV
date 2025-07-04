@echo off
setlocal

mountvol Z: /S

set boot_directory=Z:\EFI\Microsoft\Boot\

attrib -s %boot_directory%bootmgfw.efi
move %boot_directory%bootmgfw.efi %boot_directory%bootmgfw.original.efi

copy /Y %~dp0bootmgfw.efi %boot_directory%
copy /Y %~dp0hyperv-attachment.dll %boot_directory%

bcdedit /set hypervisorlaunchtype auto

endlocal

echo hyper-reV will load at next boot
pause