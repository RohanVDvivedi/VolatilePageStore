# VolatilePageStore

This project provides a temporary-file backed storage of pages. It is compatible with page_access_method interface of TupleIndexer. The data store provides no logging and no persistence and obviously no-ACID guarantees. The store is volatile even if backed by disk, because it only serves to provide an extended 64-bit address space even on 32 bit systems.

There is no locking/latching involved in this project except for its internal functioning (so, you can have 2 threads acquiring the same page, in a thread-safe manner, yet you can have race-conditions over over-writing it and it would be your concern NOT OF THE VolatilePageStore). The pages are accessed directly by mmap-ing them upon requests and then caching their mapping to avoid excessive mmap syscalls (achieved great performance improvement after implemening this cache). It is you who needs to ensure that upon coupling this project with TupleIndexer, that you will have to add datastructure (bplus_tree and hash_table) level locks.

This project is a sibling project of [MinTXEngine](https://github.com/RohanVDvivedi/MinTXEngine), It serves to provide volatile data storage for the database storage engine built on top of MinTxEngine. It should primarily be used for storing volatile database lock-tables and intermediate query results, and all other stuff that do not need to be persistent like views.

Using this project you will never experience aborts (no WAL-no atomicity/no durability), you will only have crashes upon failures, as soon as mmap/malloc or ftruncate fails, this is why you need MinTxEngine, to make things ACID.

## Setup instructions
**Install dependencies :**
 * [BlockIO](https://github.com/RohanVDvivedi/BlockIO)
 * [SerializableInteger](https://github.com/RohanVDvivedi/SerializableInteger)
 * [BoomPar](https://github.com/RohanVDvivedi/BoomPar)
 * [Cutlery](https://github.com/RohanVDvivedi/Cutlery)

**Download source code :**
 * `git clone https://github.com/RohanVDvivedi/VolatilePageStore.git`

**Build from source :**
 * `cd VolatilePageStore`
 * `make clean all`

**Install from the build :**
 * `sudo make install`
 * ***Once you have installed from source, you may discard the build by*** `make clean`

## Using The library
 * add `-lvolatilepagestore -lblockio -lboompar -lcutlery -lpthread` linker flag, while compiling your application
 * do not forget to include appropriate public api headers as and when needed. this includes
   * `#include<volatile_page_store.h>`

## Instructions for uninstalling library

**Uninstall :**
 * `cd VolatilePageStore`
 * `sudo make uninstall`
