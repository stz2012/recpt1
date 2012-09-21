#ifndef		__PT1_TUNER_H__
#define		__PT1_TUNER_H__
/***************************************************************************/
/* チューナ状態定義                                                        */
/***************************************************************************/
// SLEEPモード設定
enum	{
	TYPE_SLEEP,
	TYPE_WAKEUP
};

// チューナパワーモード設定
enum {
	BIT_TUNER,
	BIT_LNB_UP,
	BIT_LNB_DOWN,
	BIT_RESET,
	BIT_33A1,
	BIT_33A2,
	BIT_5A_,
	BIT_5A1,
	BIT_5A2
};

// LNBパワー設定
enum{
	LNB_OFF,						// LNB OFF
	LNB_11V,						// +11 V
	LNB_15V							// +15 V
};

enum{								// 電源／ハードウェアリセット
	TUNER_POWER_OFF,				// オフ／イネーブル
	TUNER_POWER_ON_RESET_ENABLE,	// オン／イネーブル
	TUNER_POWER_ON_RESET_DISABLE	// オン／ディセーブル
};

enum {
    PT1 = 0,
    PT2,
};

/***************************************************************************/
/* チューナ状態定義                                                        */
/***************************************************************************/
#define		MAX_BS_TS_ID		8			// TS-ID取得最大値
#define		MAX_ISDB_T_INFO		3			// 地デジ階層情報数
#define		MAX_ISDB_T_INFO_LEN		2			// 地デジ階層情報数
/***************************************************************************/
/* ISDB-S状態定義                                                         */
/***************************************************************************/
typedef struct  _ISDB_S_CH_TABLE{
	int		channel ;		// 入力チャンネル番号
	int		real_chno ;		// 実際のテーブル番号
	int		slotno ;		// スロット番号
}ISDB_S_CH_TABLE ;

/***************************************************************************/
/* ISDB-S状態定義                                                         */
/***************************************************************************/
typedef	struct	_ISDB_S_TS_ID{
	__u16	ts_id ;			// TS-ID
	__u16	dmy ;			// PAD
	__u8	low_mode ;		// 低階層 モード
	__u8	low_slot ;		// 低階層 スロット数
	__u8	high_mode ;		// 高階層 モード
	__u8	high_slot ;		// 高階層 スロット数
}ISDB_S_TS_ID;
typedef	struct	_ISDB_S_TMCC{
	ISDB_S_TS_ID	ts_id[MAX_BS_TS_ID];	// 相対TS番号nに対するTS ID情報
#if 0
	__u32	indicator;				// 変更指示 (5ビット)
	__u32	emergency;				// 起動制御信号 (1ビット)
	__u32	uplink;					// アップリンク制御情報 (4ビット)
	__u32	ext;					// 拡張フラグ (1ビット)
	__u32	extdata[2];				// 拡張領域 (61ビット)
#endif
	__u32	agc ;					// AGC
	__u32	clockmargin ;			// クロック周波数誤差
	__u32	carriermargin ;			// キャリア周波数誤差
}ISDB_S_TMCC;

// 階層情報
typedef	struct	_ISDB_T_INFO{
	__u32	mode;				// キャリア変調方式 (3ビット)
	__u32	rate;				// 畳込み符号化率 (3ビット)
	__u32	interleave;			// インターリーブ長 (3ビット)
	__u32	segment; 			// セグメント数 (4ビット)
}ISDB_T_INFO;

typedef	struct	_ISDB_T_TMCC {
#if 0
	__u32	sysid;		// システム識別 (2ビット)
	__u32	indicator;	// 伝送パラメータ切り替え指標 (4ビット)
	__u32	emergency;	// 緊急警報放送用起動フラグ (1ビット)
#endif
	ISDB_T_INFO	info[MAX_ISDB_T_INFO];
#if 0
						// カレント情報
	__u32	partial;	// 部分受信フラグ (1ビット)
	__u32	Phase;		// 連結送信位相補正量 (3ビット)
	__u32	Reserved;	// リザーブ (12ビット)
#endif
	__u32	cn[2] ;					// CN
	__u32	agc ;					// AGC
	__u32	clockmargin ;			// クロック周波数誤差
	__u32	carriermargin ;			// キャリア周波数誤差
}ISDB_T_TMCC;
/***************************************************************************/
/* チューナ状態定義                                                        */
/***************************************************************************/
extern	void	settuner_reset(void __iomem *, int, __u32, __u32);
extern	int		tuner_init(void __iomem *, int, struct mutex *, int);
extern	void	set_sleepmode(void __iomem *, struct mutex *, int, int, int);

extern	int		bs_tune(void __iomem *, struct mutex *, int, int, ISDB_S_TMCC *);
extern  int     ts_lock(void __iomem *, struct mutex *, int, __u16);

extern	int		isdb_t_tune(void __iomem *, struct mutex *, int, int, ISDB_T_TMCC *);
extern	int		isdb_t_frequency(void __iomem *, struct mutex *, int, int, int);
extern	int		isdb_s_read_signal_strength(void __iomem *, struct mutex *, int);
extern	int		isdb_t_read_signal_strength(void __iomem *, struct mutex *, int);

#endif
