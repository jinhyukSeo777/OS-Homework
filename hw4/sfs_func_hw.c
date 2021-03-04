//
// Submission Year : 2020
// OS Homework
// Simple FIle System
// Student Name : Seo jin hyuk
// Student Number : B811088
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );

	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);

	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

int getBlock() {
	int i, j, h;
	int k = SFS_BLOCKSIZE / 4;
	u_int32_t sd[k];

	for(i = 0; i < SFS_BITBLOCKS(spb.sp_nblocks); i++) {
			disk_read(sd, 2+i);

			for(j = 0; j < k; j++) {
				for(h = 0; h < 32; h++) {
					if(!BIT_CHECK(sd[j], h)) {
						BIT_SET(sd[j], h);
						disk_write(sd, 2+i);
						return i * SFS_BLOCKBITS + j * 32 + h;
					}
				}
			}
	}

	return -1;
}

void freeBlock(int n) {
	int i, j, h;
	int k = SFS_BLOCKSIZE / 4;
	u_int32_t sd[k];

	disk_read(sd, 2 + n / SFS_BLOCKBITS);
	i = n % SFS_BLOCKBITS;
	j = i / k;
	h = i % k;

	BIT_CLEAR(sd[j], h);
	disk_write(sd, 2 + n / SFS_BLOCKBITS);
}

void sfs_touch(const char* path) {

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, path, SFS_NAMELEN) == 0) {
					error_message("touch", path, -6);
					return;
				}
			}
		}
	}

	int emptyEntry = -1, emptyIndex = -1;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] == SFS_NOINO) {
			int emptyBlock = getBlock(); //
			if(emptyBlock == -1) {error_message("touch", path, -4); return;}
			si.sfi_direct[i] = emptyBlock;
			bzero(sd,SFS_BLOCKSIZE);
			disk_write(sd, emptyBlock);
		}
		disk_read(sd, si.sfi_direct[i]);

		for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if(sd[j].sfd_ino == SFS_NOINO) {
				emptyEntry = i;
				emptyIndex = j;
				break;
			}
		}
		if(emptyEntry != -1) {break;}
	}
	if(emptyEntry == -1) {error_message("touch", path, -3); return;}

	int emptyBlock;
	emptyBlock = getBlock();
	if(emptyBlock == -1) {error_message("touch", path, -4); return;}

	disk_read(sd, si.sfi_direct[emptyEntry]);
	sd[emptyIndex].sfd_ino = emptyBlock;
	strncpy(sd[emptyIndex].sfd_name, path, SFS_NAMELEN);
	disk_write(sd, si.sfi_direct[emptyEntry]);

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;
	bzero(&newbie,SFS_BLOCKSIZE);
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, emptyBlock);

}

void sfs_cd(const char* path) {

	if(path == NULL) {
		sd_cwd.sfd_ino = SFS_ROOT_LOCATION;
		strncpy(sd_cwd.sfd_name, "/", SFS_NAMELEN);
		return;
	}

	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);
	assert(si.sfi_type == SFS_TYPE_DIR);
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, path, SFS_NAMELEN) == 0) {
					struct sfs_inode inode;
					disk_read(&inode, sd[j].sfd_ino);
					if(inode.sfi_type == SFS_TYPE_FILE) {error_message("cd", path, -2); return;}
					sd_cwd.sfd_ino = sd[j].sfd_ino;
					strncpy(sd_cwd.sfd_name, path, SFS_NAMELEN);
					return;
				}
			}
		}
	}
	error_message("cd", path, -1);

}

