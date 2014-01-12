#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include "util.h"
#include "log.h"
#include "debugfs.h"

static ext2_filsys fs;
static ext2_ino_t root, cwd;

ext2_ino_t name_to_inode(char *name)
{
	ext2_ino_t ino;
	int len = strlen(name);
	char *end;

	if (( len > 2 ) && ( name[0] == '<' ) && ( name[len - 1] == '>' )) {
		ino = strtoul(name + 1, &end, 0);
		if ( *end == '>' )
			return ino;
	}

	if ( ext2fs_namei(fs, root, cwd, name, &ino))
		return 0;

	return ino;
}

__u16 inode_mode(char *name)
{
	struct ext2_inode set_inode;
	ext2_ino_t set_ino = name_to_inode(name);

	if (!set_ino)
		return 0;

	if (ext2fs_read_inode(fs, set_ino, &set_inode))
		return 0;

	return set_inode.i_mode;
}

__u16 modeType(__u16 mode)
{

	if ( LINUX_S_ISLNK(mode))
		return LINUX_S_IFLNK;
	else if ( LINUX_S_ISREG(mode))
		return LINUX_S_IFREG;
	else if ( LINUX_S_ISDIR(mode))
		return LINUX_S_IFDIR;
	else if ( LINUX_S_ISCHR(mode))
		return LINUX_S_IFCHR;
	else if ( LINUX_S_ISBLK(mode))
		return LINUX_S_IFBLK;
	else if ( LINUX_S_ISFIFO(mode))
		return LINUX_S_IFIFO;
	else if ( LINUX_S_ISSOCK(mode))
		return LINUX_S_IFSOCK;

	return 0;
}

