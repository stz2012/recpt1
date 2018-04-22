/* -*- tab-width: 4; indent-tabs-mode: t -*- */
/* pt1-pci.c: A PT1 on PCI bus driver for Linux. */
#include "version.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/mutex.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <asm/system.h>
#endif
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#include <linux/freezer.h>
#else
#define set_freezable()
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
typedef struct pm_message {
        int event;
} pm_message_t;
#endif
#endif
#include <linux/kthread.h>
#include <linux/dma-mapping.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/ioctl.h>

#include	"pt1_com.h"
#include	"pt1_pci.h"
#include	"pt1_tuner.h"
#include	"pt1_i2c.h"
#include	"pt1_tuner_data.h"
#include	"pt1_ioctl.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,8,0)
#define __devinit
#define __devinitdata
#define __devexit
#define __devexit_p
#endif

/* These identify the driver base version and may not be removed. */
static char version[] __devinitdata =
DRV_NAME ".c: " DRV_VERSION " " DRV_RELDATE " \n";

MODULE_AUTHOR("Tomoaki Ishikawa tomy@users.sourceforge.jp and Yoshiki Yazawa yaz@honeyplanet.jp");
#define	DRIVER_DESC		"PCI earthsoft PT1/2 driver"
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static int debug = 7;			/* 1 normal messages, 0 quiet .. 7 verbose. */
static int lnb = 0;			/* LNB OFF:0 +11V:1 +15V:2 */

module_param(debug, int, 0);
module_param(lnb, int, 0);
MODULE_PARM_DESC(debug, "debug level (1-2)");
MODULE_PARM_DESC(debug, "LNB level (0:OFF 1:+11V 2:+15V)");

#define VENDOR_EARTHSOFT 0x10ee
#define PCI_PT1_ID 0x211a
#define PCI_PT2_ID 0x222a

