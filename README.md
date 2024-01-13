Ufasoft Multi-currency Bitcoin Client and Miner

Web:
  http://ufasoft.com/coin

Forums:
	http://forum.ufasoft.com/forum/index.php?board=6.0
	https://bitcointalk.org/index.php?topic=3486.0
	https://bitcointalk.org/index.php?topic=58821.0



Miner Manual
=======================

coin-miner {-options}
  Options:
    -a scrypt|sha256|prime|<seconds>   hashing algorithm (scrypt, prime or sha256), or time between getwork requests 1..60, default 15
    -A user-agent       Set custom User-agent string in HTTP header, default: Ufasoft bitcoin miner
    -g yes|no           set 'no' to disable GPU, default 'yes'
    -h                  this help
    -i index|name       select device from Device List, can be used multiple times, default - all devices
    -I intensity        Intensity of GPU usage [-10..10], default 0
    -l yes|no           set \'no\' to disable Long-Polling, default \'yes\'\n"
    -o url              in form (http | stratum+tcp | xpt+tcp)://username:password@server.tld:port/path, by default http://127.0.0.1:8332
    -t threads          Number of threads for CPU mining, 0..256, by default is number of CPUs (Cores), 0 - disable CPU mining
    -T temperature      max temperature in Celsius degrees, default: 80
    -v                  Verbose output
    -x type=host:port   Use HTTP or SOCKS proxy. Examples: -x http=127.0.0.1:3128, -x socks=127.0.0.1:1080




Windows build
Instruction for Visual Studio 2017..2019
===========================================
1. Download 3rd-party Libraries:

	OpenSSL			http://www.openssl.org/source/openssl-1.0.1e.tar.gz
	MPIR			http://www.mpir.org/mpir-2.6.0.tar.bz2
	SQLite			http://sqlite.org/sqlite-amalgamation-3071502.zip
	Jansson			http://www.digip.org/jansson/releases/jansson-2.4.tar.bz2

	Create .vcxproj files for them and build these LIBs

	For GPU support you need:
		AMD APP SDK									http://developer.amd.com/tools-and-sdks/heterogeneous-computing/amd-accelerated-parallel-processing-app-sdk/downloads/
		AMD Display Library (ADL) SDK				http://developer.amd.com/tools-and-sdks/graphics-development/display-library-adl-sdk/
		CUDA SDK for NVidia cards					https://developer.nvidia.com/cuda-downloads

2.	Add directories with OpenSSL, SQLite header files to the coineng.vcxproj's  INCLUDE list.
3.	Add direcories with built .lib files to the coineng.vcxproj's linker settings
4.	Build coineng.vcxproj. It results to coineng.exe and coineng.tlb files.
5.	coineng.tlb is required to build WPF UI coin.exe.
6. UBUNTU Project (deprecated)

To run the entire application all .exe/.dll files of used libs should be in the same directory.




========================
This software is free.
Bitcoin address for donations: 18X598V8rVdjy3Yg1cjZmnnv4SpPthuBeT
Donating will help to improve this project.
