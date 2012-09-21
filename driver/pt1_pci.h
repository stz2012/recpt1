#ifndef		__PT1_PCI_H__
#define		__PT1_PCI_H__
/***************************************************************************/
/* PCIアドレス定義                                                         */
/***************************************************************************/
#define		FIFO_GO			0x04			// FIFO実行
#define		FIFO_DONE		0x80			// FIFO 実行中ビット
/***************************************************************************/
/* PCIアドレス定義                                                         */
/***************************************************************************/
#define		FIFO_GO_ADDR		0x00			// FIFO 実行アドレス
#define		FIFO_RESULT_ADDR	0x00			// FIFO 結果情報
#define		CFG_REGS_ADDR		0x04
#define		I2C_RESULT_ADDR		0x08			// I2C処理結果
#define		FIFO_ADDR			0x10			// FIFOに書くアドレス
#define		DMA_ADDR			0x14			// DMA設定に書くアドレス
#define		TS_TEST_ENABLE_ADDR	0x08			//

/***************************************************************************/
/* DMAエラー定義                                                           */
/***************************************************************************/
#define		MICROPACKET_ERROR		1			// Micro Packetエラー
#define		BIT_RAM_OVERFLOW		(1 << 3)	//
#define		BIT_INITIATOR_ERROR		(1 << 4)	//
#define		BIT_INITIATOR_WARNING	(1 << 5)	//
#endif
