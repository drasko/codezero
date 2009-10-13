#! /usr/bin/env python2.6
# -*- mode: python; coding: utf-8; -*-
#
#  Codezero -- a microkernel for embedded systems.
#
#  Copyright © 2009  B Labs Ltd
#
import os, sys, shelve
from os.path import join

PROJRELROOT = '../../'

SCRIPTROOT = os.path.abspath(os.path.dirname("."))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), PROJRELROOT)))

from config.projpaths import *
from config.configuration import *

LINUX_KERNEL_BUILDDIR = join(BUILDDIR, os.path.relpath(LINUX_KERNELDIR, PROJROOT))

# Create linux kernel build directory path as:
# conts/linux -> build/cont[0-9]/linux
def source_to_builddir(srcdir, id):
    cont_builddir = \
        os.path.relpath(srcdir, \
                        PROJROOT).replace("conts", \
                                          "cont" + str(id))
    return join(BUILDDIR, cont_builddir)

class LinuxBuilder:

    def __init__(self, pathdict, container):
        self.LINUX_KERNELDIR = pathdict["LINUX_KERNELDIR"]

        # Calculate linux kernel build directory
        self.LINUX_KERNEL_BUILDDIR = \
            source_to_builddir(LINUX_KERNELDIR, container.id)

        self.linux_lds_in = join(self.LINUX_KERNELDIR, "linux.lds.in")
        self.linux_lds_out = join(self.LINUX_KERNEL_BUILDDIR, "linux.lds")
        self.linux_S_in = join(self.LINUX_KERNELDIR, "linux.S.in")
        self.linux_S_out = join(self.LINUX_KERNEL_BUILDDIR, "linux.S")
        self.linux_elf_out = join(self.LINUX_KERNEL_BUILDDIR, "linux.elf")

        self.kernel_binary_image = \
            join(os.path.relpath(self.LINUX_KERNEL_BUILDDIR, LINUX_KERNELDIR), \
                 "arch/arm/boot/Image")
        self.kernel_image = None

    def build_linux(self):
        print '\nBuilding the linux kernel...'
        os.chdir(self.LINUX_KERNELDIR)
        if not os.path.exists(self.LINUX_KERNEL_BUILDDIR):
            os.makedirs(self.LINUX_KERNEL_BUILDDIR)
        os.system("make defconfig ARCH=arm O=" + self.LINUX_KERNEL_BUILDDIR)
        os.system("make ARCH=arm " + \
                  "CROSS_COMPILE=arm-none-linux-gnueabi- O=" + \
                  self.LINUX_KERNEL_BUILDDIR)

        with open(self.linux_S_in, 'r') as input:
            with open(self.linux_S_out, 'w+') as output:
                content = input.read() % self.kernel_binary_image
                output.write(content)

        os.system("arm-none-linux-gnueabi-cpp -P " + \
                  "%s > %s" % (self.linux_lds_in, self.linux_lds_out))
        os.system("arm-none-linux-gnueabi-gcc -nostdlib -o %s -T%s %s" % \
                  (self.linux_elf_out, self.linux_lds_out, self.linux_S_out))

        # Get the kernel image path
        self.kernel_image = self.linux_elf_out

        print 'Done...'

    def clean(self):
        print 'Cleaning linux kernel build...'
        if os.path.exists(self.LINUX_KERNEL_BUILDDIR):
            shutil.rmtree(self.LINUX_KERNEL_BUILDDIR)
        print 'Done...'

if __name__ == "__main__":
    # This is only a default test case
    container = Container()
    container.id = 0
    linux_builder = LinuxBuilder(projpaths, container)

    if len(sys.argv) == 1:
        linux_builder.build_linux()
    elif "clean" == sys.argv[1]:
        linux_builder.clean()
    else:
        print " Usage: %s [clean]" % (sys.argv[0])
