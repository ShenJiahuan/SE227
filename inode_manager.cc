#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk() {
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf) {
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf) {
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block() {
    /*
     * your code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */
    for (int bitmap_id = BBLOCK(0); bitmap_id <= BBLOCK(BLOCK_NUM - 1); bitmap_id++) {
        char *buf = (char *) malloc(BLOCK_SIZE);
        read_block(bitmap_id, buf);
        for (int i = 0; i < BPB; i++) {
            int index = i / 8;
            int offset = i % 8;
            if (!(buf[index] & (1 << offset))) {
                buf[index] |= (1 << offset);
                write_block(bitmap_id, buf);
                return bitmap_id * BPB + i;
            }
        }
        free(buf);
    }
    return -1;
}

void block_manager::free_block(uint32_t id) {
    /*
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
    int bitmap_id = BBLOCK(id) - BBLOCK(0);
    char *buf = (char *) malloc(BLOCK_SIZE);
    read_block(bitmap_id, buf);
    int index = (id % BPB) / 8;
    int offset = (id % BPB) % 8;
    buf[index] &= ~(1 << offset);
    write_block(bitmap_id, buf);
    free(buf);
    return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() {
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;
}

void block_manager::read_block(uint32_t id, char *buf) {
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf) {
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type) {
    /*
     * your code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     */
    printf("alloc inode, type: %u\n", type);
    static int inum = 0;
    inum++;
    struct inode *ino = (struct inode *) malloc(sizeof(struct inode));
    ino->type = type;
    ino->size = 0;
    ino->ctime = ino->atime = ino->mtime = time(NULL);
    put_inode(inum, ino);
    free(ino);
    return inum;
}

void inode_manager::free_inode(uint32_t inum) {
    /*
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */

    struct inode *prev_ino = get_inode(inum);
    if (prev_ino == NULL) {
        return;
    } else {
        free(prev_ino);
    }

    char buf[BLOCK_SIZE];
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    struct inode *ino_disk = (struct inode *) buf + inum % IPB;
    struct inode *ino = (struct inode *) malloc(sizeof(struct inode));
    memset(ino, 0, sizeof(struct inode));
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
    return;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum) {
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode *) buf + inum % IPB;
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode *) malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size) {
    /*
     * your code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_Out
     */
    struct inode *ino = get_inode(inum);
    printf("ino size: %d\n", ino->size);
    *size = ino->size;
    // making buffer a little more than needed
    *buf_out = (char *) malloc((*size / BLOCK_SIZE + 1) * BLOCK_SIZE);

    int read_size = 0;
    for (int i = 0; read_size < *size && i < NDIRECT; i++, read_size += BLOCK_SIZE) {
        bm->read_block(ino->blocks[i], *buf_out + read_size);
    }
    if (read_size < *size) {
        // read from the indirect block
        int id = ino->blocks[NDIRECT];
        blockid_t *blockidBuf = (blockid_t *) malloc(BLOCK_SIZE);
        bm->read_block(id, (char *) blockidBuf);
        for (int i = 0; read_size < *size; i++, read_size += BLOCK_SIZE) {
            bm->read_block(blockidBuf[i], *buf_out + read_size);
        }
    }

    ino->atime = time(NULL);
    put_inode(inum, ino);
    free(ino);
    return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size) {
    /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode
     */
    struct inode *ino = get_inode(inum);

    int free_size = 0;
    for (int i = 0; free_size < ino->size && i < NDIRECT; i++, free_size += BLOCK_SIZE) {
        bm->free_block(ino->blocks[i]);
    }
    if (free_size < ino->size) {
        // read from the indirect block
        int id = ino->blocks[NDIRECT];
        blockid_t *blockidBuf = (blockid_t *) malloc(BLOCK_SIZE);
        bm->read_block(id, (char *) blockidBuf);
        for (int i = 0; free_size < ino->size; i++, free_size += BLOCK_SIZE) {
            bm->free_block(blockidBuf[i]);
        }
        bm->free_block(id);
    }

    int written_size = 0;
    for (int i = 0; written_size < size && i < NDIRECT; i++, written_size += BLOCK_SIZE) {
        blockid_t id = bm->alloc_block();
        ino->blocks[i] = id;
        bm->write_block(id, buf + written_size);
    }
    if (written_size < size) {
        blockid_t id = bm->alloc_block();
        ino->blocks[NDIRECT] = id;
        blockid_t *blockidBuf = (blockid_t *) malloc(BLOCK_SIZE);
        for (int i = 0; written_size < size; i++, written_size += BLOCK_SIZE) {
            blockid_t indirectId = bm->alloc_block();
            blockidBuf[i] = indirectId;
            bm->write_block(indirectId, buf + written_size);
        }
        bm->write_block(id, (const char *) blockidBuf);
        free(blockidBuf);
    }
    ino->size = size;
    ino->mtime = ino->ctime = time(NULL);
    put_inode(inum, ino);

    free(ino);

    return;
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a) {
    /*
     * your code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */

    struct inode *ino = get_inode(inum);
    if (ino != NULL) {
        a.type = ino->type;
        a.atime = ino->atime;
        a.ctime = ino->ctime;
        a.mtime = ino->mtime;
        a.size = ino->size;
        free(ino);
    }
    printf("getattr finished\n");

    return;
}

void inode_manager::remove_file(uint32_t inum) {
    /*
     * your code goes here
     * note: you need to consider about both the data block and inode of the file
     */

    struct inode *ino = get_inode(inum);
    int size = ino->size;

    // phase 1: free blocks
    int read_size = 0;
    for (int i = 0; read_size < size && i < NDIRECT; i++, read_size += BLOCK_SIZE) {
        bm->free_block(ino->blocks[i]);
    }
    if (read_size < size) {
        // delete from the indirect block
        int id = ino->blocks[NDIRECT];
        blockid_t *blockidBuf = (blockid_t *) malloc(BLOCK_SIZE);
        bm->read_block(id, (char *) blockidBuf);
        for (int i = 0; read_size < size; i++, read_size += BLOCK_SIZE) {
            bm->free_block(blockidBuf[i]);
        }
        bm->free_block(id);
    }

    // phase 2: free inode
    free_inode(inum);

    free(ino);

    return;
}
