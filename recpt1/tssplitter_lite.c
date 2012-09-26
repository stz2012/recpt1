/* -*- tab-width: 4; indent-tabs-mode: t -*- */
/* tssplitter_lite.c -- split TS stream.

   Copyright 2009 querulous
   Copyright 2010 Naoya OYAMA <naoya.oyama@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <fcntl.h>
#include <sys/stat.h>
#include "decoder.h"
#include "recpt1.h"
#include "tssplitter_lite.h"

/* prototypes */
static int ReadTs(splitter *sp, ARIB_STD_B25_BUFFER *sbuf);
static int AnalyzePat(splitter *sp, unsigned char *buf);
static int RecreatePat(splitter *sp, unsigned char *buf, int *pos);
static char** AnalyzeSid(char *sid);
static int AnalyzePmt(splitter *sp, unsigned char *buf, unsigned char mark);
static int GetCrc32(unsigned char *data, int len);
static int GetPid(unsigned char *data);

/**
 * サービスID解析
 */
static char** AnalyzeSid(
	char* sid)						// [in]		サービスID(カンマ区切りテキスト)
{
	int i = 0;
	char** sid_list = NULL;
	char* p;
	int CommaNum = 0;

	/* sid は次の形式の引数を許容する */
	/* 指定無し */
	/* SID[0] */
	/* SID[0],SID[1],...,SID[N-1],SID[N] */

	/*カンマの数を数える*/
	p = sid;
	while(*p != '\0')
	{
		if( *p == C_CHAR_COMMA ){
			CommaNum++;
		}
		p++;
	}

	/* sid_listの数はカンマの数+2(NULL止めするから) */
	sid_list = malloc(sizeof(char*)*(CommaNum+2));
	if ( sid_list == NULL )
	{
		fprintf(stderr, "AnalyzeSid() malloc error.\n");
		return NULL;
	}

	/* sidが空である場合 */
	p = sid;
	if ( strlen(p) == 0 )
	{
		sid_list[0] = NULL;
		return sid_list;
	}

	/* カンマ無し */
	if ( CommaNum == 0 )
	{
		sid_list[0] = sid;
		sid_list[1] = NULL;
		return sid_list;
	}

	/* カンマ区切りで複数指定時 */
	i=0;
	p = sid;
	/* 文字列端に到達するか、カンマ数が数えた数に達したら終了 */
	while((*p != '\0') || i < CommaNum)
	{
		/* 現在の処理位置をsid_list[i]にセット */
		/* このタイミングの p は
		 * ・sid先頭
		 * ・[,]の次の文字
		 * いずれかであるので p を sid_list[i] に代入してよい
		 */
		sid_list[i] = p;
		i++;

		/* 最初に現れる[,]をNULL文字で置換する */
		p = strchr(p, C_CHAR_COMMA);
		if ( p == NULL )
		{
			/* カンマが見つからない場合は最後の処理対象なので終了 */
			break;
		}
		*p = '\0';
		/* 処理位置をNULLで置換した文字の次の位置に設定する */
		p++;
	}

	/* 最後のsid_list[n]はNULLポインタで止める */
	sid_list[i] = NULL;

	i=0;
	while( sid_list[i] != NULL )
	{
		i++;
	}
#if 0
	for(i=0; sid_list[i] != NULL; i++)
	{
		printf("sid_list[%d]=[%s].\n",i, sid_list[i]);
	}
#endif
	return sid_list;
}

/**
 * 初期化処理
 */
splitter* split_startup(
	char *sid		// [in]		サービスID(引数で指定した文字列)
)
{
	splitter* sp;
	sp = malloc(sizeof(splitter));
	if ( sp == NULL )
	{
		fprintf(stderr, "split_startup malloc error.\n");
		return NULL;
	}
	memset(sp->pids, 0, sizeof(sp->pids));
	memset(sp->pmt_pids, 0, sizeof(sp->pmt_pids));

	sp->sid_list	= NULL;
	sp->pat			= NULL;
	sp->sid_list	= AnalyzeSid(sid);
	if ( sp->sid_list == NULL )
	{
		free(sp);
		return NULL;
	}
	sp->pat_count	= 0xFF;
	sp->pmt_retain = -1;
	sp->pmt_counter = 0;

	memset(sp->section_remain, 0U, sizeof(sp->section_remain));
	memset(sp->packet_seq, 0U, sizeof(sp->packet_seq));

	return sp;
}

