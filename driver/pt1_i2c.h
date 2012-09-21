#ifndef		__PT1_I2C_H__
#define		__PT1_I2C_H__
#include <linux/mutex.h>
/***************************************************************************/
/* I2Cデータ位置定義                                                       */
/***************************************************************************/
#define		MAX_CHANNEL			4		// チャネル数

#define		FALSE		0
#define		TRUE		1

/***************************************************************************/
/* I2Cデータ位置定義                                                       */
/***************************************************************************/
#define		I2C_ADDRESS		10		// I2Cアドレス(10ビット)

#define		I2C_DATA_EN		10
#define		I2C_CLOCK		11
#define		I2C_WRIET_MODE	12		// I2C書き込み／読み込み
#define		I2C_BUSY		13
#define		I2C_DATA		18		// I2Cデータ(18ビット)
/***************************************************************************/
/* I2C定義                                                                 */
/***************************************************************************/
#define		WRITE_EN		1		// 書き込み
#define		READ_EN			0		// 読み込み
#define		DATA_EN			1		// データあり
#define		DATA_DIS		0		// データなし
#define		CLOCK_EN		1		// CLOCKあり
#define		CLOCK_DIS		0		// CLOCKなし
#define		BUSY_EN			1		// BUSYあり
#define		BUSY_DIS		0		// BUSYなし

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
#define		PCI_LOCKED			1
#define		RAM_LOCKED			2
#define		RAM_SHIFT			4
/***************************************************************************/
/* ビット                                                                  */
/***************************************************************************/
#define		WRITE_PCI_RESET		(1 << 16)
#define		WRITE_PCI_RESET_	(1 << 24)
#define		WRITE_RAM_RESET		(1 << 17)
#define		WRITE_RAM_RESET_	(1 << 25)
#define		WRITE_RAM_ENABLE	(1 << 1)

#define		WRITE_PULSE			(1 << 3)
#define		I2C_READ_SYNC		(1 << 29)
#define		READ_DATA			(1 << 30)
#define		READ_UNLOCK			(1 << 31)

#define		XC3S_PCI_CLOCK		(512 / 4)
#define		XC3S_PCI_CLOCK_PT2	(166)
/***************************************************************************/
/* I2Cアドレス定義                                                         */
/***************************************************************************/
#define		T0_ISDB_S			0X1B		// チューナ0 ISDB-S
#define		T1_ISDB_S			0X19		// チューナ1 ISDB-S

#define		T0_ISDB_T			0X1A		// チューナ0 ISDB-T
#define		T1_ISDB_T			0X18		// チューナ1 ISDB-T

/***************************************************************************/
/* I2C書き込みデータ定義                                                   */
/***************************************************************************/
typedef	struct	_WBLOCK{
	__u8	addr ;			// I2Cデバイスアドレス
	__u32	count ;			// 転送個数
	__u8	value[16];		// 書き込み値
}WBLOCK;

/***************************************************************************/
/* 関数定義                                                                */
/***************************************************************************/
//extern	__u32	makei2c(void __iomem *, __u32, __u32, __u32, __u32, __u32, __u32);
extern	int		xc3s_init(void __iomem *, int);
extern	void	SetStream(void __iomem *, __u32, __u32);
extern	void	blockwrite(void __iomem *, WBLOCK *);
extern	void	i2c_write(void __iomem *, struct mutex *, WBLOCK *);
extern	__u32	i2c_read(void __iomem *, struct mutex *, WBLOCK *, int);

#endif
