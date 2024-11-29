/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#ifndef _PT1_DEV_H_
#define _PT1_DEV_H_

char *bsdev[NUM_BSDEV] = {
    "/dev/pt1video1",
    "/dev/pt1video0",
    "/dev/pt1video5",
    "/dev/pt1video4",
    "/dev/pt1video9",
    "/dev/pt1video8",
    "/dev/pt1video13",
    "/dev/pt1video12",
    "/dev/pt3video1",
    "/dev/pt3video0",
    "/dev/pt3video5",
    "/dev/pt3video4",
    "/dev/pt3video9",
    "/dev/pt3video8",
    "/dev/pt3video13",
    "/dev/pt3video12",
    "/dev/px4video0",
    "/dev/px4video1",
    "/dev/px4video4",
    "/dev/px4video5",
    "/dev/asv52201",
    "/dev/asv52200",
    "/dev/asv52205",
    "/dev/asv52204",
    "/dev/asv52209",
    "/dev/asv52208",
    "/dev/asv522013",
    "/dev/asv522012",
    "/dev/pxq3pe0",
    "/dev/pxq3pe1",
    "/dev/pxq3pe4",
    "/dev/pxq3pe5",
    "/dev/pxq3pe8",
    "/dev/pxq3pe9",
    "/dev/pxq3pe12",
    "/dev/pxq3pe13",
    "/dev/pxw3u30",  
    "/dev/pxw3u32",
    "/dev/pxs3u20",  
    "/dev/pxs3u0",  

    "/dev/px4-DTV0",
    "/dev/px4-DTV1",
    "/dev/px4-DTV4",
    "/dev/px4-DTV5",
    "/dev/px4-DTV8",
    "/dev/px4-DTV9",
    "/dev/px4-DTV12",
    "/dev/px4-DTV13",
    "/dev/px4-DTV16",
    "/dev/px4-DTV17",
    "/dev/px4-DTV20",
    "/dev/px4-DTV21",
    "/dev/px4-DTV24",
    "/dev/px4-DTV25",
    "/dev/px4-DTV28",
    "/dev/px4-DTV29",

    "/dev/px5-DTV0",
    "/dev/px5-DTV1",
    "/dev/px5-DTV2",
    "/dev/px5-DTV3",
    "/dev/px5-DTV4",
    "/dev/px5-DTV5",
    "/dev/px5-DTV6",
    "/dev/px5-DTV7",
    "/dev/px5-DTV8",
    "/dev/px5-DTV9",
    "/dev/px5-DTV10",
    "/dev/px5-DTV11",
    "/dev/px5-DTV12",
    "/dev/px5-DTV13",
    "/dev/px5-DTV14",
    "/dev/px5-DTV15",
    "/dev/px5-DTV16",
    "/dev/px5-DTV17",
    "/dev/px5-DTV18",
    "/dev/px5-DTV19",
    "/dev/px5-DTV20",
    "/dev/px5-DTV21",
    "/dev/px5-DTV22",
    "/dev/px5-DTV23",
    "/dev/px5-DTV24",
    "/dev/px5-DTV25",
    "/dev/px5-DTV26",
    "/dev/px5-DTV27",
    "/dev/px5-DTV28",
    "/dev/px5-DTV29",
    "/dev/px5-DTV30",
    "/dev/px5-DTV31",

    "/dev/pxmlt5video0",
    "/dev/pxmlt5video1",
    "/dev/pxmlt5video2",
    "/dev/pxmlt5video3",
    "/dev/pxmlt5video4",

    "/dev/pxmlt8video0",
    "/dev/pxmlt8video1",
    "/dev/pxmlt8video2",
    "/dev/pxmlt8video3",
    "/dev/pxmlt8video4",
    "/dev/pxmlt8video5",
    "/dev/pxmlt8video6",
    "/dev/pxmlt8video7",

    "/dev/isdb2056video0",

    "/dev/isdb6014video0",
    "/dev/isdb6014video1",
    "/dev/isdb6014video2",
    "/dev/isdb6014video3"
};
char *isdb_t_dev[NUM_ISDB_T_DEV] = {
    "/dev/pt1video2",
    "/dev/pt1video3",
    "/dev/pt1video6",
    "/dev/pt1video7",
    "/dev/pt1video10",
    "/dev/pt1video11",
    "/dev/pt1video14",
    "/dev/pt1video15",
    "/dev/pt3video2",
    "/dev/pt3video3",
    "/dev/pt3video6",
    "/dev/pt3video7",
    "/dev/pt3video10",
    "/dev/pt3video11",
    "/dev/pt3video14",
    "/dev/pt3video15",
    "/dev/px4video2",
    "/dev/px4video3",
    "/dev/px4video6",
    "/dev/px4video7",
    "/dev/asv52202",
    "/dev/asv52203",
    "/dev/asv52206",
    "/dev/asv52207",
    "/dev/asv522010",
    "/dev/asv522011",
    "/dev/asv522014",
    "/dev/asv522015",
    "/dev/pxq3pe2",
    "/dev/pxq3pe3",
    "/dev/pxq3pe6",
    "/dev/pxq3pe7",
    "/dev/pxq3pe10",
    "/dev/pxq3pe11",
    "/dev/pxq3pe14",
    "/dev/pxq3pe15",
    "/dev/pxw3u31", 
    "/dev/pxw3u33",
    "/dev/pxs3u21",  
    "/dev/pxs3u1",  

    "/dev/px4-DTV2",
    "/dev/px4-DTV3",
    "/dev/px4-DTV6",
    "/dev/px4-DTV7",
    "/dev/px4-DTV10",
    "/dev/px4-DTV11",
    "/dev/px4-DTV14",
    "/dev/px4-DTV15",
    "/dev/px4-DTV18",
    "/dev/px4-DTV19",
    "/dev/px4-DTV22",
    "/dev/px4-DTV23",
    "/dev/px4-DTV26",
    "/dev/px4-DTV27",
    "/dev/px4-DTV30",
    "/dev/px4-DTV31",
    
    "/dev/px5-DTV0",
    "/dev/px5-DTV1",
    "/dev/px5-DTV2",
    "/dev/px5-DTV3",
    "/dev/px5-DTV4",
    "/dev/px5-DTV5",
    "/dev/px5-DTV6",
    "/dev/px5-DTV7",
    "/dev/px5-DTV8",
    "/dev/px5-DTV9",
    "/dev/px5-DTV10",
    "/dev/px5-DTV11",
    "/dev/px5-DTV12",
    "/dev/px5-DTV13",
    "/dev/px5-DTV14",
    "/dev/px5-DTV15",
    "/dev/px5-DTV16",
    "/dev/px5-DTV17",
    "/dev/px5-DTV18",
    "/dev/px5-DTV19",
    "/dev/px5-DTV20",
    "/dev/px5-DTV21",
    "/dev/px5-DTV22",
    "/dev/px5-DTV23",
    "/dev/px5-DTV24",
    "/dev/px5-DTV25",
    "/dev/px5-DTV26",
    "/dev/px5-DTV27",
    "/dev/px5-DTV28",
    "/dev/px5-DTV29",
    "/dev/px5-DTV30",
    "/dev/px5-DTV31",

    "/dev/pxmlt5video0",
    "/dev/pxmlt5video1",
    "/dev/pxmlt5video2",
    "/dev/pxmlt5video3",
    "/dev/pxmlt5video4",

    "/dev/pxmlt8video0",
    "/dev/pxmlt8video1",
    "/dev/pxmlt8video2",
    "/dev/pxmlt8video3",
    "/dev/pxmlt8video4",
    "/dev/pxmlt8video5",
    "/dev/pxmlt8video6",
    "/dev/pxmlt8video7",

    "/dev/isdb2056video0",

    "/dev/isdb6014video0",
    "/dev/isdb6014video1",
    "/dev/isdb6014video2",
    "/dev/isdb6014video3"
};