/**
 * 落とすPIDを確定させる
 */
int split_select(
	splitter *sp,						// [in/out]		splitter構造体
	ARIB_STD_B25_BUFFER *sbuf			// [in]			入力TS
)
{
	int result;
	// TS解析
	result = ReadTs(sp, sbuf);

	return result;
}

/**
 * 終了処理
 */
void split_shutdown(splitter* sp)
{
	if ( sp != NULL ) {
		if ( sp->pat != NULL )
		{
			free(sp->pat);
			sp->pat = NULL;
		}
		if ( sp->sid_list != NULL )
		{
			free(sp->sid_list);
			sp->sid_list = NULL;
		}
		free(sp);
		sp = NULL;
	}
}

/**
 * TS 解析処理
 *
 * 対象のチャンネル番号のみの PAT の再構築と出力対象 PID の抽出を行う
 */
static int ReadTs(splitter *sp, ARIB_STD_B25_BUFFER *sbuf)
#if 0
	unsigned char **pat,				// [out]	PAT 情報（再構築後）
	unsigned char* pids,				// [out]	出力対象 PID 情報
	char** sid_list,					// [in]		出力対象サービス ID のリスト
	unsigned char* pmt_pids,			// [in]		出力対象PIDのPMT PID
	,			// [in]		pt1_drvの入力TS
	int* pmt_retain,						// [in]		残すべきPMTの数
	int* pmt_counter					// [out]	残したPMTの数
#endif
{
	int length = sbuf->size;
	int pid;
	int result = TSS_ERROR;
	int index;
	int analyze_result = 0;

	index = 0;
	while(length - index - LENGTH_PACKET > 0) {
		pid = GetPid(sbuf->data + index + 1);
		// PAT
		if(0x0000 == pid) {
			result = AnalyzePat(sp, sbuf->data + index);
			if(result != TSS_SUCCESS) {
				/* 下位の関数内部でmalloc error発生 */
				return result;
			}
		}

		// PMT
		/* 残すpmt_pidである場合には、pmtに書かれている
		 * 残すべきPCR/AUDIO/VIDEO PIDを取得する */
		if(sp->pmt_pids[pid] == 1) {
			/* この中にはPMT毎に一度しか入らないようにしておく */
			analyze_result = AnalyzePmt(sp, sbuf->data + index, 1);
			if(TSS_SUCCESS == analyze_result) {
				sp->pmt_pids[pid]++;
				sp->pmt_counter += 1;
				*(sbuf->data + index + 1) = 0xff;
				*(sbuf->data + index + 2) = 0xff;
			}
		}
		/* 録画する全てのPMTについて、中にあるPCR/AUDIO/VIDEOのPIDを
		 * 得る */
		/* pmt_counter と pmt_retain が一致する場合に条件は満たされる */
		if(sp->pmt_counter == sp->pmt_retain) {
			result = TSS_SUCCESS;
			break;
		}
		else {
			result = TSS_ERROR;
		}
		index += LENGTH_PACKET;
	}

	return(result);
}

static int RescanPID(splitter *splitter, unsigned char *buf)
{
	int result = TSS_NULL;
	int i;

	// clear
	if (splitter->pmt_counter == splitter->pmt_retain) {
	    memcpy(splitter->pids, splitter->pmt_pids, sizeof(splitter->pids));
	    splitter->pmt_counter = 0;
		memset(splitter->section_remain, 0U, sizeof(splitter->section_remain));
		memset(splitter->packet_seq, 0U, sizeof(splitter->packet_seq));

		fprintf(stderr, "Rescan PID \n");
	}

	if (TSS_SUCCESS == AnalyzePmt(splitter, buf, 2)) {
	    splitter->pmt_counter += 1;
	}

 	if (splitter->pmt_retain == splitter->pmt_counter) {
	    result = TSS_SUCCESS;
		for (i = 0; MAX_PID > i; i++) {
		    if (splitter->pids[i] > 0) {
			    splitter->pids[i] -= 1;
		    }
		}
		fprintf(stderr, "Rescan PID End\n");
	}

	return result;
}
/**
 * TS 分離処理
 */
