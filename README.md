
You can configure features and modules in the generated binary by menuconfig.

Once you apply one of those default configuration by "make xxx_defconfig", then you can adjust or modify each of parameters
by "make menuconfig".

To check build error, you can run "check_build.sh" with a list of configurations to build upon, or run "check_build_all.sh" to build all supported configurations.

Examples)
./check_build.sh sandbox_defconfig
./check_build.sh scm1010_defconfig scm2010_fpga_p1_defconfig
./check_build_all.sh

