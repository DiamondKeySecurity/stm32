Profiling the Cryptech Alpha
============================

Origin
------

This code was copied from https://github.com/ErichStyger/mcuoneclipse.git,
directory Examples/KDS/FRDM-K64F120M/FRDM-K64F_Profiling/Profiling, commit
9b7eedddd8b24968128582aedc63be95b61f782c, dated Mon Jan 9 16:56:17 2017 +0100.

References
----------

I recommend reading both of these to understand how the profiling code works.

[1]: https://mcuoneclipse.com/2015/08/23/tutorial-using-gnu-profiling-gprof-with-arm-cortex-m/
"Tutorial: Using GNU Profiling (gprof) with ARM Cortex-M"

[2]: http://bgamari.github.io/posts/2014-10-31-semihosting.html
"Semihosting with ARM, GCC, and OpenOCD"

How to build
------------

From the top level, run

    make DO_PROFILING=1 hsm

By default, all code is profiled, *except* the profiling code itself,
because that would cause fatal recursion.

How to run
----------

You need to start OpenOCD on the host, and enable semihosting, at least
before you try to use it as a remote file system.

I recommend executing the following in the projects/hsm directory, so that
gmon.out ends up in the same directory as hsm.elf.

Start OpenOCD:

    $ openocd -f /usr/share/openocd/scripts/board/stm32f4discovery.cfg &

Connect to OpenOCD:

    $ telnet localhost 4444

In the OpenOCD console, enable semihosting:

    > arm semihosting enable

In the CLI, type `profile start`, then start the unit test or whatever
will be exercising the hsm. Afterwards, in the CLI, type `profile stop`.

After invoking `profile stop`, it takes almost 2 minutes to write gmon.out
over OpenOCD to the host.

In the projects/hsm directory, run gprof to analyse the gmon.out file:

    $ gprof hsm.elf >gprof.txt
