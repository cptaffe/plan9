#define	ZCrcPoly	0xedb88320
#define	ZMax32		0xffffffffUL

enum
{
	/*
	 * magic numbers
	 */
	ZHeader		= 0x04034b50,
	ZCHeader	= 0x02014b50,
	ZECHeader	= 0x06054b50,
	Z64End		= 0x06064b50,
	Z64Loc		= 0x07064b50,

	ZMax16		= 0xffff,
	ZX64		= 0x0001,
	ZXTime		= 0x5455,
	ZXNtfs		= 0x000a,
	ZXUnix		= 0x000d,
	ZXInfo		= 0x5855,
	ZDescr		= 0x08074b50,

	/*
	 * "general purpose flag" bits
	 */
	ZEncrypted	= 1 << 0,
	ZTrailInfo	= 1 << 3,	/* uncsize, csize, and crc are in trailer */
	ZCompPatch	= 1 << 5,	/* compression patched data */

	/*
	 * compression method
	 */
	ZStored		= 0,
	ZDeflate	= 8,

	/*
	 * internal file attributes
	 */
	ZIsText		= 1 << 0,

	/*
	 * file attribute interpretation, from high byte of version
	 */
	ZDos		= 0,
	ZAmiga		= 1,
	ZVMS		= 2,
	ZUnix		= 3,
	ZVMCMS		= 4,
	ZAtariST	= 5,
	ZOS2HPFS	= 6,
	ZMac		= 7,
	ZZsys		= 8,
	ZCPM		= 9,
	ZNtfs		= 10,

	/*
	 * external attribute flags for ZDos
	 */
	ZDROnly		= 0x01,
	ZDHidden	= 0x02,
	ZDSystem	= 0x04,
	ZDVLable	= 0x08,
	ZDDir		= 0x10,
	ZDArch		= 0x20,

	ZHeadSize	= 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2,
	ZHeadCrc	= 4 + 2 + 2 + 2 + 2 + 2,
	ZTrailSize	= 4 + 4 + 4,
	ZCHeadSize	= 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4,
	ZECHeadSize	= 4 + 2 + 2 + 2 + 2 + 4 + 4 + 2,
	Z64LocSize	= 4 + 4 + 8 + 4,
	Z64EndSize	= 4 + 8 + 2 + 2 + 4 + 4 + 8 + 8 + 8 + 8,
};