__u16 ext2_fileType(__u16 mode)
{

	if ( LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;
	else if ( LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;
	else if ( LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;
	else if ( LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;
	else if ( LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;
	else if ( LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;
	else if ( LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;

	return 0; // 0 == EXT2_FT_UNKNOWN..
}

int do_chmode(char *name, mode_t mode)
{
	struct ext2_inode set_inode;
	ext2_ino_t set_ino = name_to_inode(name);

	if (!set_ino)
		return 0;

	if (ext2fs_read_inode(fs, set_ino, &set_inode))
		return 0;

	set_inode.i_mode = mode;

	return ext2fs_write_inode(fs, set_ino, &set_inode) ? 0 : 1;
}

int do_chmod(char *name, unsigned long mode)
{
	struct ext2_inode set_inode;
	ext2_ino_t set_ino = name_to_inode(name);

	if (!set_ino)
		return 0;

	if (ext2fs_read_inode(fs, set_ino, &set_inode))
		return 0;

	if (!modeType(set_inode.i_mode))
		return 0;

	set_inode.i_mode = oct2dec(mode) | modeType(set_inode.i_mode);
	return ext2fs_write_inode(fs, set_ino, &set_inode) ? 0 : 1;
}

int do_chown(char *name, uid_t uid, gid_t gid)
{
	struct ext2_inode set_inode;
	ext2_ino_t set_ino = name_to_inode(name);

	if (!set_ino)
		return 0;

	if (ext2fs_read_inode(fs, set_ino, &set_inode))
		return 0;

	set_inode.i_uid = uid & (~0ULL >> 48);
	set_inode.osd2.linux2.l_i_uid_high = uid >> 16;
	set_inode.i_gid = gid & (~0ULL >> 48);
	set_inode.osd2.linux2.l_i_gid_high = gid >> 16;

	return ext2fs_write_inode(fs, set_ino, &set_inode) ? 0 : 1;
}

int do_mkdir(char *name)
{
	char *cp = strrchr(name, '/');
	ext2_ino_t parent = cp ? name_to_inode(name) : cwd;
	char *newname;

	if (!parent)
		return 0;

	if (cp) {
		*cp = 0;
		newname = cp + 1;
	} else newname = name;

	errcode_t retval = EXT2_ET_DIR_NO_SPACE;
	while ( retval == EXT2_ET_DIR_NO_SPACE ) {
		retval = ext2fs_mkdir(fs, parent, 0, newname);
		if ( retval == EXT2_ET_DIR_NO_SPACE )
			if ( ext2fs_expand_dir(fs, parent))
				return 0;
	}

	return retval ? 0 : 1;
}

int do_chdir(char *name)
{
	ext2_ino_t inode = name_to_inode(name);

	if (( !inode ) || ( ext2fs_check_directory(fs, inode)))
		return 0;

	cwd = inode;
	return 1;
}

static errcode_t copy_file(int fd, ext2_ino_t newfile, int bufsize, int make_holes)
{
	ext2_file_t e2_file;
	int got;
	unsigned int written;
	char            *buf;
	char            *ptr;
	char            *zero_buf;
	int cmp;

	if ( ext2fs_file_open(fs, newfile, EXT2_FILE_WRITE, &e2_file))
		return 0;

	if ( ext2fs_get_mem(bufsize, &buf))
		return 0;

	if ( ext2fs_get_memzero(bufsize, &zero_buf))
		return 0;

	while (1) {
		got = read(fd, buf, bufsize);
		if (got == 0)
			break;
		if (got < 0) { // Failure
			ext2fs_free_mem(&buf);
			ext2fs_free_mem(&zero_buf);
			(void)ext2fs_file_close(e2_file);
			return 0;
		}
		ptr = buf;

		if (make_holes) {
			cmp = memcmp(ptr, zero_buf, got);
			if (cmp == 0) {                                                         /* All zeros, make a hole */
				if ( ext2fs_file_lseek(e2_file, got, EXT2_SEEK_CUR, NULL)) {    // Failure
					ext2fs_free_mem(&buf);
					ext2fs_free_mem(&zero_buf);
					(void)ext2fs_file_close(e2_file);
					return 0;
				}
				got = 0;
			}
		}

		while (got > 0) { // Failure
			if ( ext2fs_file_write(e2_file, ptr, got, &written)) {
				ext2fs_free_mem(&buf);
				ext2fs_free_mem(&zero_buf);
				(void)ext2fs_file_close(e2_file);
				return 0;
			}

			got -= written;
			ptr += written;
		}
	}
	ext2fs_free_mem(&buf);
	ext2fs_free_mem(&zero_buf);
	return ext2fs_file_close(e2_file) ? 0 : 1;
}

int do_write(char *local_file, char *name)
{
	int fd;
	struct stat statbuf;
	ext2_ino_t newfile;
	struct ext2_inode inode;
	int bufsize = IO_BUFSIZE;
	int make_holes = 0;

	fd = open(local_file, O_RDONLY);
	if (fd < 0)
		return 0;

	if (fstat(fd, &statbuf) < 0) {
		close(fd);
		return 0;
	}

	if ( !ext2fs_namei(fs, root, cwd, name, &newfile)) {
		close(fd);
		return 0;
	}

	if ( ext2fs_new_inode(fs, cwd, 010755, 0, &newfile)) {
		close(fd);
		return 0;
	}

	errcode_t retval = ext2fs_link(fs, cwd, name, newfile,
				       EXT2_FT_REG_FILE);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(fs, cwd);
		if (retval) {
			close(fd);
			return 0;
		}
		retval = ext2fs_link(fs, cwd, name, newfile,
				     EXT2_FT_REG_FILE);
	}
	if (retval) {
		close(fd);
		return 0;
	}

	if (ext2fs_test_inode_bitmap2(fs->inode_map, newfile))
		log_warning("[write %s/%s] inode <%ld> already set", cwd, name, newfile);

	ext2fs_inode_alloc_stats2(fs, newfile, +1, 0);
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = (statbuf.st_mode & ~LINUX_S_IFMT) | LINUX_S_IFREG;
	inode.i_atime = inode.i_ctime = inode.i_mtime =
						fs->now ? fs->now : time(0);
	inode.i_links_count = 1;
	inode.i_size = statbuf.st_size;
	if (fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_EXTENTS) {
		int i;
		struct ext3_extent_header *eh;

		eh = (struct ext3_extent_header *)&inode.i_block[0];
		eh->eh_depth = 0;
		eh->eh_entries = 0;
		eh->eh_magic = ext2fs_cpu_to_le16(EXT3_EXT_MAGIC);
		i = (sizeof(inode.i_block) - sizeof(*eh)) /
		    sizeof(struct ext3_extent);
		eh->eh_max = ext2fs_cpu_to_le16(i);
		inode.i_flags |= EXT4_EXTENTS_FL;
	}
	if ( ext2fs_write_new_inode(fs, newfile, &inode)) {
		close(fd);
		return 0;
	}

	if (LINUX_S_ISREG(inode.i_mode)) {
		if (statbuf.st_blocks < statbuf.st_size / S_BLKSIZE) {
			make_holes = 1;
			bufsize = statbuf.st_blksize;
		}
		if ( !copy_file(fd, newfile, bufsize, make_holes)) {
			close(fd);
			return 0;
		}
	}
	close(fd);
	return 1;
}

int do_hardlink(char *sourcename, char *name)
{
	ext2_ino_t ino = name_to_inode(sourcename);
	struct ext2_inode inode;
	int retval;
	ext2_ino_t dir;
	char                *dest, *cp;
	char *base_name = strrchr(sourcename, '/');

	if (!ino)
		return 0;

	if (base_name)
		base_name++;
	else
		base_name = sourcename;

	if (!(retval = ext2fs_namei(fs, root, cwd, name, &dir)))
		dest = base_name;
	else {
		cp = strrchr(name, '/');
		if (cp) {
			*cp = 0;
			dir = name_to_inode(name);
			if (!dir)
				return 0;
			dest = cp + 1;
		} else {
			dir = cwd;
			dest = name;
		}
	}

	if ( ext2fs_read_inode(fs, ino, &inode))
		return 0;

	__u16 linkCount = inode.i_links_count;
	__u16 type = ext2_fileType(inode.i_mode);

	if ( type == EXT2_FT_UNKNOWN)
		return 0;

	if ( ext2fs_link(fs, dir, dest, ino, type))
		return 0;

	if ( ext2fs_read_inode(fs, ino, &inode))
		return 0;

	if ( inode.i_links_count == linkCount ) { // Workaround
		inode.i_links_count++;
		if ( ext2fs_write_inode(fs, ino, &inode))
			return 0;
	}

	return 1;
}

int do_symlink(char *source, char *dest)
{
	char                *cp;
	ext2_ino_t parent;
	char                *name;
	errcode_t retval;

	cp = strrchr(dest, '/');
	if (cp) {
		*cp = 0;
		parent = name_to_inode(dest);
		if (!parent)
			return 0;
		name = cp + 1;
	} else {
		parent = cwd;
		name = dest;
	}

	retval = EXT2_ET_DIR_NO_SPACE;
	while ( retval == EXT2_ET_DIR_NO_SPACE ) {
		retval = ext2fs_symlink(fs, parent, 0, name, source);
		if ( retval == EXT2_ET_DIR_NO_SPACE) {
			if ( ext2fs_expand_dir(fs, parent))
				return 0;
		} else if ( retval ) return 0;
	}

	return 1;

}

int do_mknod(char *name, int type, unsigned long major, unsigned long minor)
{
	unsigned long mode;
	ext2_ino_t newfile;
	struct ext2_inode inode;
	int type2;

	mode = 384; // chmod 600

	switch ( type ) {
	case 'f':
	case EXT2_FT_REG_FILE:
		type2 = EXT2_FT_REG_FILE;
		mode |= LINUX_S_IFREG;
		break;
	case 'p':
	case EXT2_FT_FIFO:
		type2 = EXT2_FT_FIFO;
		mode |= LINUX_S_IFIFO;
		break;
	case 's':
	case EXT2_FT_SOCK:
		type2 = EXT2_FT_SOCK;
		mode |= LINUX_S_IFSOCK;
		break;
	case 'c':
	case EXT2_FT_CHRDEV:
		type2 = EXT2_FT_CHRDEV;
		mode |= LINUX_S_IFCHR;
		break;
	case 'b':
	case EXT2_FT_BLKDEV:
		type2 = EXT2_FT_BLKDEV;
		mode |= LINUX_S_IFBLK;
		break;
	default:
		return 0;
	}

	if ( ext2fs_new_inode(fs, cwd, 010755, 0, &newfile))
		return 0;

	errcode_t retval = ext2fs_link(fs, cwd, name, newfile, type2);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		if ( ext2fs_expand_dir(fs, cwd))
			return 0;
		retval = ext2fs_link(fs, cwd, name, newfile, type2);
	}

	if (retval)
		return 0;

	if (ext2fs_test_inode_bitmap2(fs->inode_map, newfile))
		log_warning("[write %s/%s] inode <%ld> already set", cwd, name, newfile);

	ext2fs_inode_alloc_stats2(fs, newfile, +1, 0);
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = mode;
	inode.i_atime = inode.i_ctime = inode.i_mtime = fs->now ? fs->now : time(0);
	if (( type2 == EXT2_FT_REG_FILE ) || ( type2 == EXT2_FT_FIFO ) || ( type2 == EXT2_FT_SOCK )) {
		inode.i_block[0] = 0 * 256 + minor;
		inode.i_block[1] = 0;
	} else if ((major < 256) && (minor < 256)) {
		inode.i_block[0] = major * 256 + minor;
		inode.i_block[1] = 0;
	} else {
		inode.i_block[0] = 0;
		inode.i_block[1] = (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
	}

	inode.i_links_count = 1;

	if ( ext2fs_write_new_inode(fs, newfile, &inode))
		return 0;

	return 1;
}

static int release_blocks_proc(ext2_filsys fs, blk64_t *blocknr,
			       e2_blkcnt_t blockcnt EXT2FS_ATTR((unused)),
			       blk64_t ref_block EXT2FS_ATTR((unused)),
			       int ref_offset EXT2FS_ATTR((unused)),
			       void *private EXT2FS_ATTR((unused)))
{
	blk64_t block = *blocknr;

	ext2fs_block_alloc_stats2(fs, block, -1);
	return 0;
}

static int unlink_file_by_name(char *name)
{
	ext2_ino_t dir;
	char            *base_name = strrchr(name, '/');

	if (base_name) {
		*base_name++ = '\0';
		dir = name_to_inode(name);
		if (!dir) // Already unlinked
			return 1;
	} else {
		dir = cwd;
		base_name = name;
	}

	return ext2fs_unlink(fs, dir, base_name, 0, 0) ? 0 : 1;
}

static int kill_file_by_inode(ext2_ino_t inode)
{
	struct ext2_inode inode_buf;

	if ( ext2fs_read_inode(fs, inode, &inode_buf))
		return 0;

	inode_buf.i_dtime = fs->now ? fs->now : time(0);
	if ( ext2fs_write_inode(fs, inode, &inode_buf))
		return 0;

	if (ext2fs_inode_has_valid_blocks2(fs, &inode_buf))
		ext2fs_block_iterate3(fs, inode, BLOCK_FLAG_READ_ONLY, NULL, release_blocks_proc, NULL);

	ext2fs_inode_alloc_stats2(fs, inode, -1, LINUX_S_ISDIR(inode_buf.i_mode));
	return 1;
}

int do_rm(char *name)
{
	ext2_ino_t inode_num;
	struct ext2_inode inode;

	if ( ext2fs_namei(fs, root, cwd, name, &inode_num))
		return 0;

	if ( ext2fs_read_inode(fs, inode_num, &inode))
		return 0;

	if (LINUX_S_ISDIR(inode.i_mode))
		return 0;

	--inode.i_links_count;
	if ( ext2fs_write_inode(fs, inode_num, &inode))
		return 0;

	if ( !unlink_file_by_name(name))
		return 0;

	if (inode.i_links_count == 0)
		if ( !kill_file_by_inode(inode_num))
			return 0;

	return 1;
}

int open_filesystem(char *device)
{

	io_channel data_io = 0;

	if (ext2fs_open(device, (EXT2_FLAG_SOFTSUPP_FEATURES | EXT2_FLAG_64BITS | EXT2_FLAG_RW), 0, 0, unix_io_manager, &fs)) {
		fs = NULL;
		return 0;
	}

	fs->default_bitmap_type = EXT2FS_BMAP64_RBTREE;

	if ( ext2fs_read_inode_bitmap(fs)) {
		ext2fs_close(fs);
		fs = NULL;
		return 0;
	}

	if ( ext2fs_read_block_bitmap(fs)) {
		ext2fs_close(fs);
		fs = NULL;
		return 0;
	}

	if (data_io)
		if ( ext2fs_set_data_io(fs, data_io)) {
			ext2fs_close(fs);
			fs = NULL;
			return 0;
		}

	root = cwd = EXT2_ROOT_INO;
	return 1;
}

int close_filesystem(void)
{

	if ( fs->flags & EXT2_FLAG_IB_DIRTY )
		if ( ext2fs_write_inode_bitmap(fs))
			return 0;

	if ( fs->flags & EXT2_FLAG_BB_DIRTY)
		if ( ext2fs_write_block_bitmap(fs))
			return 0;

	if ( ext2fs_close(fs))
		return 0;

	fs = NULL;
	return 1;
}

int fs_isClosed(void)
{
	return !fs ? 1 : 0;
}

int fs_isReadWrite(void)
{
	return fs->flags & EXT2_FLAG_RW;
}
