#!/bin/bash

# uboot版本
UBOOT_VERSION="v2023.10"
RISCV64_UBOOT_PATH="arch/riscv64/u-boot-${UBOOT_VERSION}-riscv64"

export ARCH=${ARCH:=riscv64}
echo "ARCH: ${ARCH}"

# 磁盘镜像
DISK_NAME="disk-${ARCH}.img"

QEMU=qemu-system-${ARCH}
QEMU_MEMORY="512M"
QEMU_SMP="2,cores=2,threads=1,sockets=1"
QEMU_ACCELARATE=""
QEMU_DISK_IMAGE="../output/${DISK_NAME}"
QEMU_DRIVE="-drive id=disk,file=${QEMU_DISK_IMAGE},if=none"
QEMU_DEVICES=" -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 "

# 标准的trace events
# qemu_trace_std=cpu_reset,guest_errors
qemu_trace_std=cpu_reset

QEMU_ARGUMENT=""

if [ ${ARCH} == "riscv64" ]; then
    QEMU_MACHINE=" -machine virt "
    QEMU_MONITOR=""
    QEMU_ARGUMENT+=" --nographic "

else
    echo "不支持的架构"
    exit 1
fi
QEMU_ARGUMENT+=" -d ${QEMU_DISK_IMAGE} -m ${QEMU_MEMORY} -smp ${QEMU_SMP} -boot order=d ${QEMU_MONITOR} -d ${qemu_trace_std} "
QEMU_ARGUMENT+=" -s ${QEMU_MACHINE} "
QEMU_ARGUMENT+=" ${QEMU_DEVICES} ${QEMU_DRIVE} "


TMP_LOOP_DEVICE=""

# 安装riscv64的uboot
install_riscv_uboot()
{

    if [ ! -d ${RISCV64_UBOOT_PATH} ]; then
        echo "正在下载u-boot..."
        uboot_tar_name="u-boot-${UBOOT_VERSION}-riscv64.tar.xz"
        
        uboot_parent_path=$(dirname ${RISCV64_UBOOT_PATH}) || (echo "获取riscv u-boot 版本 ${UBOOT_VERSION} 的父目录失败" && exit 1)

        if [ ! -f ${uboot_tar_name} ]; then
            wget https://mirrors.dragonos.org.cn/pub/third_party/u-boot/${uboot_tar_name} || (echo "下载riscv u-boot 版本 ${UBOOT_VERSION} 失败" && exit 1)
        fi
        echo "下载完成"
        echo "正在解压u-boot到 '$uboot_parent_path'..."
        mkdir -p $uboot_parent_path
        tar xvf u-boot-${UBOOT_VERSION}-riscv64.tar.xz -C ${uboot_parent_path} || (echo "解压riscv u-boot 版本 ${UBOOT_VERSION} 失败" && exit 1)
        echo "解压完成"
        rm -rf u-boot-${UBOOT_VERSION}-riscv64.tar.xz
    fi
    echo "riscv u-boot 版本 ${UBOOT_VERSION} 已经安装"
} 

run_qemu()
{
    echo "正在启动qemu..."

    if [ ${ARCH} == "riscv64" ]; then
        QEMU_ARGUMENT+=" -kernel ${RISCV64_UBOOT_PATH}/u-boot.bin "
    fi

    echo "qemu命令: ${QEMU} ${QEMU_ARGUMENT}"
    ${QEMU} ${QEMU_ARGUMENT}
}


format_as_mbr() {
    echo "Formatting as MBR..."
   # 使用fdisk把disk.img的分区表设置为MBR格式(下方的空行请勿删除)
fdisk ${QEMU_DISK_IMAGE} << EOF
o
n




a
w
EOF

}


mount_disk_image(){
    echo "正在挂载磁盘镜像..."
    TMP_LOOP_DEVICE=$(sudo losetup -f --show -P ${QEMU_DISK_IMAGE}) || exit 1

    # 根据函数入参判断是否需要格式化磁盘镜像
    if [ "$1" == "mnt" ]; then
        mkdir -p ../output/mnt
        sudo mount ${TMP_LOOP_DEVICE}p1 ../output/mnt
    fi
    
    echo "挂载磁盘镜像完成"
}

umount_disk_image(){
    echo "正在卸载磁盘镜像..."
    if [ "$1" == "mnt" ]; then
        sudo umount ../output/mnt
    fi
    sudo losetup -d ${TMP_LOOP_DEVICE} || (echo "卸载磁盘镜像失败" && exit 1)
    echo "卸载磁盘镜像完成"
}

prepare_disk_image()
{
    # 如果磁盘镜像不存在，则创建磁盘镜像
    
    echo "正在准备磁盘镜像..."
    if [ ! -f ${QEMU_DISK_IMAGE} ]; then
        echo "正在创建磁盘镜像..."
        qemu-img create -f raw ${QEMU_DISK_IMAGE} 256M || (echo "创建磁盘镜像失败" && exit 1)
        format_as_mbr

        mount_disk_image
        echo "loop device: ${TMP_LOOP_DEVICE}"
        echo "正在格式化磁盘镜像..."
        sudo mkfs.vfat -F 32 ${TMP_LOOP_DEVICE}p1
        umount_disk_image

        echo "Successfully mkfs"

        chmod 777 ${QEMU_DISK_IMAGE}

        echo "创建磁盘镜像完成"
    fi
    echo "磁盘镜像已经准备好"
}


write_disk_image(){
    mkdir -p ../output/sysroot
    echo "正在写入磁盘镜像..."
    mount_disk_image mnt

    mkdir -p ../output/sysroot/efi/boot
    if [ ${ARCH} == "riscv64" ]; then
        cp ../output/dragon_stub-riscv64.efi ../output/sysroot/efi/boot/bootriscv64.efi
    fi

    sudo cp -r ../output/sysroot/* ../output/mnt
    
    umount_disk_image mnt
    echo "写入磁盘镜像完成"

}

main()
{
    install_riscv_uboot
    prepare_disk_image
    write_disk_image
    run_qemu
}

main