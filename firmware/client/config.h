#ifndef CONFIG_H
#define CONFIG_H


#define CLIENT_ID 1

#define ROUND_DELAY_BEATS ((CLIENT_ID - 1) * 4)

//ピン設定
#define PIN_SYNC_IN     1   // 
#define PIN_LED         13  // 拍表示LED

//シリアル通信設定
#define SERIAL_BAUD     115200  

#endif 