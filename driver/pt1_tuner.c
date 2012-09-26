/* pt1-tuner.c: A PT1 on Tuner driver for Linux. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <asm/system.h>
#endif
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include	"pt1_com.h"
#include	"pt1_pci.h"
#include	"pt1_i2c.h"
#include	"pt1_tuner.h"
#include	"pt1_tuner_data.h"

typedef	struct	_TUNER_INFO{
	int		isdb_s ;
	int		isdb_t ;
}TUNER_INFO;

TUNER_INFO	tuner_info[2] = {
	{T0_ISDB_S, T0_ISDB_T},
	{T1_ISDB_S, T1_ISDB_T}
};

typedef	struct	_isdb_t_freq_add_table{
	__u16		pos ;		// 追加するチャンネルポジション
	__u16		add_freq ;	// 追加する値
}isdb_t_freq_add_table;

isdb_t_freq_add_table	isdb_t_freq_add[10] = {
	{  7, 0x8081},				// 0～7迄
	{ 12, 0x80A1},				// 8～12迄
	{ 21, 0x8062},				// 13～21迄
	{ 39, 0x80A2},				// 22～39迄
	{ 51, 0x80E2},				// 40～51迄
	{ 59, 0x8064},				// 52～59迄
	{ 75, 0x8084},				// 60～75迄
	{ 84, 0x80a4},				// 76～84迄
	{100, 0x80C4},				// 85～100迄
	{112, 0x80E4}				// 101～112迄
};

void	settuner_reset(void __iomem *regs, int cardtype, __u32 lnb, __u32 tuner)
{
	__u32	val = TUNER_POWER_OFF;
	switch(lnb){
		case LNB_11V:
			val = (1 << BIT_LNB_DOWN);
			break ;
		case LNB_15V:
			val = (1 << BIT_LNB_UP) | (1 << BIT_LNB_DOWN);
			break ;
	}

	if(cardtype == PT1) {
		switch(tuner){
		case TUNER_POWER_ON_RESET_ENABLE:
			val |= (1 << BIT_TUNER);
			break;
		case TUNER_POWER_ON_RESET_DISABLE:
			val |= (1 << BIT_TUNER) | (1 << BIT_RESET);
			break ;
		}
	}
	else if(cardtype == PT2) {
		switch(tuner){
		case TUNER_POWER_ON_RESET_ENABLE:
			val |= (1 << BIT_TUNER)
				| (1 << BIT_33A1)
				| (1 << BIT_33A2)
				| (1 << BIT_5A_)
				| (1 << BIT_5A1)
				| (1 << BIT_5A2);
			break;
		case TUNER_POWER_ON_RESET_DISABLE:
			val |= (1 << BIT_TUNER)
				| (1 << BIT_RESET)
				| (1 << BIT_33A1)
				| (1 << BIT_33A2)
				| (1 << BIT_5A_)
				| (1 << BIT_5A1)
				| (1 << BIT_5A2);
			break ;
		}
	}
	writel(val, (regs + CFG_REGS_ADDR));
}
static	int		init_isdb_s(void __iomem *regs, int cardtype, struct mutex *lock, __u32 addr)
{

	WBLOCK	wk;
	int		lp ;
	__u32	val ;

	// ISDB-S/T初期化
	memcpy(&wk, &com_initdata, sizeof(WBLOCK));

	// 初期化１(なぜかREADなので)
	memcpy(&wk, &isdb_s_init1, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);

	if(cardtype == PT1) {
		if((val & 0xff) != 0x4c) {
			printk(KERN_INFO "PT1:ISDB-S Read(%x)\n", val);
			return -EIO ;
		}
		for(lp = 0 ; lp < PT1_MAX_ISDB_S_INIT ; lp++) {
			memcpy(&wk, isdb_s_initial_pt1[lp], sizeof(WBLOCK));
			wk.addr = addr;
			i2c_write(regs, lock, &wk);
		}
	}
	else if(cardtype == PT2) {
		if((val & 0xff) != 0x52) {
			printk(KERN_INFO "PT2:ISDB-S Read(%x)\n", val);
			return -EIO ;
		}
		for(lp = 0 ; lp < PT2_MAX_ISDB_S_INIT ; lp++) {
			memcpy(&wk, isdb_s_initial_pt2[lp], sizeof(WBLOCK));
			wk.addr = addr;
			i2c_write(regs, lock, &wk);
		}
	}

	return 0 ;
}
static	void	init_isdb_t(void __iomem *regs, int cardtype, struct mutex *lock, __u32 addr)
{
	int		lp ;
	WBLOCK	wk;

	// ISDB-S/T初期化
	if(cardtype == PT1) {
		for(lp = 0 ; lp < PT1_MAX_ISDB_T_INIT ; lp++){
			memcpy(&wk, isdb_t_initial_pt1[lp], sizeof(WBLOCK));
			wk.addr = addr;
			i2c_write(regs, lock, &wk);
		}
	}
	else if(cardtype == PT2) {
		for(lp = 0 ; lp < PT2_MAX_ISDB_T_INIT ; lp++){
			memcpy(&wk, isdb_t_initial_pt2[lp], sizeof(WBLOCK));
			wk.addr = addr;
			i2c_write(regs, lock, &wk);
		}
	}
}

int		tuner_init(void __iomem *regs, int cardtype, struct mutex *lock, int tuner_no)
{

	int		rc ;
	WBLOCK	wk;

	// ISDB-S/T初期化
	memcpy(&wk, &com_initdata, sizeof(WBLOCK));

	// 初期化(共通)
	wk.addr = tuner_info[tuner_no].isdb_t ;
	i2c_write(regs, lock, &wk);
	wk.addr = tuner_info[tuner_no].isdb_s ;
	i2c_write(regs, lock, &wk);

	rc = init_isdb_s(regs, cardtype, lock, tuner_info[tuner_no].isdb_s);
	if(rc < 0){
		return rc ;
	}
	init_isdb_t(regs, cardtype, lock, tuner_info[tuner_no].isdb_t);

	memcpy(&wk, &isdb_s_init21, sizeof(WBLOCK));
	wk.addr = tuner_info[tuner_no].isdb_s ;
	i2c_write(regs, lock, &wk);

	memcpy(&wk, &isdb_t_init17, sizeof(WBLOCK));
	wk.addr = tuner_info[tuner_no].isdb_t ;
	i2c_write(regs, lock, &wk);

	return 0 ;
}
void	set_sleepmode(void __iomem *regs, struct mutex *lock, int address, int tuner_type, int type)
{
	WBLOCK	wk;

	if(type == TYPE_WAKEUP){
		switch(tuner_type){
		case CHANNEL_TYPE_ISDB_S:
			printk(KERN_INFO "PT1:ISDB-S Wakeup\n");
			memcpy(&wk, &isdb_s_wake, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);

			memcpy(&wk, &isdb_s_wake2, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);
			break ;
		case CHANNEL_TYPE_ISDB_T:
			printk(KERN_INFO "PT1:ISDB-T Wakeup\n");
			memcpy(&wk, &isdb_t_wake, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);

			memcpy(&wk, &isdb_t_wake2, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);
			break ;
		}
	}
	if(type == TYPE_SLEEP){
		switch(tuner_type){
		case CHANNEL_TYPE_ISDB_S:
			printk(KERN_INFO "PT1:ISDB-S Sleep\n");
			memcpy(&wk, &isdb_s_sleep, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);
			break ;
		case CHANNEL_TYPE_ISDB_T:
			printk(KERN_INFO "PT1:ISDB-T Sleep\n");
			memcpy(&wk, &isdb_t_sleep, sizeof(WBLOCK));
			wk.addr = address;
			i2c_write(regs, lock, &wk);
			break ;
		}
	}
}

int		bs_frequency(void __iomem *regs, struct mutex *lock, int addr, int channel)
{
	int		lp ;
	int		tmcclock = FALSE ;
	WBLOCK	wk;
	__u32	val ;

	if(channel >= MAX_BS_CHANNEL){
		return -EIO ;
	}
	// ISDB-S PLLロック
	for(lp = 0 ; lp < MAX_BS_CHANNEL_PLL_COMMAND ; lp++){
		memcpy(&wk, bs_pll[channel].wblock[lp], sizeof(WBLOCK));
		wk.addr = addr ;
		i2c_write(regs, lock, &wk);
	}

	// PLLロック確認
	// チェック用
	for(lp = 0 ; lp < 200 ; lp++){
		memcpy(&wk, &bs_pll_lock, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 1);
		if(((val & 0xFF) != 0) && ((val & 0XFF) != 0XFF)){
			tmcclock = TRUE ;
			break ;
		}
	}

	if(tmcclock == FALSE){
		printk(KERN_INFO "PLL LOCK ERROR\n");
		return -EIO;
	}

	memcpy(&wk, &bs_tmcc_get_1, sizeof(WBLOCK));
	wk.addr = addr;
	i2c_write(regs, lock, &wk);

	tmcclock = FALSE ;

	for(lp = 0 ; lp < 200 ; lp++){
		memcpy(&wk, &bs_tmcc_get_2, sizeof(WBLOCK));
		wk.addr = addr;

		val = i2c_read(regs, lock, &wk, 1);
		if(((val & 0XFF) != 0XFF) && (!(val & 0x10))){
			tmcclock = TRUE ;
			break ;
		}
	}

	if(tmcclock == FALSE){
		printk(KERN_INFO "TMCC LOCK ERROR\n");
		return -EIO;
	}

	return 0 ;
}
int		ts_lock(void __iomem *regs, struct mutex *lock, int addr, __u16 ts_id)
{

	int		lp ;
	WBLOCK	wk;
	__u32	val ;
	union{
		__u8	ts[2];
		__u16	tsid;
	}uts_id ;

	uts_id.tsid = ts_id ;
	memcpy(&wk, &bs_set_ts_lock, sizeof(WBLOCK));
	wk.addr = addr;
	// TS-ID設定
	wk.value[1]  = uts_id.ts[1];
	wk.value[2]  = uts_id.ts[0];
	i2c_write(regs, lock, &wk);

	for(lp = 0 ; lp < 100 ; lp++){
		memcpy(&wk, &bs_get_ts_lock, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 2);
		if((val & 0xFFFF) == ts_id){
			return 0 ;
		}
	}
	printk(KERN_INFO "PT1:ERROR TS-LOCK(%x)\n", ts_id);
	return -EIO ;
}
int		bs_tune(void __iomem *regs, struct mutex *lock, int addr, int channel, ISDB_S_TMCC *tmcc)
{

	int		lp ;
	int		lp2;
	WBLOCK	wk;
	__u32	val ;
	ISDB_S_TS_ID	*tsid ;
	union{
		__u8	slot[4];
		__u32	u32slot;
	}ts_slot ;
	union{
		__u16	ts[2];
		__u32	tsid;
	}ts_id ;

	if(channel >= MAX_BS_CHANNEL){
		printk(KERN_INFO "Invalid Channel(%d)\n", channel);
		return -EIO ;
	}
	val = bs_frequency(regs, lock, addr, channel);
	if(val == -EIO){
		return val ;
	}

	tsid = &tmcc->ts_id[0] ;
	// 該当周波数のTS-IDを取得
	for(lp = 0 ; lp < (MAX_BS_TS_ID / 2) ; lp++){
		for(lp2 = 0 ; lp2 < 100 ; lp2++){
			memcpy(&wk, bs_get_ts_id[lp], sizeof(WBLOCK));
			wk.addr = addr;
			ts_id.tsid = i2c_read(regs, lock, &wk, 4);
			// TS-IDが0の場合は再取得する
			if((ts_id.ts[0] != 0) && (ts_id.ts[1] != 0)){
				break ;
			}
		}
		tsid->ts_id = ts_id.ts[1] ;
		tsid += 1;
		tsid->ts_id = ts_id.ts[0] ;
		tsid += 1;
	}

	memcpy(&wk, &bs_get_agc, sizeof(WBLOCK));
	wk.addr = addr;
	tmcc->agc = i2c_read(regs, lock, &wk, 1);

	// TS-ID別の情報を取得
	tsid = &tmcc->ts_id[0] ;
	for(lp = 0 ; lp < MAX_BS_TS_ID ; lp++, tsid += 1){
		// TS-IDなし=0XFFFF
		if(tsid->ts_id == 0xFFFF){
			continue ;
		}
		ts_lock(regs, lock, addr, tsid->ts_id);

		//スロット取得
		memcpy(&wk, &bs_get_slot, sizeof(WBLOCK));
		wk.addr = addr;
		ts_slot.u32slot = i2c_read(regs, lock, &wk, 3);
		tsid->high_mode = 0;
		tsid->low_slot  = ts_slot.slot[0] ;
		tsid->high_slot = ts_slot.slot[1] ;
		tsid->low_mode  = ts_slot.slot[2] ;
	}

	memcpy(&wk, &bs_get_clock, sizeof(WBLOCK));
	wk.addr = addr;
	tmcc->clockmargin = i2c_read(regs, lock, &wk, 1);

	memcpy(&wk, &bs_get_carrir, sizeof(WBLOCK));
	wk.addr = addr;
	tmcc->carriermargin = i2c_read(regs, lock, &wk, 1);
	return 0 ;
}
int		isdb_s_read_signal_strength(void __iomem *regs, struct mutex *lock, int addr)
{
	WBLOCK	wk;
	__u32	val ;
	__u32	val2;
	int		val3 ;

	memcpy(&wk, &bs_get_signal1, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);

	memcpy(&wk, &bs_get_signal2, sizeof(WBLOCK));
	wk.addr = addr;
	val2 = i2c_read(regs, lock, &wk, 1);
	val3 = (((val << 8) & 0XFF00) | (val2 & 0XFF));

	return val3 ;
}

__u32	getfrequency_add(__u32 channel)
{
	int		lp ;

	for(lp = 0 ; lp < 10 ; lp++){
		if(channel <= isdb_t_freq_add[lp].pos){
			return isdb_t_freq_add[lp].add_freq ;
		}
	}
	return 0 ;
}
__u32	getfrequency(__u32 channel, int addfreq)
{
	__u32	frequencyoffset = 0;
	__u32	frequencyOffset = 0;

	if (12 <= channel)
		frequencyoffset += 2;
	if (17 <= channel)
		frequencyoffset -= 2;
	if (63 <= channel){
		frequencyoffset += 2;
	}
#if 0
	return (((93 + channel * 6 + frequencyOffset) + addfreq) * 7) + 400;
#endif
	frequencyOffset = 93 + channel * 6 + frequencyoffset;
	frequencyOffset = 7 * (frequencyOffset + addfreq);
	return frequencyOffset + 400;

}
int		isdb_t_frequency(void __iomem *regs, struct mutex *lock, int addr, int channel, int addfreq)
{

	int		lp ;
	WBLOCK	wk;
	__u32	val ;
	int		tmcclock = FALSE ;
	union{
		__u8	charfreq[2];
		__u16	freq;
	}freq[2] ;

	if(channel >= MAX_ISDB_T_CHANNEL){
		return -EIO ;
	}

	freq[0].freq = getfrequency(channel, addfreq);
	freq[1].freq = getfrequency_add(channel);
	//指定周波数
	memcpy(&wk, &isdb_t_pll_base, sizeof(WBLOCK));
	wk.addr = addr ;
	// 計算した周波数を設定
	wk.value[wk.count] = freq[0].charfreq[1];
	wk.count += 1 ;
	wk.value[wk.count] = freq[0].charfreq[0];
	wk.count += 1 ;

	// 計算した周波数付加情報を設定
	wk.value[wk.count] = freq[1].charfreq[1];
	wk.count += 1 ;
	wk.value[wk.count] = freq[1].charfreq[0];
	wk.count += 1 ;

	i2c_write(regs, lock, &wk);

	for(lp = 0 ; lp < 100 ; lp++){
		memcpy(&wk, &isdb_t_pll_lock, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 1);
		if(((val & 0xFF) != 0XFF) && ((val & 0X50) == 0x50)){
			tmcclock = TRUE ;
			break ;
		}
	}
	if(tmcclock != TRUE){
		printk(KERN_INFO "PT1:ISDB-T LOCK NG(%08x)\n", val);
		return -EIO ;
	}

	memcpy(&wk, &isdb_t_check_tune, sizeof(WBLOCK));
	wk.addr = addr ;
	i2c_write(regs, lock, &wk);

	tmcclock = FALSE ;
	for(lp = 0 ; lp < 1000 ; lp++){
		memcpy(&wk, &isdb_t_tune_read, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 1);
		if(((val & 0xFF) != 0XFF) && ((val & 0X8) != 8)){
			tmcclock = TRUE ;
			break ;
		}
	}
	if(tmcclock != TRUE){
		return -EIO ;
	}
	return 0 ;
}
int		isdb_t_read_signal_strength(void __iomem *regs, struct mutex *lock, int addr)
{
	__u32	val ;
	__u32	val2;
	__u32	val3;
	WBLOCK	wk;

	memcpy(&wk, &isdb_t_signal1, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);
	printk(KERN_INFO "CN(1)Val(%x)\n", val);

	memcpy(&wk, &isdb_t_signal2, sizeof(WBLOCK));
	wk.addr = addr;
	val2 = i2c_read(regs, lock, &wk, 1);
	val3 = (((val << 8) & 0XFF00) | (val2 & 0XFF));
	return val3 ;
}
#if 0
int		isdb_t_tune(void __iomem *regs, struct mutex *lock, int addr, int channel, ISDB_T_TMCC *tmcc)
{

	int		lp ;
	int		rc ;
	int		lp2 ;
	WBLOCK	wk;
	__u32	val ;

	printk(KERN_INFO "Channel(%d) Start\n", channel);
	if(channel >= MAX_ISDB_T_CHANNEL){
		return -EIO ;
	}
	rc = isdb_t_frequency(regs, lock, addr, channel);
	if(rc < 0){
		return -EIO ;
	}
	for(lp = 0 ; lp < 100 ; lp++){
		memcpy(&wk, &isdb_t_tmcc_read_1, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 4);
		if((val & 0xFF) != 0){
			break ;
		}
	}
	printk(KERN_INFO "TMCC(1)Val(%x)\n", val);

	for(lp = 0 ; lp < 100 ; lp++){
		memcpy(&wk, &isdb_t_tmcc_read_2, sizeof(WBLOCK));
		wk.addr = addr;
		val = i2c_read(regs, lock, &wk, 4);
		if((val & 0xFF) != 0){
			break ;
		}
	}
	printk(KERN_INFO "TMCC(2)Val(%x)\n", val);

	memcpy(&wk, &isdb_t_cn_1, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);
	printk(KERN_INFO "CN(1)Val(%x)\n", val);

	memcpy(&wk, &isdb_t_cn_2, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);
	printk(KERN_INFO "CN(2)Val(%x)\n", val);

	memcpy(&wk, &isdb_t_agc_1, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);
	printk(KERN_INFO "AGC(1)Val(%x)\n", val);

	memcpy(&wk, &isdb_t_agc_2, sizeof(WBLOCK));
	wk.addr = addr;
	val = i2c_read(regs, lock, &wk, 1);
	printk(KERN_INFO "AGC(2)Val(%x)\n", val);
	return 0;
}
#endif