int split_ts(
	splitter *splitter,					// [in]		splitterパラメータ
	ARIB_STD_B25_BUFFER *sbuf,			// [in]		入力TS
	splitbuf_t *dbuf							// [out]	出力TS
)
{
	int pid;
	unsigned char *sptr, *dptr;
	int s_offset = 0;
	int d_offset = 0;
	int result = TSS_SUCCESS;
	int pmts = 0;
	int version = 0;

	/* 初期化 */
	dbuf->size = 0;
	if (sbuf->size < 0) {
		return TSS_ERROR;
	}

	sptr = sbuf->data;
	dptr = dbuf->buffer;

	while(sbuf->size > s_offset) {
		pid = GetPid(sptr + s_offset + 1);
		switch(pid) {

		// PAT
		case 0x0000:
			// 巡回カウンタカウントアップ
			if(0xFF == splitter->pat_count) {
				splitter->pat_count = splitter->pat[3];
			}
			else {
				splitter->pat_count += 1;
				if(0 == splitter->pat_count % 0x10) {
					splitter->pat_count -= 0x10;
				}
			}
			splitter->pat[3] = splitter->pat_count;

			memcpy(dptr + d_offset, splitter->pat, LENGTH_PACKET);
			d_offset += LENGTH_PACKET;
			dbuf->size += LENGTH_PACKET;
			break;
		default:
		    if(0 != splitter->pmt_pids[pid]) {
			    //PMT
			    if ((sptr + s_offset)[1] & 0x40) {		// PES開始インジケータ
				    // バージョンチェック
				    for(pmts = 0; splitter->pmt_retain > pmts; pmts++) {
					    if (splitter->pmt_version[pmts].pid == pid) {
						  version = splitter->pmt_version[pmts].version;
						  break;
						}
					}
					if((version != ((sptr + s_offset)[10] & 0x3e))
					   ||(splitter->pmt_retain != splitter->pmt_counter)) {
					    // 再チェック
					    result = RescanPID(splitter, sptr + s_offset);
					}
				}
				else {
				    if (splitter->pmt_retain != splitter->pmt_counter) {
					    // 再チェック
					    result = RescanPID(splitter, sptr + s_offset);
					}
				}
			}
			/* pids[pid] が 1 は残すパケットなので書き込む */
			if(0 != splitter->pids[pid]) {
				memcpy(dptr + d_offset, sptr + s_offset, LENGTH_PACKET);
				d_offset += LENGTH_PACKET;
				dbuf->size += LENGTH_PACKET;
			}
			break;
		} /* switch */

		s_offset += LENGTH_PACKET;
	}

	return result;
}

/**
 * PAT 解析処理
 *
 * PAT を解析し、出力対象チャンネルが含まれているかチェックを行い、PAT を再構築する
 */
static int AnalyzePat(splitter *sp, unsigned char *buf)
#if 0
	splitter *sp
		unsigned char** pat,				// [out]	PAT 情報（再構築後）
		unsigned char* pids,				// [out]	出力対象 PID 情報
		char** sid_list,					// [in]		出力対象サービス ID のリスト
		unsigned char* pmt_pids,			// [out]	サービス ID に対応する PMT の PID
		int* pmt_retain						// [out]	残すPMTの数

	unsigned char* buf,					// [in]		読み込んだバッファ