void sfs_ls(const char* path) {

	int check = 1;

	if(path == NULL) {
		struct sfs_inode si;
		disk_read(&si, sd_cwd.sfd_ino);
		assert( si.sfi_type == SFS_TYPE_DIR );
		struct sfs_dir sd[SFS_DENTRYPERBLOCK];

		int i, j;
		for(i = 0; i < SFS_NDIRECT; i++) {
			if(si.sfi_direct[i] != SFS_NOINO) {
				disk_read(sd, si.sfi_direct[i]);

				for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
					if(sd[j].sfd_ino != SFS_NOINO) {
						struct sfs_inode inode;
						disk_read(&inode, sd[j].sfd_ino);
						if(inode.sfi_type == SFS_TYPE_FILE) {printf("%s\t", sd[j].sfd_name);}
						else {printf("%s/\t", sd[j].sfd_name);}
					}
				}
			}
		}
		printf("\n");
	}

	if(path != NULL) {
		struct sfs_inode si;
		disk_read(&si, sd_cwd.sfd_ino);
		assert( si.sfi_type == SFS_TYPE_DIR );
		struct sfs_dir sd[SFS_DENTRYPERBLOCK];

		int i, j;
		for(i = 0; i < SFS_NDIRECT; i++) {
			if(si.sfi_direct[i] != SFS_NOINO) {
				disk_read(sd, si.sfi_direct[i]);

				for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
					if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, path, SFS_NAMELEN) == 0) {
						struct sfs_inode inode;
						disk_read(&inode, sd[j].sfd_ino);
						if(inode.sfi_type == SFS_TYPE_FILE) {printf("%s\n", sd[j].sfd_name); return;}
						else {
							int psd_cwd = sd_cwd.sfd_ino;
							sfs_cd(sd[j].sfd_name);
							sfs_ls(NULL);
							sd_cwd.sfd_ino = psd_cwd;
							return;
						}
					}
				}
			}
		}
		error_message("ls", path, -1);
	}

}

void sfs_mkdir(const char* org_path) {

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, org_path, SFS_NAMELEN) == 0) {
					error_message("mkdir", org_path, -6);
					return;
				}
			}
		}
	}

	int emptyEntry = -1, emptyIndex = -1;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] == SFS_NOINO) {
			int emptyBlock = getBlock(); //
			if(emptyBlock == -1) {error_message("mkdir", org_path, -4); return;}
			si.sfi_direct[i] = emptyBlock;
			bzero(sd,SFS_BLOCKSIZE);
			disk_write(sd, emptyBlock);
		}
		disk_read(sd, si.sfi_direct[i]);

		for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if(sd[j].sfd_ino == SFS_NOINO) {
				emptyEntry = i;
				emptyIndex = j;
				break;
			}
		}
		if(emptyEntry != -1) {break;}
	}
	if(emptyEntry == -1) {error_message("mkdir", org_path, -3); return;}

	int emptyBlock1, emptyBlock2;
	emptyBlock1 = getBlock();
	emptyBlock2 = getBlock();
	if(emptyBlock1 == -1) {error_message("mkdir", org_path, -4); return;}
	if(emptyBlock2 == -1) {freeBlock(emptyBlock1); error_message("mkdir", org_path, -4); return;}

	disk_read(sd, si.sfi_direct[emptyEntry]);
	sd[emptyIndex].sfd_ino = emptyBlock1;
	strncpy(sd[emptyIndex].sfd_name, org_path, SFS_NAMELEN);
	disk_write(sd, si.sfi_direct[emptyEntry]);

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;
	bzero(sd,SFS_BLOCKSIZE);
	bzero(&newbie,SFS_BLOCKSIZE);

	strncpy(sd[0].sfd_name, ".", SFS_NAMELEN);
	strncpy(sd[1].sfd_name, "..", SFS_NAMELEN);
	sd[0].sfd_ino = emptyBlock1;
	sd[1].sfd_ino = sd_cwd.sfd_ino;
	disk_write(sd, emptyBlock2);

	newbie.sfi_size = 128;
	newbie.sfi_type = SFS_TYPE_DIR;
	newbie.sfi_direct[0] = emptyBlock2;
	disk_write(&newbie, emptyBlock1);

}

