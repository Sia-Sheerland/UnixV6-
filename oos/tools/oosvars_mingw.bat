@set oos_path=C:\Users\Sia-Sheerland\Desktop\OS_practice\UNIX\oos
@set mingw_path=C:\Users\Sia-Sheerland\Desktop\OS_practice\UNIX\MinGW\bin
@set nasm_path=C:\Users\Sia-Sheerland\Desktop\OS_practice\UNIX\NASM
@set bochs_path=C:\Users\Sia-Sheerland\Desktop\OS_practice\UNIX\Bochs-2.6
@set BXSHARE=%bochs_path%
@set partcopy_path=%oos_path%\tools\partcopy

@set path=%partcopy_path%;%bochs_path%;%nasm_path%;%mingw_path%;%oos_path%;%path%

@cls
@echo Setting develop and build environment for UnixV6++.