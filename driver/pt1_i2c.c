/***************************************************************************/
/* I2C情報作成                                                             */
/***************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <asm/system.h>
#endif
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include	"pt1_com.h"
#include	"pt1_i2c.h"
#include	"pt1_pci.h"
#include	"pt1_tuner.h"

#define		PROGRAM_ADDRESS		1024
static	int		state = STATE_STOP ;
static	int		i2c_lock(void __iomem *, __u32, __u32, __u32);
static	int		i2c_lock_one(void __iomem *, __u32, __u32);
static	int		i2c_unlock(void __iomem *, int);
static	void	writebits(void __iomem *, __u32 *, __u32, __u32);
static	void	begin_i2c(void __iomem *, __u32 *, __u32 *);
static	void	start_i2c(void __iomem *, __u32 *, __u32 *, __u32);
static	void	stop_i2c(void __iomem *, __u32 *, __u32 *, __u32, __u32);


// PCIに書き込むI2Cデータ生成
void	makei2c(void __iomem *regs, __u32 base_addr, __u32 i2caddr, __u32 writemode, __u32 data_en, __u32 clock, __u32 busy)
{

	__u32		val ;
	val =  ((base_addr << I2C_DATA) | (writemode << I2C_WRIET_MODE) |
			( data_en << I2C_DATA_EN) |
			(clock << I2C_CLOCK) | (busy << I2C_BUSY) | i2caddr) ;
	writel(val, regs + FIFO_ADDR);
}

int		xc3s_init(void __iomem *regs, int cardtype)
{

	__u32	val ;
	int		lp ;
	int		rc ;
	int		phase = XC3S_PCI_CLOCK;

/*
	val = (1 << 19) | (1 << 27) | (1 << 16) | (1 << 24) | (1 << 17) | (1 << 25);
	writel(WRITE_PULSE, regs);
BIT 19, 19+8 ON
BIT 16, 16+8 ON
BIT 17, 17+8 ON
 */
	// XC3S初期化
	for(lp = 0 ; lp < PROGRAM_ADDRESS ; lp++){
		makei2c(regs, lp, 0, READ_EN, DATA_DIS, CLOCK_DIS, BUSY_DIS);
	}
	// XC3S 初期化待ち (512 PCI Clocks)
	for(lp = 0 ; lp <  XC3S_PCI_CLOCK ; lp++){
		makei2c(regs, 0, 0, READ_EN, DATA_DIS, CLOCK_DIS, BUSY_DIS);
	}
	// プロテクト解除
	// これは何を意図しているんだろう？
	// 元コードが良く判らない
	for(lp = 0 ; lp < 57 ; lp++){
		val = readl(regs);
		if(val & I2C_READ_SYNC){
			break ;
		}
		writel(WRITE_PULSE, regs);
	}

	for(lp = 0 ; lp < 57 ; lp++){
		val = readl(regs);
		if(val & READ_DATA){
			break ;
		}
		writel(WRITE_PULSE, regs);
	}

	// UNLOCK
	rc = i2c_unlock(regs, READ_UNLOCK);
	if(rc < 0){
		return rc ;
	}

	// Enable PCI
	rc =i2c_lock(regs, (WRITE_PCI_RESET | WRITE_PCI_RESET_), WRITE_PCI_RESET_, PCI_LOCKED);
	if(rc < 0){
		return -EIO ;
	}

	// Enable RAM
	rc =i2c_lock(regs, (WRITE_RAM_RESET | WRITE_RAM_RESET_), WRITE_RAM_RESET_, RAM_LOCKED);
	if(rc){
		return -EIO ;
	}
	switch(cardtype) {
        case PT1:
		phase = XC3S_PCI_CLOCK;
		break;
	case PT2:
		phase = XC3S_PCI_CLOCK_PT2;
		break;
	}
	for(lp = 0; lp < phase; lp++){
		rc = i2c_lock_one(regs, WRITE_RAM_ENABLE, RAM_SHIFT);
		if(rc < 0){
			printk(KERN_ERR "PT1:LOCK FALUT\n");
			return rc ;
		}
	}

	// ストリームごとの転送制御(OFF)
	for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
		SetStream(regs, lp, 0);
		SetStream(regs, lp, 0);
	}
	return 0 ;
}
//
//
//BIT 0. 1 : Tuner番号 (Enable/Disable)
//BIT 8. 9 : Tuner番号
//
//
void	SetStream(void __iomem *regs, __u32 channel, __u32 enable)
{
	__u32	val ;

	val = (1 << (8 + channel));
	if(enable){
		val |= (1 << channel);
	}
	writel(val, regs + TS_TEST_ENABLE_ADDR);
}

