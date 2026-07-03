#include <CPP/Windows/PropVariant.h>
#include <windows.h>
#include <string>

namespace Vdfs {
    
    

    NWindows::NCOM::CPropVariant GetProperty(PROPID propId);

    static const Byte vdfs_signature[] = {'V', 'D', 'F', 'S'};

    static const Byte VDFS4_BTREE_HEAD_NODE_MAGIC[] = {'e', 'H', 'N', 'D'};
    static const Byte BTREES_END_MAGIC[] = {0xED, 0xAC, 0xEF, 0x0D};
    static const Byte NODE_MAGIC[] = {'N', 'd', 0x00, 0x00};

    #define IMAGE_CMD_LENGTH (512 - 4 - 4 - 16 - 16 - 12 - 4)
    #define VDFS4_MAX_CRYPTED_HASH_LEN		256

    #pragma pack(push, 1)
    struct vdfs4_timespec {
	    /** The seconds part of the date */
	    UInt32	seconds;
	    UInt32	seconds_high;
	    /** The nanoseconds part of the date */
	    UInt32	nanoseconds;
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct vdfs4_extent {
	    UInt64	begin;
	    UInt64	length;
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct vdfs4_volume_begins {
	    /** magic */
	    uint8_t signature[4];		/* VDFS4 */
	    /** vdfs4 layout version **/
	    uint8_t layout_version[4];
	    /* image was created with command line */
	    uint8_t command_line[IMAGE_CMD_LENGTH];
	    /* image creation time, YYYY.MM.DD-hh:mm(16byte) */
	    uint8_t creation_time[16];
	    /* image creater username, (16byte) */
	    uint8_t user_name[16];
	    /* reserved */
	    uint8_t reserved[12];
	    /** Checksum */
	    UInt32 checksum;
    };
    #pragma pack(pop)

    /**
    * @brief	The eMMCFS superblock. (+0x200/+0x400)
    */
   #pragma pack(push, 1)
    struct vdfs4_super_block {
	    /** magic */
        uint8_t signature[4];		/* VDFS4 */
	    /** vdfs4 layout version **/
	    uint8_t layout_version[4];

	    /* maximum blocks count on volume */
	    UInt64 maximum_blocks_count;

	    /** Creation timestamp */
        struct vdfs4_timespec creation_timestamp;

	    /** 128-bit uuid for volume */
	    uint8_t volume_uuid[16];

	    /** Volume name */
        char volume_name[16];

	    /** The mkfs tool version used to generate Volume */
        char mkfs_version[64];
	    /** unused field */
        char unused[40];

	    /** log2 (Block size in bytes) */
        uint8_t log_block_size;

	    /** Metadata bnode size and alignment */
        uint8_t log_super_page_size;

	    /** Discard request granularity */
	    uint8_t log_erase_block_size;

	    /** Case insensitive mode */
	    uint8_t case_insensitive;

	    /** Read-only image */
	    uint8_t read_only;

	    uint8_t image_crc32_present;

	    /*force full decompression enable/disable bit*/
	    uint8_t force_full_decomp_decrypt;

	    /*hash calculation algorithm*/
	    uint8_t hash_type;

	    /* AES encryption flags */
	    uint8_t encryption_flags;

	    /* sb signature type */
	    uint8_t sign_type;

	    /** Padding */
        uint8_t reserved[54];

        UInt32 exsb_checksum;

        UInt32 basetable_checksum;

        UInt32 meta_hashtable_checksum;

        UInt64 image_inode_count;

	    UInt32 pad;

	    /*RSA enctypted hash code of superblock*/
        uint8_t sb_hash[VDFS4_MAX_CRYPTED_HASH_LEN];

	    /** Checksum */
        UInt32 checksum;
    };
    #pragma pack(pop)

    #define VDFS4_META_BTREE_EXTENTS		96

    #pragma pack(push, 1)
    struct vdfs4_extended_super_block {
	    /** File system files count */
	    UInt64			files_count;
	    /** File system folder count */
	    UInt64			folders_count;
	    /** Extent describing the volume */
        struct vdfs4_extent	volume_body;
	    /** Number of mount operations */
        UInt32			mount_counter;
	    /* SYNC counter */
	    UInt32			sync_counter;		/* not used */
	    /** Number of umount operations */
	    UInt32			umount_counter;
	    /** inode numbers generation */
	    UInt32			generation;
	    /** Debug area position */
        struct vdfs4_extent	debug_area;
	    /* btrees extents total block count */
        UInt32			meta_tbc;
	    UInt32			pad;
	    /** translation tables extents */
        struct vdfs4_extent	tables;
	    /** btrees extents */
        struct vdfs4_extent	meta[VDFS4_META_BTREE_EXTENTS];
	    /* translation tables extention */
        struct vdfs4_extent	extension;		/* not used */

	    /* volume size in blocks, could be increased at first mounting */
        UInt64			volume_blocks_count;

	    /** Flag indicating signed image */
        uint8_t			crc;

	    /** 128-bit uuid for volume */
        uint8_t			volume_uuid[16];

	    /** Reserved */
        uint8_t			_reserved[7];

	    /** Write statistics */
        UInt64			kbytes_written;

	    /** Meta HashTable extent */
        struct vdfs4_extent	meta_hashtable_area;

	    /** Reserved */
        uint8_t			reserved[860];

	    /** Extended superblock checksum */
        UInt32			checksum;
    };
    #pragma pack(pop)

    struct vdfs4_gen_node_descr {
	    /** Magic */
	    uint8_t magic[4];
	    UInt32 version[2];
	    /** Free space left in bnode */
	    UInt16 free_space;
	    /** Amount of records that this bnode contains */
	    UInt16 recs_count;
	    /** Node id */
	    UInt32 node_id;
	    /** Node id of left sibling */
	    UInt32 prev_node_id;
	    /** Node id of right sibling */
	    UInt32 next_node_id;
	    /** Type of bnode node or index (value of enum vdfs4_node_type) */
	    UInt32 type;
    };

    struct vdfs4_generic_key {
	    /** Unique number that identifies structure */
	    uint8_t magic[4];
	    /** Length of tree-specific key */
	    UInt16 key_len;
	    /** Full length of record containing the key */
	    UInt16 record_len;
    };

    #define	VDFS4_FILE_NAME_LEN		255

    struct vdfs4_cattree_key {
	    /** Generic key part */
	    struct vdfs4_generic_key gen_key;
	    /** Object id of parent object (directory) */
	    UInt64 parent_id;
	    /** Object id of child object (file) */
	    UInt64 object_id;
	    /** Catalog tree record type */
	    uint8_t record_type;
	    /** Object's name */
	    uint8_t	name_len;
	    char	name[VDFS4_FILE_NAME_LEN];
    };

    /* Catalog btree record types */
    #define VDFS4_CATALOG_RECORD_DUMMY		0x00
    #define VDFS4_CATALOG_FOLDER_RECORD		0x01
    #define VDFS4_CATALOG_FILE_RECORD		0x02
    #define VDFS4_CATALOG_HLINK_RECORD		0x03
    /* UNUSED:								0x04 */
    #define VDFS4_CATALOG_ILINK_RECORD		0x05
    #define VDFS4_CATALOG_UNPACK_INODE		0x10

    struct vdfs4_catalog_hlink_record {
	    /** file mode */
	    UInt16  file_mode;
	    UInt16	pad1;
	    UInt32	pad2;
    };

    struct vdfs4_catalog_folder_record {
	    /** Flags */
	    UInt32	flags;
	    UInt32	generation;
	    /** Amount of files in the directory */
	    UInt64	total_items_count;
	    /** Link's count for file */
	    UInt64	links_count;
	    /** Next inode in orphan list */
	    UInt64  next_orphan_id;
	    /** File mode */
	    UInt16	file_mode;
	    UInt16	pad;
	    /** User ID */
	    UInt32	uid;
	    /** Group ID */
	    UInt32	gid;
	    /** Record creation time */
	    struct vdfs4_timespec	creation_time;
	    /** Record modification time */
	    struct vdfs4_timespec	modification_time;
	    /** Record last access time */
	    struct vdfs4_timespec	access_time;
    };

	#define VDFS4_EXTENTS_COUNT_IN_FORK	9
    #define VDFS4_INLINE_DATA_MAX_SIZE (200) /* sizeof(struct vdfs4_fork_info) */
    #define VDFS4_COMP_INLINE_DATA_MAX_SIZE (196) /* sizeof(struct vdfs4_fork_info) - 4 */
    struct vdfs4_comp_inline_data {
	    uint8_t compr_type[3];
	    uint8_t compr_sz;
	    uint8_t data[VDFS4_COMP_INLINE_DATA_MAX_SIZE];
    };

	struct vdfs4_iextent {
	    /** file data location */
	    struct vdfs4_extent	extent;
	    /** extent start block logical index */
	    UInt64	iblock;
    };

    struct vdfs4_fork {
	    /** The size in bytes of the valid data in the fork */
	    UInt64			size_in_bytes;
	    /** The total number of allocation blocks which is
	     * allocated for file system object under last actual
	     * snapshot in this fork */
	    UInt64			total_blocks_count;
	    /** The set of extents which describe file system
	     * object's blocks placement */
	    union {
	    	struct vdfs4_iextent extents[VDFS4_EXTENTS_COUNT_IN_FORK];
	    	uint8_t inline_data[VDFS4_INLINE_DATA_MAX_SIZE];
	    	struct vdfs4_comp_inline_data comp_inline_data;
	    };
    };

    struct vdfs4_catalog_file_record {
	    /** Common part of record (file or folder) */
	    struct vdfs4_catalog_folder_record common;
		/** Fork containing info about area occupied by file */
		struct vdfs4_fork data_fork;
    };

	/** The VDFS4 inode flags */
	#define HAS_BLOCKS_IN_EXTTREE	(1 << 1)
	#define VDFS4_IMMUTABLE		(1 << 2)
	#define HARD_LINK		(1 << 10)
	#define ORPHAN_INODE		(1 << 12)
	#define VDFS4_COMPRESSED_FILE	(1 << 13)
	/* UNUSED:			 			14 */
	#define VDFS4_AUTH_FILE		(1 << 15)
	#define VDFS4_READ_ONLY_AUTH	(1 << 16)
	#define VDFS4_ENCRYPTED_FILE	(1 << 17)
	#define VDFS4_PROFILED_FILE	(1 << 18)
	#define VDFS4_INLINE_DATA_FILE	(1 << 19)
	#define VDFS4_COMP_INLINE_DATA_FILE	(1 << 20)

	///////

	struct MyInode {
		uint8_t kind;

		vdfs4_catalog_folder_record folder_record;
		vdfs4_catalog_file_record file_record;
    };

	struct InoItem {
		std::string path;
		uint64_t inode_id;
	};

    
}