#endif
{
	int pos[MAX_PID];
	int service_id;
	int i, j, k;
	int size = 0;
	int pid;
	int result = TSS_SUCCESS;
	char **p;
	int sid_found = FALSE;
	int avail_sids[MAX_SERVICES];

	unsigned char *pat = sp->pat;
	unsigned char *pids = sp->pids;
	char **sid_list = sp->sid_list;
	unsigned char *pmt_pids = sp->pmt_pids;

	char chosen_sid[512];
	chosen_sid[0] = '\0';

	if(pat == NULL) {
		/* 初期化 */
		sp->pmt_retain = 0;
		memset(pos, 0, sizeof(pos));
		size = buf[7];

		/* prescan SID/PMT */
		for(i = 13, j = 0; i < (size + 8) - 4; i = i + 4) {

			pid = GetPid(&buf[i+2]);
			if(pid == 0x0010)
				continue;

			avail_sids[j] = (buf[i] << 8) + buf[i+1];
			sp->avail_pmts[j] = pid;
			j++;
		}
		sp->num_pmts = j;

		// 対象チャンネル判定
		/* size + 8 = パケット全長 */
		/* 最終 4 バイトはCRCなので飛ばす */
		for(i = 13; i < (size + 8) - 4; i = i + 4) {

			pid = GetPid(&buf[i+2]);
			if(pid == 0x0010)
				continue;

			service_id = (buf[i] << 8) + buf[i+1];
			p = sid_list;

			while(*p) {
				if(service_id == atoi(*p)) {
					/* 録画対象の pmt_pids は 1 とする */
					/* 録画対象の pmt の pids は 1 とする */
					pid = GetPid(&buf[i + 2]);
					*(pmt_pids+pid) = 1;
					*(pids+pid) = 1;
					pos[pid] = i;
					sid_found = TRUE;
					sp->pmt_version[sp->pmt_retain].pid = pid;
					sp->pmt_retain += 1;
					sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					p++;
					continue;
				}
				else if(!strcasecmp(*p, "hd") || !strcasecmp(*p, "sd1")) {
					/* hd/sd1 指定時には1番目のサービスを保存する */
					if(service_id == avail_sids[0]) {
						pid = GetPid(&buf[i + 2]);
						*(pmt_pids+pid) = 1;
						*(pids+pid) = 1;
						pos[pid] = i;
						sid_found = TRUE;
						sp->pmt_version[sp->pmt_retain].pid = pid;
						sp->pmt_retain += 1;
						sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					}
					p++;
					continue;
				}
				else if(!strcasecmp(*p, "sd2")) {
					/* sd2 指定時には2番目のサービスを保存する */
					if(service_id == avail_sids[1]) {
						pid = GetPid(&buf[i + 2]);
						*(pmt_pids+pid) = 1;
						*(pids+pid) = 1;
						pos[pid] = i;
						sid_found = TRUE;
						sp->pmt_version[sp->pmt_retain].pid = pid;
						sp->pmt_retain += 1;
						sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					}
					p++;
					continue;
				}
				else if(!strcasecmp(*p, "sd3")) {
					/* sd3 指定時には3番目のサービスを保存する */
					if(service_id == avail_sids[2]) {
						pid = GetPid(&buf[i + 2]);
						*(pmt_pids+pid) = 1;
						*(pids+pid) = 1;
						pos[pid] = i;
						sid_found = TRUE;
						sp->pmt_version[sp->pmt_retain].pid = pid;
						sp->pmt_retain += 1;
						sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					}
					p++;
					continue;
				}
				else if(!strcasecmp(*p, "1seg")) {
					/* 1seg 指定時には PMTPID=0x1FC8 のサービスを保存する */
					pid = GetPid(&buf[i + 2]);
					if(pid == 0x1FC8) {
						*(pmt_pids+pid) = 1;
						*(pids+pid) = 1;
						pos[pid] = i;
						sid_found = TRUE;
						sp->pmt_version[sp->pmt_retain].pid = pid;
						sp->pmt_retain += 1;
						sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					}
					p++;
					continue;
				}
				else if(!strcasecmp(*p, "all")) {
					/* all指定時には全保存する */
					pid = GetPid(&buf[i + 2]);
					*(pmt_pids+pid) = 1;
					*(pids+pid) = 1;
					pos[pid] = i;
					sid_found = TRUE;
					sp->pmt_version[sp->pmt_retain].pid = pid;
					sp->pmt_retain += 1;
					sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
					break;
				}
				else if(!strcasecmp(*p, "epg")) {
					/* epg抽出に必要なPIDのみを保存する */
					sid_found    = TRUE;
					*(pids+0x11) = 1;
					*(pids+0x12) = 1;
					*(pids+0x23) = 1;
					*(pids+0x29) = 1;
					break;
				}

				p++;
			} /* while */
		}

		/* if sid has been specified but no sid found, fall back to all */
		if(*sid_list && !sid_found) {
			for(i = 13; i < (size + 8) - 4; i = i + 4) {

				pid = GetPid(&buf[i+2]);
				if(pid==0x0010)
					continue;

				service_id = (buf[i] << 8) + buf[i+1];
				pid = GetPid(&buf[i + 2]);
				*(pmt_pids+pid) = 1;
				*(pids+pid) = 1;
				pos[pid] = i;
				sid_found = TRUE;
				sp->pmt_version[sp->pmt_retain].pid = pid;
				sp->pmt_retain += 1;
				sprintf(chosen_sid, "%s %d", *chosen_sid ? chosen_sid : "", service_id);
			}
		}

		/* print SIDs */
		fprintf(stderr, "Available sid = ");
		for(k=0; k < sp->num_pmts; k++)
			fprintf(stderr, "%d ", avail_sids[k]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Chosen sid    =%s\n", chosen_sid);

#if 1
		/* print PMTs */
		fprintf(stderr, "Available PMT = ");
		for(k=0; k < sp->num_pmts; k++)
			fprintf(stderr, "0x%x ", sp->avail_pmts[k]);
		fprintf(stderr, "\n");
#endif

		// PAT 再構築
		result = RecreatePat(sp, buf, pos);
#if 0
		int tc;
		for(tc=0; tc<188; tc++)
			fprintf(stderr, "%02x ", *(sp->pat+tc));
#endif
	}

	return(result);
}

/**
 * PAT 再構築処理
 *
 * PMT から出力対象チャンネル以外のチャンネル情報を削除し、PAT を再構築する
 */
static int RecreatePat(splitter *sp, unsigned char *buf, int *pos)
#if 0
	splitter *sp						// [in/out]
		unsigned char** pat,			// [out]	PAT 情報（再構築後）
		unsigned char* pids,			// [out]	出力対象 PID 情報

	unsigned char* buf,					// [in]		読み込んだバッファ
	int *pos							// [in]		取得対象 PMT のバッファ中の位置
#endif
{
	unsigned char y[LENGTH_CRC_DATA];
	int crc;
	int i;
	int j;
	int pos_i;
	int pid_num = 0;

	// CRC 計算のためのデータ
	{
		// チャンネルによって変わらない部分
		for (i = 0; i < LENGTH_PAT_HEADER - 4; i++)
		{
			y[i] = buf[i + 5];
		}

		// NIT
		y[LENGTH_PAT_HEADER-4] = 0x00;
		y[LENGTH_PAT_HEADER-3] = 0x00;
		y[LENGTH_PAT_HEADER-2] = 0xe0;
		y[LENGTH_PAT_HEADER-1] = 0x10;

		// チャンネルによって変わる部分
		for (i = 0; i < MAX_PID; i++)
		{
			if(pos[i] != 0)
			{
				/* buf[pos_i] を y にコピー(抽出したPIDの数) */
				pos_i = pos[i];
				for (j = 0; j < 4; j++)
				{
					y[LENGTH_PAT_HEADER + ((4*pid_num) + j)] = buf[pos_i + j];
				}
				pid_num++;
			}
		}
	}
	/* パケットサイズ計算 */
	y[2] = pid_num * 4 + 0x0d;
	// CRC 計算
	crc = GetCrc32(y, LENGTH_PAT_HEADER + pid_num*4);

	// PAT 再構成
	sp->pat = (unsigned char*)malloc(LENGTH_PACKET);
	if(sp->pat == NULL)
	{
		fprintf(stderr, "RecreatePat() malloc error.\n");
		return(TSS_NULL);
	}
	memset(sp->pat, 0xFF, LENGTH_PACKET);
	for (i = 0; i < 5; i++)
	{
		(sp->pat)[i] = buf[i];
	}
	for (i = 0; i < LENGTH_PAT_HEADER + pid_num*4; i++)
	{
		(sp->pat)[i + 5] = y[i];
	}
	(sp->pat)[5 + LENGTH_PAT_HEADER + pid_num*4] = (crc >> 24) & 0xFF;
	(sp->pat)[6 + LENGTH_PAT_HEADER + pid_num*4] = (crc >> 16) & 0xFF;
	(sp->pat)[7 + LENGTH_PAT_HEADER + pid_num*4] = (crc >>  8) & 0xFF;
	(sp->pat)[8 + LENGTH_PAT_HEADER + pid_num*4] = (crc      ) & 0xFF;

	return(TSS_SUCCESS);
}

/**
 * PMT 解析処理
 *
 * PMT を解析し、保存対象の PID を特定する
 */
static int AnalyzePmt(splitter *sp, unsigned char *buf, unsigned char mark)
#if 0
	unsigned char* buf,					// [in]		読み込んだバッファ
	unsigned char* pids					// [out]	出力対象 PID 情報
#endif
{
	unsigned char Nall;
	unsigned char N;
	int pcr;
	int epid;
	int pid;
	int retry_count = 0;
	int count;
	int payload_offset;	// offset to payload

	pid = GetPid(&buf[1]);
	if (buf[1] & 0x40) {		// PES開始インジケータ
		sp->section_remain[pid] = ((buf[6] & 0x0F) << 8) + buf[7] + 3;	// セクションサイズ取得(ヘッダ込)
		payload_offset = 5;

		for (count = 0; sp->pmt_retain > count; count++) {
		    if (sp->pmt_version[count].pid  == pid) {
                sp->pmt_version[count].version = buf[10] & 0x3e;
			}
		}
		// PCR, 番組情報が先頭からはみ出ることはないだろう

		// PCR
		pcr = GetPid(&buf[payload_offset + 8]);
		sp->pids[pcr] = mark;

		// ECM
		N = ((buf[payload_offset + 10] & 0x0F) << 8) + buf[payload_offset + 11] + payload_offset + 12;	// ES情報開始点
		int p = payload_offset + 12;

		while(p < N) {
			uint32_t ca_pid;
			uint32_t tag;
			uint32_t len;

			tag = buf[p];
			len = buf[p+1];
			p += 2;

			if(tag == 0x09 && len >= 4 && p+len <= N) {
				ca_pid = ((buf[p+2] << 8) | buf[p+3]) & 0x1fff;
				sp->pids[ca_pid] = mark;
			}
			p += len;
		}
	}
	else {
		if (sp->section_remain[pid] == 0) return TSS_ERROR;								// セクション先頭が飛んでいる
		if ((buf[3] & 0x0F) != ((sp->packet_seq[pid] + 1) & 0x0F)) return TSS_ERROR;	// パケットカウンタが飛んだ
		payload_offset = 4;
		N = payload_offset;
	}
	sp->packet_seq[pid] = buf[3] & 0x0F;				// 巡回カウンタ
	
	Nall = sp->section_remain[pid];
	if(Nall > LENGTH_PACKET - payload_offset)
		Nall = LENGTH_PACKET - payload_offset;

	// ES PID
	while (N <= Nall + payload_offset - 5)
	{
		// ストリーム種別が 0x0D（type D）は出力対象外
		if (0x0D != buf[N])
		{
			epid = GetPid(&buf[N + 1]);

			sp->pids[epid] = mark;
		}
		N += 4 + (((buf[N + 3]) & 0x0F) << 8) + buf[N + 4] + 1;
		retry_count++;
		if(retry_count > Nall) {
			return TSS_ERROR;
		}
	}
	sp->section_remain[pid] -= Nall;

	if (sp->section_remain[pid] > 0)
		return SECTION_CONTINUE;
	else
		return TSS_SUCCESS;
}

/**
 * CRC 計算
 */
static int GetCrc32(
	unsigned char* data,				// [in]		CRC 計算対象データ
	int len)							// [in]		CRC 計算対象データ長
{
	int crc;
	int i, j;
	int c;
	int bit;

	crc = 0xFFFFFFFF;
	for (i = 0; i < len; i++)
	{
		char x;
		x = data[i];

		for (j = 0; j < 8; j++)
		{

			bit = (x >> (7 - j)) & 0x1;

			c = 0;
			if (crc & 0x80000000)
			{
				c = 1;
			}

			crc = crc << 1;

			if (c ^ bit)
			{
				crc ^= 0x04C11DB7;
			}

			crc &= 0xFFFFFFFF;
		}
	}

	return crc;
}

/**
 * PID 取得
 */
static int GetPid(
	unsigned char* data)				// [in]		取得対象データのポインタ
{
	return ((data[0] & 0x1F) << 8) + data[1];
}
