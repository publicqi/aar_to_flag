gcc exp.c -o extracted/exp -static
cd extracted
find . | cpio -H newc -ov -F ../rootfs.cpio 2> /dev/null
cd ..