void sfs_rmdir(const char* org_path) {

	if(strncmp(org_path, ".", SFS_NAMELEN) == 0) {error_message("rmdir", org_path, -8); return;}

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j, h;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, org_path, SFS_NAMELEN) == 0) {
					struct sfs_inode inode;
					disk_read(&inode, sd[j].sfd_ino);
					if(inode.sfi_type == SFS_TYPE_FILE) {error_message("rmdir", org_path, -5); return;}
					if(inode.sfi_size != 2*sizeof(struct sfs_dir)) {error_message("rmdir", org_path, -7); return;}

					for(h = 0; h < SFS_NDIRECT; h++) {
						if(inode.sfi_direct[h] != SFS_NOINO) {
							freeBlock(inode.sfi_direct[h]);
						}
					}

					freeBlock(sd[j].sfd_ino);
					sd[j].sfd_ino = SFS_NOINO;
					disk_write(sd, si.sfi_direct[i]);
					si.sfi_size -= sizeof(struct sfs_dir);
					disk_write(&si, sd_cwd.sfd_ino);
					return;
				}
			}
		}
	}
	error_message("rmdir", org_path, -1);

}

void sfs_mv(const char* src_name, const char* dst_name) {

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, dst_name, SFS_NAMELEN) == 0) {error_message("mv", dst_name, -6); return;}
			}
		}
	}

	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, src_name, SFS_NAMELEN) == 0) {
					strcpy(sd[j].sfd_name, dst_name);
					disk_write(sd, si.sfi_direct[i]);
					return;
				}
			}
		}
	}
	error_message("mv", src_name, -1);

}

void sfs_rm(const char* path) {

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j, h;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, path, SFS_NAMELEN) == 0) {
					struct sfs_inode inode;
					disk_read(&inode, sd[j].sfd_ino);
					if(inode.sfi_type == SFS_TYPE_DIR) {error_message("rm", path, -9); return;}

					for(h = 0; h < 15; h++) {
						if(inode.sfi_direct[h] != SFS_NOINO) {
							freeBlock(inode.sfi_direct[h]);
						}
					}

					if(inode.sfi_indirect != SFS_NOINO) {
						int k = SFS_BLOCKSIZE / 4;
						u_int32_t arr[k];
						disk_read(arr, inode.sfi_indirect);
						for(h = 0; h < k; h++) {
							if(arr[h] != SFS_NOINO) {
								freeBlock(arr[h]);
							}
						}
					}

					freeBlock(sd[j].sfd_ino);
					sd[j].sfd_ino = SFS_NOINO;
					disk_write(sd, si.sfi_direct[i]);
					si.sfi_size -= sizeof(struct sfs_dir);
					disk_write(&si, sd_cwd.sfd_ino);
					return;
				}
			}
		}
	}
	error_message("rm", path, -1);

}

