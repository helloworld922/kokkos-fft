.. _quick_start:

Quickstart guide
================

This section will quickly illustrate how to use KokkosFFT.
First of all, you need to clone this repo. 

.. code-block:: bash

    git clone --recursive https://github.com/CExA-project/kokkos-fft.git

To configure KokkosFFT, we can just use CMake options for Kokkos, which automatically enables the FFT interface on Kokkos device. 
If CMake fails to find a backend FFT library, see :doc:`How to find fft libraries?<../finding_libraries>`.

Requirements
------------

KokkosFFT requieres ``Kokkos 4.2+`` and dedicated compilers for CPUs or GPUs.
It employs ``CMake 3.22+`` for building. 

Here are list of compilers we frequently use for testing. 

* ``gcc 8.3.0+`` - CPUs
* ``IntelLLVM 2023.0.0+`` - CPUs, Intel GPUs
* ``nvcc 12.0.0+`` - NVIDIA GPUs
* ``rocm 5.3.0+`` - AMD GPUs

Building
--------

For the moment, there are two ways to use KokkosFFT: including as a subdirectory in CMake project or installing as a library.
For simplicity, however, we demonstrate an example to use KokkosFFT as a subdirectory in a CMake project. For installation, see :ref:`Building KokkosFFT<building>` for detail.
Since KokkosFFT is a header-only library, it is enough to simply add as a subdirectory. It is assumed that kokkos and kokkosFFT are placed under ``<project_directory>/tpls``.

Here is an example to use KokkosFFT in the following CMake project.

.. code-block:: bash

    ---/
     |
     └──<project_directory>/
        |--tpls
        |    |--kokkos/
        |    └──kokkos-fft/
        |--CMakeLists.txt
        └──hello.cpp

The ``CMakeLists.txt`` would be

.. code-block:: CMake

    cmake_minimum_required(VERSION 3.23)
    project(kokkos-fft-as-subdirectory LANGUAGES CXX)

    add_subdirectory(tpls/kokkos)
    add_subdirectory(tpls/kokkos-fft)

    add_executable(hello-kokkos-fft hello.cpp)
    target_link_libraries(hello-kokkos-fft PUBLIC Kokkos::kokkos KokkosFFT::fft)

For compilation, we basically rely on the CMake options for Kokkos. For example, the configure options for A100 GPU is as follows.

.. code-block:: bash

    mkdir build && cd build
    cmake -DCMAKE_CXX_COMPILER=g++ \
          -DCMAKE_BUILD_TYPE=Release \
          -DKokkos_ENABLE_CUDA=ON \
          -DKokkos_ARCH_AMPERE80=ON ..
    cmake --build . -j 8

This way, all the functionalities are executed on A100 GPUs.

Trying
------

For those who are familiar with `numpy.fft <https://numpy.org/doc/stable/reference/routines.fft.html>`_, 
you may use KokkosFFT quite easily. Here is an example for 1D real to complex transform with rfft in python and KokkosFFT.

.. code-block:: python

   import numpy as np
   x = np.random.rand(4)
   x_hat = np.fft.rfft(x)

.. code-block:: C++

   #include <Kokkos_Core.hpp>
   #include <Kokkos_Complex.hpp>
   #include <Kokkos_Random.hpp>
   #include <KokkosFFT.hpp>
   using execution_space = Kokkos::DefaultExecutionSpace;
   template <typename T> using View1D = Kokkos::View<T*, execution_space>;
   constexpr int n = 4;

   View1D<double> x("x", n);
   View1D<Kokkos::complex<double> > x_hat("x_hat", n/2+1);

   Kokkos::Random_XorShift64_Pool<> random_pool(12345);
   Kokkos::fill_random(x, random_pool, 1);
   Kokkos::fence();

   KokkosFFT::rfft(execution_space(), x, x_hat);

In most cases, a funciton ``numpy.fft.<function_name>`` is available by ``KokkosFFT::<function_name>``.
There are two major differences: ``execution_space`` argument and output value (``x_hat``) is an argument of API (not a returned value from API).
Instead of numpy.array, we rely on `Kokkos Views <https://kokkos.org/kokkos-core-wiki/API/core/View.html>`_.
The accessibilities of Views from ``execution_space`` are statically checked (compilation errors if not accessible). 
It is easiest to rely only on the ``Kokkos::DefaultExecutionSpace`` for both View allocation and KokkosFFT APIs.
See :ref:`Using KokkosFFT<using>` for detail.