static struct pci_device_id pt1_pci_tbl[] = {
	{ VENDOR_EARTHSOFT, PCI_PT1_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ VENDOR_EARTHSOFT, PCI_PT2_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pt1_pci_tbl);
#define		DEV_NAME	"pt1video"

#define		PACKET_SIZE			188		// 1パケット長
#define		MAX_READ_BLOCK	4			// 1度に読み出す最大DMAバッファ数
#define		MAX_PCI_DEVICE		128		// 最大64枚
#define		DMA_SIZE	4096			// DMAバッファサイズ
#define		DMA_RING_SIZE	128			// number of DMA RINGS
#define		DMA_RING_MAX	511			// number of DMA entries in a RING(1023はNGで511まで)
#define		CHANNEL_DMA_SIZE	(2*1024*1024)	// 地デジ用(16Mbps)
#define		BS_CHANNEL_DMA_SIZE	(4*1024*1024)	// BS用(32Mbps)
#define		READ_SIZE	(16*DMA_SIZE)

typedef	struct	_DMA_CONTROL{
	dma_addr_t	ring_dma[DMA_RING_MAX] ;	// DMA情報
	__u32		*data[DMA_RING_MAX];
}DMA_CONTROL;

typedef	struct	_PT1_CHANNEL	PT1_CHANNEL;

typedef	struct	_pt1_device{
	unsigned long	mmio_start ;
	__u32			mmio_len ;
	void __iomem		*regs;
	struct mutex		lock ;
	dma_addr_t		ring_dma[DMA_RING_SIZE] ;	// DMA情報
	void			*dmaptr[DMA_RING_SIZE] ;
	struct	task_struct	*kthread;
	dev_t			dev ;
	int			card_number;
	__u32			base_minor ;
	struct	cdev	cdev[MAX_CHANNEL];
	wait_queue_head_t	dma_wait_q ;// for poll on reading
	DMA_CONTROL		*dmactl[DMA_RING_SIZE];
	PT1_CHANNEL		*channel[MAX_CHANNEL];
	int			cardtype;
} PT1_DEVICE;

typedef	struct	_MICRO_PACKET{
	char	data[3];
	char	head ;
}MICRO_PACKET;

struct	_PT1_CHANNEL{
	__u32			valid ;			// 使用中フラグ
	__u32			address ;		// I2Cアドレス
	__u32			channel ;		// チャネル番号
	int			type ;			// チャネルタイプ
	__u32			packet_size ;	// パケットサイズ
	__u32			drop ;			// パケットドロップ数
	struct mutex		lock ;			// CH別mutex_lock用
	__u32			size ;			// DMAされたサイズ
	__u32			maxsize ;		// DMA用バッファサイズ
	__u32			bufsize ;		// チャネルに割り振られたサイズ
	__u32			overflow ;		// オーバーフローエラー発生
	__u32			counetererr ;	// 転送カウンタ１エラー
	__u32			transerr ;		// 転送エラー
	__u32			minor ;			// マイナー番号
	__u8			*buf;			// CH別受信メモリ
	__u32			pointer;
	__u8			req_dma ;		// 溢れたチャネル
	__u8			packet_buf[PACKET_SIZE] ;		// 溢れたチャネル
	PT1_DEVICE		*ptr ;			// カード別情報
	wait_queue_head_t	wait_q ;	// for poll on reading
};

// I2Cアドレス(video0, 1 = ISDB-S) (video2, 3 = ISDB-T)
int		i2c_address[MAX_CHANNEL] = {T0_ISDB_S, T1_ISDB_S, T0_ISDB_T, T1_ISDB_T};
int		real_channel[MAX_CHANNEL] = {0, 2, 1, 3};
int		channeltype[MAX_CHANNEL] = {CHANNEL_TYPE_ISDB_S, CHANNEL_TYPE_ISDB_S,
									CHANNEL_TYPE_ISDB_T, CHANNEL_TYPE_ISDB_T};

static	PT1_DEVICE	*device[MAX_PCI_DEVICE];
static struct class	*pt1video_class;

#define		PT1MAJOR	251
#define		DRIVERNAME	"pt1video"

static	void	reset_dma(PT1_DEVICE *dev_conf)
{

	int		lp ;
	__u32	addr ;
	int		ring_pos = 0;
	int		data_pos = 0 ;
	__u32	*dataptr ;

	// データ初期化
	for(ring_pos = 0 ; ring_pos < DMA_RING_SIZE ; ring_pos++){
		for(data_pos = 0 ; data_pos < DMA_RING_MAX ; data_pos++){
			dataptr = (dev_conf->dmactl[ring_pos])->data[data_pos];
			dataptr[(DMA_SIZE / sizeof(__u32)) - 2] = 0;
		}
	}
	// 転送カウンタをリセット
	writel(0x00000010, dev_conf->regs);
	// 転送カウンタをインクリメント
	for(lp = 0 ; lp < DMA_RING_SIZE ; lp++){
		writel(0x00000020, dev_conf->regs);
	}

	addr = (int)dev_conf->ring_dma[0] ;
	addr >>= 12 ;
	// DMAバッファ設定
	writel(addr, dev_conf->regs + DMA_ADDR);
	// DMA開始
	writel(0x0c000040, dev_conf->regs);

}
static	int		pt1_thread(void *data)
{
	PT1_DEVICE	*dev_conf = data ;
	PT1_CHANNEL	*channel ;
	int		ring_pos = 0;
	int		data_pos = 0 ;
	int		lp ;
	int		chno ;
	int		dma_channel ;
	int		packet_pos ;
	__u32	*dataptr ;
	__u32	*curdataptr ;
	__u32	val ;
	union	mpacket{
		__u32	val ;
		MICRO_PACKET	packet ;
	}micro;

	set_freezable();
	reset_dma(dev_conf);
	printk(KERN_INFO "pt1_thread run\n");

	for(;;){
		if(kthread_should_stop()){
			break ;
		}

		for(;;){
			dataptr = (dev_conf->dmactl[ring_pos])->data[data_pos];
			// データあり？
			if(dataptr[(DMA_SIZE / sizeof(__u32)) - 2] == 0){
				break ;
			}
			micro.val = *dataptr ;
			curdataptr = dataptr ;
			data_pos += 1 ;
			for(lp = 0 ; lp < (DMA_SIZE / sizeof(__u32)) ; lp++, dataptr++){
				micro.val = *dataptr ;
				dma_channel = ((micro.packet.head >> 5) & 0x07);
				//チャネル情報不正
				if(dma_channel > MAX_CHANNEL){
					printk(KERN_ERR "DMA Channel Number Error(%d)\n", dma_channel);
					continue ;
				}
				chno = real_channel[(((micro.packet.head >> 5) & 0x07) - 1)];
				packet_pos = ((micro.packet.head >> 2) & 0x07);
				channel = dev_conf->channel[chno] ;
				//  エラーチェック
				if((micro.packet.head & MICROPACKET_ERROR)){
					val = readl(dev_conf->regs);
					if((val & BIT_RAM_OVERFLOW)){
						channel->overflow += 1 ;
					}
					if((val & BIT_INITIATOR_ERROR)){
						channel->counetererr += 1 ;
					}
					if((val & BIT_INITIATOR_WARNING)){
						channel->transerr += 1 ;
					}
					// 初期化して先頭から
					reset_dma(dev_conf);
					ring_pos = data_pos = 0 ;
					break ;
				}
				// 未使用チャネルは捨てる
				if(channel->valid == FALSE){
					continue ;
				}
				mutex_lock(&channel->lock);
				// あふれたら読み出すまで待つ
				while(1){
					if(channel->size >= (channel->maxsize - PACKET_SIZE - 4)){
						// 該当チャンネルのDMA読みだし待ちにする
						wake_up(&channel->wait_q);
						channel->req_dma = TRUE ;
						mutex_unlock(&channel->lock);
						// タスクに時間を渡す為中断
						wait_event_timeout(dev_conf->dma_wait_q, (channel->req_dma == FALSE),
											msecs_to_jiffies(500));
						mutex_lock(&channel->lock);
						channel->drop += 1 ;
					}else{
						break ;
					}
				}
				// 先頭で、一時バッファに残っている場合
				if((micro.packet.head & 0x02) &&  (channel->packet_size != 0)){
					channel->packet_size = 0 ;
				}
				// データコピー
				channel->packet_buf[channel->packet_size]   = micro.packet.data[2];
				channel->packet_buf[channel->packet_size+1] = micro.packet.data[1];
				channel->packet_buf[channel->packet_size+2] = micro.packet.data[0];
				channel->packet_size += 3;

				// パケットが出来たらコピーする
				if(channel->packet_size >= PACKET_SIZE){
					if (channel->pointer + channel->size >= channel->maxsize) {
						// リングバッファの境界を越えていてリングバッファの先頭に戻っている場合
						// channel->pointer + channel->size - channel->maxsize でリングバッファ先頭からのアドレスになる
						memcpy(&channel->buf[channel->pointer + channel->size - channel->maxsize], channel->packet_buf, PACKET_SIZE);
					} else if (channel->pointer + channel->size + PACKET_SIZE > channel->maxsize) {
						// リングバッファの境界をまたぐように書き込まれる場合
						// リングバッファの境界まで書き込み
						__u32 tmp_size = channel->maxsize - (channel->pointer + channel->size);
						memcpy(&channel->buf[channel->pointer + channel->size], channel->packet_buf, tmp_size);
						// 先頭に戻って書き込み
						memcpy(channel->buf, &channel->packet_buf[tmp_size], PACKET_SIZE - tmp_size);
					} else {
						// リングバッファ内で収まる場合
						// 通常の書き込み
						memcpy(&channel->buf[channel->pointer + channel->size], channel->packet_buf, PACKET_SIZE);
					}
					channel->size += PACKET_SIZE ;
					channel->packet_size = 0 ;
				}
				mutex_unlock(&channel->lock);
			}
			curdataptr[(DMA_SIZE / sizeof(__u32)) - 2] = 0;

			if(data_pos >= DMA_RING_MAX){
				data_pos = 0;
				ring_pos += 1 ;
				// DMAリングが変わった場合はインクリメント
				writel(0x00000020, dev_conf->regs);
				if(ring_pos >= DMA_RING_SIZE){
					ring_pos = 0 ;
				}
			}

			// 頻度を落す(wait until READ_SIZE)
			for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
				channel = dev_conf->channel[real_channel[lp]] ;
				if((channel->size >= READ_SIZE) && (channel->valid == TRUE)){
					wake_up(&channel->wait_q);
				}
			}
		}
		schedule_timeout_interruptible(msecs_to_jiffies(100));
	}
	return 0 ;
}
static int pt1_open(struct inode *inode, struct file *file)
{
	int		major = imajor(inode);
	int		minor = iminor(inode);
	int		lp ;
	int		lp2 ;
	PT1_CHANNEL	*channel ;

	for(lp = 0 ; lp < MAX_PCI_DEVICE ; lp++){
		if(device[lp] == NULL){
			return -EIO ;
		}

		if(MAJOR(device[lp]->dev) == major &&
		   device[lp]->base_minor <= minor &&
		   device[lp]->base_minor + MAX_CHANNEL > minor) {

			mutex_lock(&device[lp]->lock);
			for(lp2 = 0 ; lp2 < MAX_CHANNEL ; lp2++){
				channel = device[lp]->channel[lp2] ;
				if(channel->minor == minor){
					if(channel->valid == TRUE){
						mutex_unlock(&device[lp]->lock);
						return -EIO ;
					}

					/* wake tuner up */
					set_sleepmode(channel->ptr->regs, &channel->lock,
								  channel->address, channel->type,
								  TYPE_WAKEUP);
					schedule_timeout_interruptible(msecs_to_jiffies(100));

					channel->drop  = 0 ;
					channel->valid = TRUE ;
					channel->overflow = 0 ;
					channel->counetererr = 0 ;
					channel->transerr = 0 ;
					channel->packet_size = 0 ;
					file->private_data = channel;
					mutex_lock(&channel->lock);
					// データ初期化
					channel->size = 0 ;
					mutex_unlock(&channel->lock);
					mutex_unlock(&device[lp]->lock);
					return 0 ;
				}
			}
		}
	}
	return -EIO;
}
static int pt1_release(struct inode *inode, struct file *file)
{
	PT1_CHANNEL	*channel = file->private_data;

	mutex_lock(&channel->ptr->lock);
	SetStream(channel->ptr->regs, channel->channel, FALSE);
	channel->valid = FALSE ;
	printk(KERN_INFO "(%d:%d)Drop=%08d:%08d:%08d:%08d\n", imajor(inode), iminor(inode), channel->drop,
						channel->overflow, channel->counetererr, channel->transerr);
	channel->overflow = 0 ;
	channel->counetererr = 0 ;
	channel->transerr = 0 ;
	channel->drop = 0 ;
	// 停止している場合は起こす
	if(channel->req_dma == TRUE){
		channel->req_dma = FALSE ;
		wake_up(&channel->ptr->dma_wait_q);
	}
	mutex_unlock(&channel->ptr->lock);

	/* send tuner to sleep */
	set_sleepmode(channel->ptr->regs, &channel->lock,
				  channel->address, channel->type, TYPE_SLEEP);
	schedule_timeout_interruptible(msecs_to_jiffies(100));

	return 0;
}