void sfs_cpin(const char* local_path, const char* path) {

	int fd = open(path, O_RDWR);

	if(fd < 0) {
		printf("cpin: can't open %s input file\n", path);
		return;
	}

	int size = lseek(fd, 0, SEEK_END);

	if(size > 512*(15+128)) {
		printf("cpin: input file size exceeds the max file size\n");
		return;
	}

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, local_path, SFS_NAMELEN) == 0) {
					error_message("cpin", local_path, -6);
					return;
				}
			}
		}
	}

	int emptyEntry = -1, emptyIndex = -1;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] == SFS_NOINO) {
			int emptyBlock = getBlock();
			if(emptyBlock == -1) {error_message("cpin", local_path, -4); return;}
			si.sfi_direct[i] = emptyBlock;
			bzero(sd,SFS_BLOCKSIZE);
			disk_write(sd, emptyBlock);
		}
		disk_read(sd, si.sfi_direct[i]);

		for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if(sd[j].sfd_ino == SFS_NOINO) {
				emptyEntry = i;
				emptyIndex = j;
				break;
			}
		}
		if(emptyEntry != -1) {break;}
	}
	if(emptyEntry == -1) {error_message("cpin", local_path, -3); return;}

	int emptyBlock;
	emptyBlock = getBlock();
	if(emptyBlock == -1) {error_message("cpin", local_path, -4); return;}

	disk_read(sd, si.sfi_direct[emptyEntry]);
	sd[emptyIndex].sfd_ino = emptyBlock;
	strncpy(sd[emptyIndex].sfd_name, local_path, SFS_NAMELEN);
	disk_write(sd, si.sfi_direct[emptyEntry]);

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;
	bzero(&newbie,SFS_BLOCKSIZE);
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, emptyBlock);

	char buff[SFS_BLOCKSIZE];
	u_int32_t tot = 0;
	int len;
	lseek(fd, 0, SEEK_SET);

	while (tot < size) {
		len = read(fd, buff, SFS_BLOCKSIZE);

		if (len < 0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			err(1, "read");
		}

		if (len==0) {
			err(1, "unexpected EOF in mid-sector");
		}
		tot += len;

		disk_read(&si, emptyBlock);

		for(i = 0; i < SFS_NDIRECT; i++) {
			if(si.sfi_direct[i] == SFS_NOINO) {
				int emptyBlock2 = getBlock();
				if(emptyBlock2 == -1) {error_message("cpin", local_path, -4); return;}

				si.sfi_direct[i] = emptyBlock2;
				si.sfi_size += len;

				disk_write(buff, emptyBlock2);
				disk_write(&si, emptyBlock);
				break;
			}
		}

		if(i == SFS_NDIRECT) {
			if(si.sfi_indirect == SFS_NOINO) {
				int emptyBlock2 = getBlock();
				if(emptyBlock2 == -1) {error_message("cpin", local_path, -4); return;}
				si.sfi_indirect = emptyBlock2;

				emptyBlock2 = getBlock();
				if(emptyBlock2 == -1) {error_message("cpin", local_path, -4); return;}

				int k = SFS_BLOCKSIZE / 4;
				u_int32_t arr[k];
				bzero(arr,SFS_BLOCKSIZE);
				arr[0] = emptyBlock2;
				si.sfi_size += len;

				disk_write(buff, arr[0]);
				disk_write(arr, si.sfi_indirect);
				disk_write(&si, emptyBlock);
			}

			else {
				int k = SFS_BLOCKSIZE / 4;
				u_int32_t arr[k];
				disk_read(arr, si.sfi_indirect);

				for(i = 0; i < k; i++) {
					if(arr[i] == SFS_NOINO) {
						int emptyBlock2 = getBlock();
						if(emptyBlock2 == -1) {error_message("cpin", local_path, -4); return;}
						arr[i] = emptyBlock2;
						si.sfi_size += len;

						disk_write(buff, arr[i]);
						disk_write(arr, si.sfi_indirect);
						disk_write(&si, emptyBlock);
						break;
					}
				}
			}
		}
	}

}

void sfs_cpout(const char* local_path, const char* path) {

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );
	assert( si.sfi_type == SFS_TYPE_DIR );
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	int i, j;
	int entry = -1, index = -1;
	for(i = 0; i < SFS_NDIRECT; i++) {
		if(si.sfi_direct[i] != SFS_NOINO) {
			disk_read(sd, si.sfi_direct[i]);

			for(j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if(sd[j].sfd_ino != SFS_NOINO && strncmp(sd[j].sfd_name, local_path, SFS_NAMELEN) == 0) {
					entry = i;
					index = j;
					break;
				}
			}
		}
		if(entry != -1) {break;}
	}
	if(entry == -1) {error_message("cpout", local_path, -1); return;}

	int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
	if(fd < 0) {error_message("cpout", path, -6); return;}

	disk_read(sd, si.sfi_direct[entry]);
	struct sfs_inode inode;
	disk_read(&inode, sd[index].sfd_ino);

	char buff[SFS_BLOCKSIZE];
	int len;

	for(i = 0; i < SFS_NDIRECT; i++) {
		if(inode.sfi_direct[i] != SFS_NOINO) {
			disk_read(buff, inode.sfi_direct[i]);
			len = write(fd, buff, strlen(buff));
			if(len < SFS_BLOCKSIZE) {return;}
		}
	}

	if(inode.sfi_indirect != SFS_NOINO) {
		int k = SFS_BLOCKSIZE / 4;
		u_int32_t arr[k];
		disk_read(arr, inode.sfi_indirect);

		for(i = 0; i < k; i++) {
			if(arr[i] != SFS_NOINO) {
				disk_read(buff, arr[i]);
				len = write(fd, buff, strlen(buff));
				if(len < SFS_BLOCKSIZE) {return;}
			}
		}
	}

}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
