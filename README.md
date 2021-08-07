# mdadm Linear Device
## Basic Functionality
Implement functionalities of mdam (multiple disk and device administration - tool for doing cool tricks with multiple disks utility in Linux) 
- Implement linear device (makes multiple disks appear as a one large disk to the operating
system) 
  - Configure 16 disks of size 64KB as a single 1MB
  - Functions:
    - ```int mdadm_mount(void)``` -  Mount the linear device; now mdadm user can run read and operations
on the linear address space that combines all disks. Returns 1 on success and -1 on
failure. Calling this function the second time without calling mdadm_unmount in between fails.
    - ```int mdadm_unmount(void)``` - Unmount the linear device; now all commands to the linear
device should fail. Rreturns 1 on success and -1 on failure. Calling this function the second time without calling mdadm_mount in between fails.
    - ```int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)``` - Read len
bytes into buf starting at addr. Read from an out-of-bound linear address fails. A read larger
than 1,024 bytes fails. There are a few more restrictions.
## Writes
Implement write functionality for mdadm
- ```int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)``` - Writes len bytes from the user-supplied buf buffer to storage system, starting at address addr. buf parameter has a const specifier, it is an in parameter; mdadm_write only reads from this parameter and does
not modify it. Similar to mdadm_read, writing to an out-of-bound linear address fails. A read larger than 1,024 bytes fails. There are a few more restrictions.
## Caching 
Adding a cache to mdadm system significantly improves its latency and reduced the load on
the JBOD.
Implement a block cache for mdadm. In the case of mdadm, the key is
tuple consisting of disk number and block number that identifies a specific block in JBOD, and the value is contents of the block. When the users of mdadm system issue mdadm_read call, implementation
of mdadm_read will first look if the block corresponding to the address specified by the user is in the cache, and if it is, then the block will be copied from the cache without issuing a slow JBOD_READ_BLOCK call to JBOD. If the block is not in the cache, then is is read from JBOD and insert it to the cache, so that if a user asks for the block again, it can be served faster from the cache.
- Functions:
  - ```int cache_create(int num_entries);``` - Dynamically allocate space for num_entries cache
entries and stores the address in the cache global variable. Sets cache_size to
num_entries, since that describes the size of the cache and will also be used by other functions.
Calling this function twice without an intervening cache_destroy call fails. The
num_entries argument can be 2 at minimum and 4096 at maximum.
  - ```int cache_destroy(void);``` - Free the dynamically allocated space for cache, and sets cache
to NULL, and cache_size to zero. Calling this function twice without an intervening cache_-
create call fails.
  - ```int cache_lookup(int disk_num, int block_num, uint8_t *buf);``` - Lookup the block
identified by disk_num and block_num in the cache. If found, copies the block into buf, which cannot
be NULL. This function increments num_queries global variable every time it performs a
lookup. If the lookup is successful, this function also increments num_hits global variable; it also increments clock variable and assign it to the access_time field of the corresponding
entry, to indicate that the entry was used recently. num_queries and num_hits
variables are used to compute cache’s hit ratio.
  - ```int cache_insert(int disk_num, int block_num, uint8_t *buf);``` - Insert the block
identified by disk_num and block_num into the cache and copy buf—which cannot be NULL—to
the corresponding cache entry. Insertion never fails: if the cache is full, then an entry is
overwritten according to the LRU policy using data from this insert operation. This function also increments and assigns clock variable to the access_time of the newly inserted entry.
  - ```void cache_update(int disk_num, int block_num, const uint8_t *buf);``` - If the
entry exists in cache, updates its block content with the new data in buf. Also updates the
access_time if successful
  - ```bool cache_enabled(void);``` - Returns true if cache is enabled. Used to integrate
the cache to mdadm_read and mdadm_write functions
## Networking
Implement a client component of this protocol that will connect to the JBOD server and execute JBOD operations over the network. Having networking support in mdadm allows avoidance of downtime in case a JBOD system malfunctions, by switching to another JBOD system on the fly. In new implementation, all calls to jbod_operation are replaced with jbod_client_operation, which will send JBOD commands over a network to a JBOD server that can be anywhere on the Internet. Also implement several support functions that will take care of connecting/disconnecting to/from the JBOD server
- Functions
  - ```jbod_connect``` - connects to JBOD SERVER at port JBOD PORT, both defined in net.h
  - ```jbod_disconnect``` - closes the connection to the JBOD server