static	int		i2c_lock(void __iomem *regs, __u32 firstval, __u32  secondval, __u32 lockval)
{

	__u32	val ;
	int		lp ;

	writel(firstval, regs);
	writel(secondval, regs);

	// RAMがロックされた？
	for(lp = 0 ; lp < XC3S_PCI_CLOCK ; lp++){
		val = readl(regs);
		if((val & lockval)){
			return 0 ;
		}
		schedule_timeout_interruptible(msecs_to_jiffies(1));
	}
	return -EIO ;
}

static	int		i2c_lock_one(void __iomem *regs, __u32 firstval, __u32 lockval)
{

	__u32	val ;
	__u32	val2 ;
	int		lp ;

	val = (readl(regs) & lockval);
	writel(firstval, regs);

	// RAMがロックされた？
	for(lp = 0 ; lp < 10 ; lp++){
		for(lp = 0 ; lp < 1024 ; lp++){
			val2 = readl(regs);
			// 最初に取得したデータと逆になればOK
			if(((val2 & lockval) != val)){
				return 0 ;
			}
		}
		schedule_timeout_interruptible(msecs_to_jiffies(1));
	}
	printk(KERN_INFO "PT1:Lock Fault(%x:%x)\n", val, val2);
	return -EIO ;
}
static	int		i2c_unlock(void __iomem *regs, int lockval)
{
	int		lp ;
	__u32	val ;

	writel(WRITE_PULSE, regs);

	for(lp = 0 ; lp < 3 ; lp++){
		val = readl(regs);
		if((val &lockval)){
			return 0 ;
		}
		schedule_timeout_interruptible(msecs_to_jiffies(1));
	}
	return -EIO ;
}
void	blockwrite(void __iomem *regs, WBLOCK *wblock)
{
	int		lp ;
	int		bitpos ;
	__u32	bits ;
	__u32	old_bits = 1 ;
	__u32	address = 0;
	__u32	clock = 0;

	begin_i2c(regs, &address, &clock);
	if(state == STATE_STOP){
		start_i2c(regs, &address, &clock, old_bits);
		old_bits = 0 ;
		stop_i2c(regs, &address, &clock, old_bits, FALSE);
		state = STATE_START ;
	}
	old_bits = 1 ;
	start_i2c(regs, &address, &clock, old_bits);
	old_bits = 0 ;

	// まずアドレスを書く
	for(bitpos = 0 ; bitpos < 7 ; bitpos++){
		bits  = ((wblock->addr >> (6 - bitpos)) & 1);
		writebits(regs, &address, old_bits, bits);
		old_bits = bits ;
	}
	// タイプ：WRT
	writebits(regs, &address, old_bits, 0);
	// ACK/NACK用(必ず1)
	writebits(regs, &address, 0, 1);

	old_bits = 1 ;
	// 実際のデータを書く
	for (lp = 0 ; lp < wblock->count ; lp++){
		for(bitpos = 0 ; bitpos < 8 ; bitpos++){
			bits  = ((wblock->value[lp] >> (7 - bitpos)) & 1);
			writebits(regs, &address, old_bits, bits);
			old_bits = bits ;
		}
		// ACK/NACK用(必ず1)
		writebits(regs, &address, old_bits, 1);
		old_bits = 1 ;
	}

	// Clock negedge
	makei2c(regs, address, address + 1, 0, (old_bits ^ 1), 1, 1);
	clock = TRUE ;
	address += 1 ;
	stop_i2c(regs, &address, &clock, old_bits, TRUE);

}