static ssize_t pt1_read(struct file *file, char __user *buf, size_t cnt, loff_t * ppos)
{
	PT1_CHANNEL	*channel = file->private_data;
	__u32	size ;
	unsigned long dummy;

	// READ_SIZE単位で起こされるのを待つ(CPU負荷対策)
	if(channel->size < READ_SIZE){
		wait_event_timeout(channel->wait_q, (channel->size >= READ_SIZE),
							msecs_to_jiffies(500));
	}
	mutex_lock(&channel->lock);
	if(!channel->size){
		size = 0 ;
	}else{
		__u32 tmp_size = 0;
		if (cnt < channel->size) {
			// バッファにあるデータより小さい読み込みの場合
			size = cnt;
		} else {
			// バッファにあるデータ以上の読み込みの場合
			size = channel->size;
		}
		if (channel->maxsize <= size + channel->pointer) {
			// リングバッファの境界を越える場合
			tmp_size = channel->maxsize - channel->pointer;
			// 境界までコピー
			dummy = copy_to_user(buf, &channel->buf[channel->pointer], tmp_size);
			// 残りをコピー
			dummy = copy_to_user(&buf[tmp_size], channel->buf, size - tmp_size);
			channel->pointer = size - tmp_size;
		} else {
			// 普通にコピー
			dummy = copy_to_user(buf, &channel->buf[channel->pointer], size);
			channel->pointer += size;
		}
		channel->size -= size;
	}
	// 読み終わったかつ使用しているのがが4K以下
	if(channel->req_dma == TRUE){
		channel->req_dma = FALSE ;
		wake_up(&channel->ptr->dma_wait_q);
	}
	mutex_unlock(&channel->lock);
	return size ;
}
static	int		SetFreq(PT1_CHANNEL *channel, FREQUENCY *freq)
{

	switch(channel->type){
		case CHANNEL_TYPE_ISDB_S:
			{
				ISDB_S_TMCC		tmcc ;
				if(bs_tune(channel->ptr->regs,
						&channel->ptr->lock,
						channel->address,
						freq->frequencyno,
						&tmcc) < 0){
					return -EIO ;
				}

#if 0
				printk(KERN_INFO "clockmargin = (%x)\n", (tmcc.clockmargin & 0xFF));
				printk(KERN_INFO "carriermargin  = (%x)\n", (tmcc.carriermargin & 0xFF));
				{
					int lp;
					for(lp = 0 ; lp < MAX_BS_TS_ID ; lp++){
						if(tmcc.ts_id[lp].ts_id == 0xFFFF){
							continue ;
						}
						printk(KERN_INFO "Slot(%d:%x)\n", lp, tmcc.ts_id[lp].ts_id);
						printk(KERN_INFO "mode (low/high) = (%x:%x)\n",
							   tmcc.ts_id[lp].low_mode, tmcc.ts_id[lp].high_mode);
						printk(KERN_INFO "slot (low/high) = (%x:%x)\n",
							   tmcc.ts_id[lp].low_slot,
							   tmcc.ts_id[lp].high_slot);
					}
				}
#endif
				ts_lock(channel->ptr->regs,
						&channel->ptr->lock,
						channel->address,
						tmcc.ts_id[freq->slot].ts_id);
			}
			break ;
		case CHANNEL_TYPE_ISDB_T:
			{
				if(isdb_t_frequency(channel->ptr->regs,
						&channel->ptr->lock,
						channel->address,
						freq->frequencyno, freq->slot) < 0){
					return -EINVAL ;
				}
			}
	}
	return 0 ;
}