// 変換テーブル(ISDB-T用)
// 実際にioctl()を行う値の部分はREADMEを参照の事。
// BS/CSの設定値およびスロット番号は
// http://www5e.biglobe.ne.jp/~kazu_f/digital-sat/index.htmlより取得。
//

ISDB_T_FREQ_CONV_TABLE    isdb_t_conv_table[] = {
    {   0, CHTYPE_SATELLITE, 0, "151"},  /* 151ch：BS朝日 */
    {   0, CHTYPE_SATELLITE, 1, "161"},  /* 161ch：BS-TBS */
    {   0, CHTYPE_SATELLITE, 2, "171"},  /* 171ch：BSテレ東 */
    {   1, CHTYPE_SATELLITE, 0, "191"},  /* 191ch：WOWOWプライム */
    {   1, CHTYPE_SATELLITE, 1, "236"},  /* 236ch：BSアニマックス */
    {   1, CHTYPE_SATELLITE, 2, "251"},  /* 251ch：BS釣りビジョン */
    {   2, CHTYPE_SATELLITE, 0, "192"},  /* 192ch：WOWOWライブ */
    {   2, CHTYPE_SATELLITE, 1, "193"},  /* 193ch：WOWOWシネマ */
    {   4, CHTYPE_SATELLITE, 0, "211"},  /* 211ch：BS11イレブン */
    {   4, CHTYPE_SATELLITE, 2, "222"},  /* 222ch：BS12トゥエルビ */
    {   6, CHTYPE_SATELLITE, 2, "231"},  /* 231ch：放送大学ex */
    {   6, CHTYPE_SATELLITE, 2, "232"},  /* 232ch：放送大学on */
    {   6, CHTYPE_SATELLITE, 2, "531"},  /* 531ch：放送大学ラジオ */
    {   6, CHTYPE_SATELLITE, 0, "141"},  /* 141ch：BS日テレ */
    {   6, CHTYPE_SATELLITE, 1, "181"},  /* 181ch：BSフジ */
    {   7, CHTYPE_SATELLITE, 0, "101"},  /* 101ch：NHKBS1 */
    {   7, CHTYPE_SATELLITE, 0, "102"},  /* 102ch：NHKBS1 */
    {   7, CHTYPE_SATELLITE, 1, "200"},  /* 200ch：スター・チャンネル */
    {   9, CHTYPE_SATELLITE, 0, "245"},  /* 245ch：J SPORTS 4 */
    {   9, CHTYPE_SATELLITE, 1, "242"},  /* 242ch：J SPORTS 1 */
    {   9, CHTYPE_SATELLITE, 2, "243"},  /* 243ch：J SPORTS 2 */
    {   9, CHTYPE_SATELLITE, 3, "244"},  /* 244ch：J SPORTS 3 */
    {  10, CHTYPE_SATELLITE, 0, "252"},  /* 252ch：WOWOWプラス */
    {  10, CHTYPE_SATELLITE, 1, "255"},  /* 255ch：日本映画専門ch */
    {  10, CHTYPE_SATELLITE, 2, "234"},  /* 234ch：グリーンチャンネル */
    {  11, CHTYPE_SATELLITE, 0, "256"},  /* 256ch：ディズニーch */
    {  11, CHTYPE_SATELLITE, 1, "265"},  /* 265ch：BSよしもと */
    {  11, CHTYPE_SATELLITE, 2, "263"},  /* 263ch：BSJapanext */
    {  11, CHTYPE_SATELLITE, 3, "260"},  /* 260ch：BS松竹東急 */
    {  12, CHTYPE_SATELLITE, 0, "CS2"},  /* ND2：
                                          * 296ch：TBSチャンネル1
                                          * 298ch：テレ朝チャンネル1
                                          * 299ch：テレ朝チャンネル2
                                          * 339ch：ディズニージュニア */
    {  13, CHTYPE_SATELLITE, 0, "CS4"},  /* ND4：
                                          * 250ch：スカイA
                                          * 292ch：時代劇専門チャンネル
                                          * 301ch：エンタメ〜テレ
                                          * 323ch：MTV */
    {  14, CHTYPE_SATELLITE, 0, "CS6"},  /* ND6：
                                          * 294ch：ホームドラマチャンネル
                                          * 324ch：ミュージック・エア
                                          * 329ch：歌謡ポップスチャンネル
                                          * 331ch：カートゥーン・ネットワーク
                                          * 340ch：ディスカバリーチャンネル
                                          * 341ch：アニマルプラネット
                                          * 354ch：CNNj
                                          * 363ch：囲碁・将棋チャンネル */
    {  15, CHTYPE_SATELLITE, 0, "CS8"},  /* ND8：
                                          * 055ch：ショップチャンネル
                                          * 218ch：東映チャンネル
                                          * 318ch：Mnet
                                          * 349ch：日テレNEWS24 */
    {  16, CHTYPE_SATELLITE, 0, "CS10"}, /* ND10：
                                          * 219ch：衛星劇場
                                          * 317ch：KBS World
                                          * 800ch：スポーツライブ＋
                                          * 801ch：スカチャン1 */
    {  17, CHTYPE_SATELLITE, 0, "CS12"}, /* ND12：
                                          * 254ch：GAORA SPORTS
                                          * 325ch：MUSIC ON! TV
                                          * 330ch：キッズステーション
                                          * 343ch：ナショナルジオグラフィック */
    {  18, CHTYPE_SATELLITE, 0, "CS14"}, /* ND14：
                                          * 342ch：ヒストリーチャンネル
                                          * 227ch：ザ・シネマ
                                          * 293ch：ファミリー劇場
                                          * 310ch：スーパー！ドラマTV */
    {  19, CHTYPE_SATELLITE, 0, "CS16"}, /* ND16：
                                          * 290ch：TAKARAZUKA SKY STAGE
                                          * 311ch：アクションチャンネル
                                          * 316ch：ミステリーチャンネル
                                          * 321ch：100%ヒッツ！スペースシャワーTVプラス
                                          * 333ch：アニメシアターX
                                          * 353ch：BBCワールドニュース */
    {  20, CHTYPE_SATELLITE, 0, "CS18"}, /* ND18：
                                          * 240ch：ムービープラス
                                          * 262ch：ゴルフネットワーク
                                          * 305ch：チャンネル銀河
                                          * 314ch：女性チャンネル LaLa TV */
    {  21, CHTYPE_SATELLITE, 0, "CS20"}, /* ND20：
                                          * 307ch：フジテレビONE
                                          * 308ch：フジテレビTWO
                                          * 309ch：フジテレビNEXT
                                          * 322ch：音楽・ライブ! スペースシャワーTV */
    {  22, CHTYPE_SATELLITE, 0, "CS22"}, /* ND22：
                                          * 161ch：QVC
                                          * 297ch：TBSチャンネル2
                                          * 312ch：FOX
                                          * 351ch：TBS NEWS */
    {  23, CHTYPE_SATELLITE, 0, "CS24"}, /* ND24：
                                          * 223ch：映画・チャンネルNECO
                                          * 257ch：日テレジータス
                                          * 295ch：MONDO TV
                                          * 300ch：日テレプラス */
    {   0, CHTYPE_GROUND, 0,   "1"}, {   1, CHTYPE_GROUND, 0,   "2"},
    {   2, CHTYPE_GROUND, 0,   "3"}, {   3, CHTYPE_GROUND, 0, "C13"},
    {   4, CHTYPE_GROUND, 0, "C14"}, {   5, CHTYPE_GROUND, 0, "C15"},
    {   6, CHTYPE_GROUND, 0, "C16"}, {   7, CHTYPE_GROUND, 0, "C17"},
    {   8, CHTYPE_GROUND, 0, "C18"}, {   9, CHTYPE_GROUND, 0, "C19"},
    {  10, CHTYPE_GROUND, 0, "C20"}, {  11, CHTYPE_GROUND, 0, "C21"},
    {  12, CHTYPE_GROUND, 0, "C22"}, {  13, CHTYPE_GROUND, 0,   "4"},
    {  14, CHTYPE_GROUND, 0,   "5"}, {  15, CHTYPE_GROUND, 0,   "6"},
    {  16, CHTYPE_GROUND, 0,   "7"}, {  17, CHTYPE_GROUND, 0,   "8"},
    {  18, CHTYPE_GROUND, 0,   "9"}, {  19, CHTYPE_GROUND, 0,  "10"},
    {  20, CHTYPE_GROUND, 0,  "11"}, {  21, CHTYPE_GROUND, 0,  "12"},
    {  22, CHTYPE_GROUND, 0, "C23"}, {  23, CHTYPE_GROUND, 0, "C24"},
    {  24, CHTYPE_GROUND, 0, "C25"}, {  25, CHTYPE_GROUND, 0, "C26"},
    {  26, CHTYPE_GROUND, 0, "C27"}, {  27, CHTYPE_GROUND, 0, "C28"},
    {  28, CHTYPE_GROUND, 0, "C29"}, {  29, CHTYPE_GROUND, 0, "C30"},
    {  30, CHTYPE_GROUND, 0, "C31"}, {  31, CHTYPE_GROUND, 0, "C32"},
    {  32, CHTYPE_GROUND, 0, "C33"}, {  33, CHTYPE_GROUND, 0, "C34"},
    {  34, CHTYPE_GROUND, 0, "C35"}, {  35, CHTYPE_GROUND, 0, "C36"},
    {  36, CHTYPE_GROUND, 0, "C37"}, {  37, CHTYPE_GROUND, 0, "C38"},
    {  38, CHTYPE_GROUND, 0, "C39"}, {  39, CHTYPE_GROUND, 0, "C40"},
    {  40, CHTYPE_GROUND, 0, "C41"}, {  41, CHTYPE_GROUND, 0, "C42"},
    {  42, CHTYPE_GROUND, 0, "C43"}, {  43, CHTYPE_GROUND, 0, "C44"},
    {  44, CHTYPE_GROUND, 0, "C45"}, {  45, CHTYPE_GROUND, 0, "C46"},
    {  46, CHTYPE_GROUND, 0, "C47"}, {  47, CHTYPE_GROUND, 0, "C48"},
    {  48, CHTYPE_GROUND, 0, "C49"}, {  49, CHTYPE_GROUND, 0, "C50"},
    {  50, CHTYPE_GROUND, 0, "C51"}, {  51, CHTYPE_GROUND, 0, "C52"},
    {  52, CHTYPE_GROUND, 0, "C53"}, {  53, CHTYPE_GROUND, 0, "C54"},
    {  54, CHTYPE_GROUND, 0, "C55"}, {  55, CHTYPE_GROUND, 0, "C56"},
    {  56, CHTYPE_GROUND, 0, "C57"}, {  57, CHTYPE_GROUND, 0, "C58"},
    {  58, CHTYPE_GROUND, 0, "C59"}, {  59, CHTYPE_GROUND, 0, "C60"},
    {  60, CHTYPE_GROUND, 0, "C61"}, {  61, CHTYPE_GROUND, 0, "C62"},
    {  62, CHTYPE_GROUND, 0, "C63"}, {  63, CHTYPE_GROUND, 0,  "13"},
    {  64, CHTYPE_GROUND, 0,  "14"}, {  65, CHTYPE_GROUND, 0,  "15"},
    {  66, CHTYPE_GROUND, 0,  "16"}, {  67, CHTYPE_GROUND, 0,  "17"},
    {  68, CHTYPE_GROUND, 0,  "18"}, {  69, CHTYPE_GROUND, 0,  "19"},
    {  70, CHTYPE_GROUND, 0,  "20"}, {  71, CHTYPE_GROUND, 0,  "21"},
    {  72, CHTYPE_GROUND, 0,  "22"}, {  73, CHTYPE_GROUND, 0,  "23"},
    {  74, CHTYPE_GROUND, 0,  "24"}, {  75, CHTYPE_GROUND, 0,  "25"},
    {  76, CHTYPE_GROUND, 0,  "26"}, {  77, CHTYPE_GROUND, 0,  "27"},
    {  78, CHTYPE_GROUND, 0,  "28"}, {  79, CHTYPE_GROUND, 0,  "29"},
    {  80, CHTYPE_GROUND, 0,  "30"}, {  81, CHTYPE_GROUND, 0,  "31"},
    {  82, CHTYPE_GROUND, 0,  "32"}, {  83, CHTYPE_GROUND, 0,  "33"},
    {  84, CHTYPE_GROUND, 0,  "34"}, {  85, CHTYPE_GROUND, 0,  "35"},
    {  86, CHTYPE_GROUND, 0,  "36"}, {  87, CHTYPE_GROUND, 0,  "37"},
    {  88, CHTYPE_GROUND, 0,  "38"}, {  89, CHTYPE_GROUND, 0,  "39"},
    {  90, CHTYPE_GROUND, 0,  "40"}, {  91, CHTYPE_GROUND, 0,  "41"},
    {  92, CHTYPE_GROUND, 0,  "42"}, {  93, CHTYPE_GROUND, 0,  "43"},
    {  94, CHTYPE_GROUND, 0,  "44"}, {  95, CHTYPE_GROUND, 0,  "45"},
    {  96, CHTYPE_GROUND, 0,  "46"}, {  97, CHTYPE_GROUND, 0,  "47"},
    {  98, CHTYPE_GROUND, 0,  "48"}, {  99, CHTYPE_GROUND, 0,  "49"},
    { 100, CHTYPE_GROUND, 0,  "50"}, { 101, CHTYPE_GROUND, 0,  "51"},
    { 102, CHTYPE_GROUND, 0,  "52"}, { 103, CHTYPE_GROUND, 0,  "53"},
    { 104, CHTYPE_GROUND, 0,  "54"}, { 105, CHTYPE_GROUND, 0,  "55"},
    { 106, CHTYPE_GROUND, 0,  "56"}, { 107, CHTYPE_GROUND, 0,  "57"},
    { 108, CHTYPE_GROUND, 0,  "58"}, { 109, CHTYPE_GROUND, 0,  "59"},
    { 110, CHTYPE_GROUND, 0,  "60"}, { 111, CHTYPE_GROUND, 0,  "61"},
    { 112, CHTYPE_GROUND, 0,  "62"},
    { 0, 0, 0, NULL} /* terminate */
};
#endif