void	blockread(void __iomem *regs, WBLOCK *wblock, int count)
{
	int		lp ;
	int		bitpos ;
	__u32	bits ;
	__u32	old_bits = 1 ;
	__u32	address = 0;
	__u32	clock = 0;

	begin_i2c(regs, &address, &clock);
	if(state == STATE_STOP){
		start_i2c(regs, &address, &clock, old_bits);
		old_bits = 0 ;
		stop_i2c(regs, &address, &clock, old_bits, FALSE);
		state = STATE_START ;
	}
	old_bits = 1 ;
	start_i2c(regs, &address, &clock, old_bits);
	old_bits = 0 ;

	// まずアドレスを書く
	for(bitpos = 0 ; bitpos < 7 ; bitpos++){
		bits  = ((wblock->addr >> (6 - bitpos)) & 1);
		writebits(regs, &address, old_bits, bits);
		old_bits = bits ;
	}
	// タイプ：WRT
	writebits(regs, &address, old_bits, 0);
	// ACK/NACK用(必ず1)
	writebits(regs, &address, 0, 1);

	old_bits = 1 ;
	// 実際のデータを書く
	for (lp = 0 ; lp < wblock->count ; lp++){
		for(bitpos = 0 ; bitpos < 8 ; bitpos++){
			bits  = ((wblock->value[lp] >> (7 - bitpos)) & 1);
			writebits(regs, &address, old_bits, bits);
			old_bits = bits ;
		}
		// ACK/NACK用(必ず1)
		writebits(regs, &address, old_bits, 1);
		old_bits = 1 ;
	}

	// Clock negedge
	makei2c(regs, address, address + 1, 0, (old_bits ^ 1), 1, 1);
	clock = TRUE ;
	address += 1 ;

	// ここから Read
	start_i2c(regs, &address, &clock, old_bits);
	old_bits = 0 ;
	// まずアドレスを書く
	for(bitpos = 0 ; bitpos < 7 ; bitpos++){
		bits  = ((wblock->addr >> (6 - bitpos)) & 1);
		writebits(regs, &address, old_bits, bits);
		old_bits = bits ;
	}
	// タイプ：RD
	writebits(regs, &address, old_bits, 1);
	// ACK/NACK用(必ず1)
	writebits(regs, &address, 1, 1);

	old_bits = 1 ;
	// 実際のデータを書く
	for (lp = 0 ; lp < count ; lp++){
		for(bitpos = 0 ; bitpos < 8 ; bitpos++){
			writebits(regs, &address, old_bits, 1);
			// Read Mode Set
			makei2c(regs, address, address + 1, 1, 0, 0, 1);
			address += 1 ;
			old_bits = 1 ;
		}
		if(lp >= (count - 1)){
			// ACK/NACK用(必ず1)
			writebits(regs, &address, old_bits, 1);
			old_bits = 0 ;
		}else{
			// ACK/NACK用(必ず1)
			writebits(regs, &address, old_bits, 0);
			old_bits = 1 ;
		}
	}

	// Clock negedge
	makei2c(regs, address, address + 1, 0, 0, 1, 1);
	clock = TRUE ;
	address += 1 ;
	old_bits = 1 ;
	stop_i2c(regs, &address, &clock, old_bits, TRUE);

}
static	void	writebits(void __iomem *regs, __u32 *address, __u32 old_bits, __u32 bits)
{
	// CLOCK UP
	makei2c(regs, *address, *address + 1, 0, (old_bits ^ 1), 1, 1);
	*address += 1 ;

	// CLOCK UP
	makei2c(regs, *address, *address + 1, 0, (bits ^ 1), 1, 1);
	*address += 1 ;

	// CLOCK DOWN
	makei2c(regs, *address, *address + 1, 0, (bits ^ 1), 0, 1);
	*address += 1 ;

}
static	void begin_i2c(void __iomem *regs, __u32 *address, __u32 *clock)
{
	// bus FREE
	makei2c(regs, *address, *address, 0, 0, 0, 0);
	*address += 1 ;

	//  bus busy
	makei2c(regs, *address, *address + 1, 0, 0, 0, 1);
	*address += 1 ;
	*clock = FALSE ;
}