static int count_used_bs_tuners(PT1_DEVICE *device)
{
	int count = 0;
	int i;

	for(i=0; i<MAX_CHANNEL; i++) {
		if(device && device->channel[i] &&
		   device->channel[i]->type == CHANNEL_TYPE_ISDB_S &&
		   device->channel[i]->valid)
			count++;
	}

	printk(KERN_INFO "used bs tuners on %p = %d\n", device, count);
	return count;
}

static long pt1_do_ioctl(struct file  *file, unsigned int cmd, unsigned long arg0)
{
	PT1_CHANNEL	*channel = file->private_data;
	int		signal;
	unsigned long	dummy;
	void		*arg = (void *)arg0;
	int		lnb_eff, lnb_usr;
	char *voltage[] = {"0V", "11V", "15V"};
	int count;

	switch(cmd){
		case SET_CHANNEL:
			{
				FREQUENCY	freq ;
				dummy = copy_from_user(&freq, arg, sizeof(FREQUENCY));
				return SetFreq(channel, &freq);
			}
		case START_REC:
			SetStream(channel->ptr->regs, channel->channel, TRUE);
			return 0 ;
		case STOP_REC:
			SetStream(channel->ptr->regs, channel->channel, FALSE);
			schedule_timeout_interruptible(msecs_to_jiffies(100));
			return 0 ;
		case GET_SIGNAL_STRENGTH:
			switch(channel->type){
				case CHANNEL_TYPE_ISDB_S:
					signal = isdb_s_read_signal_strength(channel->ptr->regs,
												&channel->ptr->lock,
												channel->address);
					break ;
				case CHANNEL_TYPE_ISDB_T:
					signal = isdb_t_read_signal_strength(channel->ptr->regs,
												&channel->ptr->lock, channel->address);
					break ;
			}
			dummy = copy_to_user(arg, &signal, sizeof(int));
			return 0 ;
		case LNB_ENABLE:
			count = count_used_bs_tuners(channel->ptr);
			if(count <= 1) {
				lnb_usr = (int)arg0;
				lnb_eff = lnb_usr ? lnb_usr : lnb;
				settuner_reset(channel->ptr->regs, channel->ptr->cardtype, lnb_eff, TUNER_POWER_ON_RESET_DISABLE);
				printk(KERN_INFO "PT1:LNB on %s\n", voltage[lnb_eff]);
			}
			return 0 ;
		case LNB_DISABLE:
			count = count_used_bs_tuners(channel->ptr);
			if(count <= 1) {
				settuner_reset(channel->ptr->regs, channel->ptr->cardtype, LNB_OFF, TUNER_POWER_ON_RESET_DISABLE);
				printk(KERN_INFO "PT1:LNB off\n");
			}
			return 0 ;
	}
	return -EINVAL;
}

