#ifndef __S3CFB_MDNIE_H__
#define __S3CFB_MDNIE_H__

#define S3C_MDNIE_PHY_BASE		0x11CA0000
#define S3C_MDNIE_MAP_SIZE		0x00001000

#define S3C_MDNIE_rR34		0x0088
#define S3C_MDNIE_rR35		0x008C
#define S3C_MDNIE_rR40		0x00A0

#define	S3C_MDNIE_SIZE_MASK		(0x7FF<<0)

#define END_SEQ			0xffff

#define TRUE				1
#define FALSE				0

int s3c_mdnie_setup(void);
int s3c_mdnie_init_global(struct s3cfb_global *s3cfb_ctrl);
int s3c_mdnie_start(struct s3cfb_global *ctrl);
int s3c_mdnie_off(void);
int s3c_mdnie_stop(void);

int mdnie_write(unsigned int addr, unsigned int val);
int s3c_mdnie_mask(void);


void mDNIe_Init_Set_Mode(void);

#endif /* __S3CFB_MDNIE_H__ */