static	void	start_i2c(void __iomem *regs, __u32 *address, __u32 *clock, __u32 data)
{
	// データが残っていなければデータを下げる
	if(!data){
		// CLOCKがあればCLOCKを下げる
		if(*clock != TRUE){
			*clock = TRUE ;
			makei2c(regs, *address, *address + 1, 0, 1, 1, 1);
			*address += 1 ;
		}
		makei2c(regs, *address, *address + 1, 0, 0, 1, 1);
		*address += 1 ;
	}

	if(*clock != FALSE){
		*clock = FALSE ;
		makei2c(regs, *address, *address + 1, 0, 0, 0, 1);
		*address += 1;
	}
	makei2c(regs, *address, *address + 1, 0, 1, 0, 1);
	*address += 1;
	*clock = FALSE ;
}

static	void	stop_i2c(void __iomem *regs, __u32 *address, __u32 *clock, __u32 data, __u32 end)
{
	// データが残っていて
	if(data){
		// クロックがあれば
		if(*clock != TRUE){
			*clock = TRUE ;
			makei2c(regs, *address, *address + 1, 0, 0, 1, 1);
			*address += 1;
		}
		makei2c(regs, *address, *address + 1, 0, 1, 1, 1);
		*address += 1 ;
	}
	// クロックが落ちていれば
	if(*clock){
		*clock = FALSE ;
		makei2c(regs, *address, *address + 1, 0, 1, 0, 1);
		*address += 1 ;
	}

	if(end){
		makei2c(regs, *address, 0, 0, 0, 0, 1);
	}else{
		makei2c(regs, *address, *address + 1, 0, 0, 0, 1);
		*address += 1 ;
	}
}

void	i2c_write(void __iomem *regs, struct mutex *lock, WBLOCK *wblock)
{

	int		lp;
	__u32	val ;

	// ロックする
	mutex_lock(lock);
#if 0
	printk(KERN_INFO "Addr=%x(%d)\n", wblock->addr, wblock->count);
	for(lp = 0 ; lp  < wblock->count ; lp++){
		printk(KERN_INFO "%x\n", wblock->value[lp]);
	}
	printk(KERN_INFO "\n");
#endif

	blockwrite(regs, wblock);
	writel(FIFO_GO, regs + FIFO_GO_ADDR);
	//とりあえずロックしないように。
	for(lp = 0 ; lp < 100 ; lp++){
		val = readl(regs + FIFO_RESULT_ADDR);
		if(!(val & FIFO_DONE)){
			break ;
		}
		schedule_timeout_interruptible(msecs_to_jiffies(1));
	}
	mutex_unlock(lock);
}

__u32	i2c_read(void __iomem *regs, struct mutex *lock, WBLOCK *wblock, int size)
{

	int		lp;
	__u32	val ;

	// ロックする
	mutex_lock(lock);
#if 0
	printk(KERN_INFO "Addr=%x:%d:%d\n", wblock->addr, wblock->count, size);
	for(lp = 0 ; lp  < wblock->count ; lp++){
		printk(KERN_INFO "%x\n", wblock->value[lp]);
	}
	printk(KERN_INFO "\n");
#endif
	blockread(regs, wblock, size);

	writel(FIFO_GO, regs + FIFO_GO_ADDR);

	for(lp = 0 ; lp < 100 ; lp++){
		schedule_timeout_interruptible(msecs_to_jiffies(1));
		val = readl(regs + FIFO_RESULT_ADDR);
		if(!(val & FIFO_DONE)){
			break ;
		}
	}

	val = readl(regs + I2C_RESULT_ADDR);
	mutex_unlock(lock);
	return val ;
}