static long pt1_unlocked_ioctl(struct file  *file, unsigned int cmd, unsigned long arg0)
{
	PT1_CHANNEL	*channel = file->private_data;
	long ret;

	mutex_lock(&channel->lock);
	ret = pt1_do_ioctl(file, cmd, arg0);
	mutex_unlock(&channel->lock);

	return ret;
}

static long pt1_compat_ioctl(struct file  *file, unsigned int cmd, unsigned long arg0)
{
	long ret;
	/* should do 32bit <-> 64bit conversion here? --yaz */
	ret = pt1_unlocked_ioctl(file, cmd, arg0);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int pt1_ioctl(struct inode *inode, struct file  *file, unsigned int cmd, unsigned long arg0)
{
	int ret;
	ret = (int)pt1_do_ioctl(file, cmd, arg0);
	return ret;
}
#endif

/*
*/
static const struct file_operations pt1_fops = {
	.owner		=	THIS_MODULE,
	.open		=	pt1_open,
	.release	=	pt1_release,
	.read		=	pt1_read,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.ioctl		=	pt1_ioctl,
#else
	.unlocked_ioctl		=	pt1_unlocked_ioctl,
	.compat_ioctl		=	pt1_compat_ioctl,
#endif
	.llseek		=	no_llseek,
};

int		pt1_makering(struct pci_dev *pdev, PT1_DEVICE *dev_conf)
{
	int		lp ;
	int		lp2 ;
	DMA_CONTROL		*dmactl;
	__u32	*dmaptr ;
	__u32	addr  ;
	__u32	*ptr ;

	//DMAリング作成
	for(lp = 0 ; lp < DMA_RING_SIZE ; lp++){
		ptr = dev_conf->dmaptr[lp];
		if(lp ==  (DMA_RING_SIZE - 1)){
			addr = (__u32)dev_conf->ring_dma[0];
		}else{
			addr = (__u32)dev_conf->ring_dma[(lp + 1)];
		}
		addr >>= 12 ;
		memcpy(ptr, &addr, sizeof(__u32));
		ptr += 1 ;

		dmactl = dev_conf->dmactl[lp];
		for(lp2 = 0 ; lp2 < DMA_RING_MAX ; lp2++){
			dmaptr = pci_alloc_consistent(pdev, DMA_SIZE, &dmactl->ring_dma[lp2]);
			if(dmaptr == NULL){
				printk(KERN_INFO "PT1:DMA ALLOC ERROR\n");
				return -1 ;
			}
			dmactl->data[lp2] = dmaptr ;
			// DMAデータエリア初期化
			dmaptr[(DMA_SIZE / sizeof(__u32)) - 2] = 0 ;
			addr = (__u32)dmactl->ring_dma[lp2];
			addr >>= 12 ;
			memcpy(ptr, &addr, sizeof(__u32));
			ptr += 1 ;
		}
	}
	return 0 ;
}
int		pt1_dma_init(struct pci_dev *pdev, PT1_DEVICE *dev_conf)
{
	int		lp ;
	void	*ptr ;

	for(lp = 0 ; lp < DMA_RING_SIZE ; lp++){
		ptr = pci_alloc_consistent(pdev, DMA_SIZE, &dev_conf->ring_dma[lp]);
		if(ptr == NULL){
			printk(KERN_INFO "PT1:DMA ALLOC ERROR\n");
			return -1 ;
		}
		dev_conf->dmaptr[lp] = ptr ;
	}

	return pt1_makering(pdev, dev_conf);
}
int		pt1_dma_free(struct pci_dev *pdev, PT1_DEVICE *dev_conf)
{

	int		lp ;
	int		lp2 ;

	for(lp = 0 ; lp < DMA_RING_SIZE ; lp++){
		if(dev_conf->dmaptr[lp] != NULL){
			pci_free_consistent(pdev, DMA_SIZE,
								dev_conf->dmaptr[lp], dev_conf->ring_dma[lp]);
			for(lp2 = 0 ; lp2 < DMA_RING_MAX ; lp2++){
				if((dev_conf->dmactl[lp])->data[lp2] != NULL){
					pci_free_consistent(pdev, DMA_SIZE,
										(dev_conf->dmactl[lp])->data[lp2],
										(dev_conf->dmactl[lp])->ring_dma[lp2]);
				}
			}
		}
	}
	return 0 ;
}
static int __devinit pt1_pci_init_one (struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	int			rc ;
	int			lp ;
	int			minor ;
	u16			cmd ;
	PT1_DEVICE	*dev_conf ;
	PT1_CHANNEL	*channel ;
	int i;
	struct resource *dummy;

	rc = pci_enable_device(pdev);
	if (rc)
		return rc;
	rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (rc) {
		printk(KERN_ERR "PT1:DMA MASK ERROR");
		return rc;
	}

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MASTER)) {
		printk(KERN_INFO "Attempting to enable Bus Mastering\n");
		pci_set_master(pdev);
		pci_read_config_word(pdev, PCI_COMMAND, &cmd);
		if (!(cmd & PCI_COMMAND_MASTER)) {
			printk(KERN_ERR "Bus Mastering is not enabled\n");
			return -EIO;
		}
	}
	printk(KERN_INFO "Bus Mastering Enabled.\n");

	dev_conf = kzalloc(sizeof(PT1_DEVICE), GFP_KERNEL);
	if(!dev_conf){
		printk(KERN_ERR "PT1:out of memory !");
		return -ENOMEM ;
	}
	for (i = 0; i < DMA_RING_SIZE; i++) {
		dev_conf->dmactl[i] = kzalloc(sizeof(DMA_CONTROL), GFP_KERNEL);
		if(!dev_conf->dmactl[i]){
			int j;
			for (j = 0; j < i; j++) {
				kfree(dev_conf->dmactl[j]);
			}
			kfree(dev_conf);
			printk(KERN_ERR "PT1:out of memory !");
			return -ENOMEM ;
		}
	}

	switch(ent->device) {
	case PCI_PT1_ID:
		dev_conf->cardtype = PT1;
		break;
	case PCI_PT2_ID:
		dev_conf->cardtype = PT2;
		break;
	default:
		break;
	}

	// PCIアドレスをマップする
	dev_conf->mmio_start = pci_resource_start(pdev, 0);
	dev_conf->mmio_len = pci_resource_len(pdev, 0);
	dummy = request_mem_region(dev_conf->mmio_start, dev_conf->mmio_len, DEV_NAME);
	if (!dummy) {
		printk(KERN_ERR "PT1:cannot request iomem  (0x%llx).\n", (unsigned long long) dev_conf->mmio_start);
		goto out_err_regbase;
	}

	dev_conf->regs = ioremap(dev_conf->mmio_start, dev_conf->mmio_len);
	if (!dev_conf->regs){
		printk(KERN_ERR "pt1:Can't remap register area.\n");
		goto out_err_regbase;
	}
	// 初期化処理
	if(xc3s_init(dev_conf->regs, dev_conf->cardtype)){
		printk(KERN_ERR "Error xc3s_init\n");
		goto out_err_fpga;
	}
	// チューナリセット
	settuner_reset(dev_conf->regs, dev_conf->cardtype, LNB_OFF, TUNER_POWER_ON_RESET_ENABLE);
	schedule_timeout_interruptible(msecs_to_jiffies(100));

	settuner_reset(dev_conf->regs, dev_conf->cardtype, LNB_OFF, TUNER_POWER_ON_RESET_DISABLE);
	schedule_timeout_interruptible(msecs_to_jiffies(100));
	mutex_init(&dev_conf->lock);

	// Tuner 初期化処理
	for(lp = 0 ; lp < MAX_TUNER ; lp++){
		rc = tuner_init(dev_conf->regs, dev_conf->cardtype, &dev_conf->lock, lp);
		if(rc < 0){
			printk(KERN_ERR "Error tuner_init\n");
			goto out_err_fpga;
		}
	}
	// 初期化完了
	for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
		set_sleepmode(dev_conf->regs, &dev_conf->lock,
						i2c_address[lp], channeltype[lp], TYPE_SLEEP);

		schedule_timeout_interruptible(msecs_to_jiffies(100));
	}
	rc = alloc_chrdev_region(&dev_conf->dev, 0, MAX_CHANNEL, DEV_NAME);
	if(rc < 0){
		goto out_err_fpga;
	}

	// 初期化
	init_waitqueue_head(&dev_conf->dma_wait_q);

	minor = MINOR(dev_conf->dev) ;
	dev_conf->base_minor = minor ;
	for(lp = 0 ; lp < MAX_PCI_DEVICE ; lp++){
		printk(KERN_INFO "PT1:device[%d]=%p\n", lp, device[lp]);
		if(device[lp] == NULL){
			device[lp] = dev_conf ;
			dev_conf->card_number = lp;
			break ;
		}
	}
	for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
		cdev_init(&dev_conf->cdev[lp], &pt1_fops);
		dev_conf->cdev[lp].owner = THIS_MODULE;
		cdev_add(&dev_conf->cdev[lp],
			 MKDEV(MAJOR(dev_conf->dev), (MINOR(dev_conf->dev) + lp)), 1);
		channel = kzalloc(sizeof(PT1_CHANNEL), GFP_KERNEL);
		if(!channel){
			printk(KERN_ERR "PT1:out of memory !");
			return -ENOMEM ;
		}

		// 共通情報
		mutex_init(&channel->lock);
		// 待ち状態を解除
		channel->req_dma = FALSE ;
		// マイナー番号設定
		channel->minor = MINOR(dev_conf->dev) + lp ;
		// 対象のI2Cデバイス
		channel->address = i2c_address[lp] ;
		channel->type = channeltype[lp] ;
		// 実際のチューナ番号
		channel->channel = real_channel[lp] ;
		channel->ptr = dev_conf ;
		channel->size = 0 ;
		dev_conf->channel[lp] = channel ;

		init_waitqueue_head(&channel->wait_q);

		switch(channel->type){
			case CHANNEL_TYPE_ISDB_T:
				channel->maxsize = CHANNEL_DMA_SIZE ;
				channel->buf = vmalloc(CHANNEL_DMA_SIZE);
				channel->pointer = 0;
				break ;
			case CHANNEL_TYPE_ISDB_S:
				channel->maxsize = BS_CHANNEL_DMA_SIZE ;
				channel->buf = vmalloc(BS_CHANNEL_DMA_SIZE);
				channel->pointer = 0;
				break ;
		}
		if(channel->buf == NULL){
			goto out_err_v4l;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
		printk(KERN_INFO "PT1:card_number = %d\n",
		       dev_conf->card_number);
		device_create(pt1video_class,
			      NULL,
			      MKDEV(MAJOR(dev_conf->dev),
				    (MINOR(dev_conf->dev) + lp)),
			      NULL,
			      "pt1video%u",
			      MINOR(dev_conf->dev) + lp +
			      dev_conf->card_number * MAX_CHANNEL);
#else
		device_create(pt1video_class,
			      NULL,
			      MKDEV(MAJOR(dev_conf->dev),
				    (MINOR(dev_conf->dev) + lp)),
			      "pt1video%u",
			      MINOR(dev_conf->dev) + lp +
			      dev_conf->card_number * MAX_CHANNEL);
#endif

#if 0
		dev_conf->vdev[lp] = video_device_alloc();
		memcpy(dev_conf->vdev[lp], &pt1_template, sizeof(pt1_template));
		video_set_drvdata(dev_conf->vdev[lp], channel);
		video_register_device(dev_conf->vdev[lp], VFL_TYPE_GRABBER, -1);
#endif
	}

	if(pt1_dma_init(pdev, dev_conf) < 0){
		goto out_err_dma;
	}
	dev_conf->kthread = kthread_run(pt1_thread, dev_conf, "pt1");
	pci_set_drvdata(pdev, dev_conf);
	return 0;

out_err_dma:
	pt1_dma_free(pdev, dev_conf);
out_err_v4l:
	for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
		if(dev_conf->channel[lp] != NULL){
			if(dev_conf->channel[lp]->buf != NULL){
				vfree(dev_conf->channel[lp]->buf);
			}
			kfree(dev_conf->channel[lp]);
		}
	}
out_err_fpga:
	writel(0xb0b0000, dev_conf->regs);
	writel(0, dev_conf->regs + CFG_REGS_ADDR);
	iounmap(dev_conf->regs);
	release_mem_region(dev_conf->mmio_start, dev_conf->mmio_len);
	for (i = 0; i < DMA_RING_SIZE; i++) {
		kfree(dev_conf->dmactl[i]);
	}
	kfree(dev_conf);
out_err_regbase:
	return -EIO;

}

static void __devexit pt1_pci_remove_one(struct pci_dev *pdev)
{

	int		lp ;
	__u32	val ;
	PT1_DEVICE	*dev_conf = (PT1_DEVICE *)pci_get_drvdata(pdev);
	int		i;

	if(dev_conf){
		if(dev_conf->kthread) {
			kthread_stop(dev_conf->kthread);
			dev_conf->kthread = NULL;
		}

		// DMA終了
		writel(0x08080000, dev_conf->regs);
		for(lp = 0 ; lp < 10 ; lp++){
			val = readl(dev_conf->regs);
			if(!(val & (1 << 6))){
				break ;
			}
			schedule_timeout_interruptible(msecs_to_jiffies(100));
		}
		pt1_dma_free(pdev, dev_conf);
		for(lp = 0 ; lp < MAX_CHANNEL ; lp++){
			if(dev_conf->channel[lp] != NULL){
				cdev_del(&dev_conf->cdev[lp]);
				vfree(dev_conf->channel[lp]->buf);
				kfree(dev_conf->channel[lp]);
			}
			device_destroy(pt1video_class,
				       MKDEV(MAJOR(dev_conf->dev),
					     (MINOR(dev_conf->dev) + lp)));
		}

		unregister_chrdev_region(dev_conf->dev, MAX_CHANNEL);
		writel(0xb0b0000, dev_conf->regs);
		writel(0, dev_conf->regs + CFG_REGS_ADDR);
		settuner_reset(dev_conf->regs, dev_conf->cardtype, LNB_OFF, TUNER_POWER_OFF);
		release_mem_region(dev_conf->mmio_start, dev_conf->mmio_len);
		iounmap(dev_conf->regs);
		for (i = 0; i < DMA_RING_SIZE; i++) {
			kfree(dev_conf->dmactl[i]);
		}
		device[dev_conf->card_number] = NULL;
		kfree(dev_conf);
	}
	pci_set_drvdata(pdev, NULL);
}
#ifdef CONFIG_PM

static int pt1_pci_suspend (struct pci_dev *pdev, pm_message_t state)
{
	return 0;
}

static int pt1_pci_resume (struct pci_dev *pdev)
{
	return 0;
}

#endif /* CONFIG_PM */


static struct pci_driver pt1_driver = {
	.name		= DRV_NAME,
	.probe		= pt1_pci_init_one,
	.remove		= __devexit_p(pt1_pci_remove_one),
	.id_table	= pt1_pci_tbl,
#ifdef CONFIG_PM
	.suspend	= pt1_pci_suspend,
	.resume		= pt1_pci_resume,
#endif /* CONFIG_PM */

};


static int __init pt1_pci_init(void)
{
	printk(KERN_INFO "%s", version);
	pt1video_class = class_create(THIS_MODULE, DRIVERNAME);
	if (IS_ERR(pt1video_class))
		return PTR_ERR(pt1video_class);
	return pci_register_driver(&pt1_driver);
}


static void __exit pt1_pci_cleanup(void)
{
	pci_unregister_driver(&pt1_driver);
	class_destroy(pt1video_class);
}

module_init(pt1_pci_init);
module_exit(pt1_pci_cleanup);
