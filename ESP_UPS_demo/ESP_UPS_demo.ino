/*
 
  esp—ups
  模拟群晖不断电服务器协议 实现自制ups 接入群晖系统
  
  https://github.com/bilibilifmk/ESP_UPS_demo
  
  需要依赖wifi_link_tool ：https://github.com/bilibilifmk/wifi_link_tool 

  by：bilibilifmk
  
*/


#define FS_CONFIG
//激活文件系统模式配网
#include <wifi_link_tool.h>
//引入wifilinktool头文件

//#include <ESP8266WiFi.h>
#include <algorithm>
#include <cstdint>
#include <cstddef>

String usp_mfr = "keai";               /*厂商*/
String usp_model = "keai NO.1";        /*型号*/
String usp_charge = "100";             /*电池电量*/
String usp_charge_low = "1440";        /*电池预估续航时间*/
String usp_status = "OL CHRG";         /*UPS状态 OL：在线 CHRG：充电 LB：电量不足     */



/*尽量不要修改 */
static constexpr uint8_t max_client { 4 };         /*最大客户端数量 */
static constexpr uint32_t t_delay { 1000 };         /*丢弃开头 */
static WiFiServer server { 3493 };                  /*TCP端口号 */
static WiFiClient serverClients[max_client];
static uint32_t connection_time[max_client];
/*尽量不要修改 */



unsigned long previousMillis = 0;
const long interval = 60000;
String tmps = "";
void setup() {


  rstb = 0;
  //重置io
  stateled = 2;
  //指示灯io
  Hostname = "ESP_USP";
  //设备名称 允许中文名称 不建议太长
  wxscan = true;
  
  Version = "0.0.1";
  
  Serial.begin(115200);

  load();

  webServer.on("/model", getmodel);
  webServer.on("/charge", getcharge);
  webServer.on("/chargelow", getchargelow);
  server.begin();
  server.setNoDelay(true);

}


void getchargelow() {
  usp_charge_low = webServer.arg("vlue");
  Serial.println(usp_charge_low);
  webServer.send(200, "text/plain", "ojbk");
}


void getcharge() {
  usp_charge = webServer.arg("vlue");
  Serial.println(usp_charge);
  webServer.send(200, "text/plain", "ojbk");
}

void getmodel() {
  if (webServer.arg("model") == "AC") {
    usp_status = "OL CHRG";
    Serial.println(usp_status);
    webServer.send(200, "text/plain", "ojbk");
  }
  if (webServer.arg("model") == "DC") {
    usp_status = "OB DISCHRG";
    Serial.println(usp_status);
    webServer.send(200, "text/plain", "ojbk");
  }
  if (webServer.arg("model") == "LB") {
    usp_status = "OB LB DISCHRG";
    Serial.println(usp_status);
    webServer.send(200, "text/plain", "ojbk");
  }
}

void loop() {


  pant();

  if (server.hasClient()) {
    for (uint8_t i { 0 }; i < max_client; ++i) {

      if (!serverClients[i].connected()) {
        serverClients[i].stop();

        serverClients[i] = server.available();
        connection_time[i] = ::millis();
        serverClients[i].write("\xff\xfb\x01", 3);
        serverClients[i].flush();
        serverClients[i].write("\xff\xfe\x01", 3);
        serverClients[i].flush();
        serverClients[i].write("\xff\xfe\x22", 3);
        serverClients[i].flush();

        serverClients[i].write("欢迎使用UPS.\r\n");
        serverClients[i].flush();
        return;
      }
    }
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  //接收client数据转发到串口
  for (uint8_t i { 0 }; i < max_client; ++i) {
    if (serverClients[i].connected()) {
      while (serverClients[i].available()) {
        const uint32_t dt {
          ::millis() - connection_time[i]
        };
        const uint8_t tmp { static_cast<uint8_t>(serverClients[i].read()) };
        if (dt < t_delay && tmp == 0xff) {

          while (serverClients[i].available() < 2) {
            ::delay(0);
          }
          serverClients[i].read();
          serverClients[i].read();
        } else {
          Serial.write(tmp);
          char buf = (char)tmp;
          tmps += (String)buf;
          //Serial.println(tmps);
          if (tmps.indexOf("LIST UPS") != -1) {
            tmps = "";
            Serial.println("捕获");
            char lsups[] = "BEGIN LIST UPS\n\rUPS usp \"Description unavailable\"\n\rEND LIST UPS\n\r";
            for (uint8_t k { 0 }; k < max_client; ++k) {
              if (serverClients[k].connected()) {
                serverClients[k].write(lsups, sizeof(lsups));
                ::delay(0);
              }
            }

          }
          if (tmps.indexOf("LIST VAR ups") != -1  ) {
            tmps = "";
            Serial.println("捕获");
            String lsups = "BEGIN LIST VAR ups\n\rVAR ups battery.charge \"" + usp_charge + "\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"" + usp_charge_low + "\"\n\rVAR ups battery.type \"PbAc\"\n\rVAR ups device.mfr \"" + usp_mfr + "\"\n\rVAR ups device.model \"" + usp_model + "\"\n\rVAR ups device.type \"ups\"\n\rVAR ups outlet.id \"0\"\n\rVAR ups outlet.switchable \"yes\"\n\rVAR ups ups.delay.shutdown \"20\"\n\rVAR ups ups.delay.start \"30\"\n\rVAR ups ups.load \"20\"\n\rVAR ups ups.mfr \"" + usp_mfr + "\"\n\rVAR ups ups.model \"" + usp_model + "\"\n\rVAR ups ups.status \"" + usp_status + "\"\n\rEND LIST VAR ups\n\r";
            char lsups_buf[lsups.length() + 1] ;
            lsups.toCharArray(lsups_buf, lsups.length() + 1);

            for (uint8_t k { 0 }; k < max_client; ++k) {
              if (serverClients[k].connected()) {
                serverClients[k].write(lsups_buf, sizeof(lsups_buf));
                ::delay(0);
              }
            }
          }

          if (tmps.indexOf("GET VAR ups ups.status") != -1) {
            tmps = "";
            Serial.println("捕获");
            String lsups;
            if (usp_status == "OL CHRG") {
              lsups = "VAR ups ups.status \"OL\"\n\r";
            } else
            {
              //lsups = "BEGIN LIST VAR ups\n\rVAR ups battery.charge \"" + usp_charge + "\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"" + usp_charge_low + "\"\n\rVAR ups battery.type \"PbAc\"\n\rVAR ups device.mfr \"" + usp_mfr + "\"\n\rVAR ups device.model \"" + usp_model + "\"\n\rVAR ups device.type \"ups\"\n\rVAR ups outlet.id \"0\"\n\rVAR ups outlet.switchable \"yes\"\n\rVAR ups ups.delay.shutdown \"20\"\n\rVAR ups ups.delay.start \"30\"\n\rVAR ups ups.load \"20\"\n\rVAR ups ups.mfr \"" + usp_mfr + "\"\n\rVAR ups ups.model \"" + usp_model + "\"\n\rVAR ups ups.status \"" + usp_status + "\"\n\rEND LIST VAR ups\n\r";
              lsups = "VAR ups ups.status \"" + usp_status + "\"\n\r";
            }

            char lsups_buf[lsups.length() + 1] ;
            lsups.toCharArray(lsups_buf, lsups.length() + 1);
            //Serial.println(lsups_buf);
            for (uint8_t k { 0 }; k < max_client; ++k) {
              if (serverClients[k].connected()) {
                serverClients[k].write(lsups_buf, sizeof(lsups_buf));
                ::delay(0);
              }
            }
          }


          if (tmps.indexOf("LOGOUT") != -1) {
            tmps = "";
            Serial.println("捕获");
            char lsups[] = "OK Goodbye\n\r";
            for (uint8_t k { 0 }; k < max_client; ++k) {
              if (serverClients[k].connected()) {
                serverClients[k].write(lsups, sizeof(lsups));
                ::delay(0);
              }
            }
          }

          if (tmps.indexOf("USERNAME monuser") != -1 || tmps.indexOf("PASSWORD secret") != -1) {
            tmps = "";
            Serial.println("捕获");
            //char lsups[] = "ERR UNKNOWN-COMMAND\n\r";
            char lsups[] = "BEGIN LIST UPS\n\rUPS usp \"Description unavailable\"\n\rEND LIST UPS\n\r";

            for (uint8_t k { 0 }; k < max_client; ++k) {
              if (serverClients[k].connected()) {
                serverClients[k].write(lsups, sizeof(lsups));
                ::delay(0);
              }
            }
          }
        }
      }
    }
  }





  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    //char lsups[] = "BEGIN LIST VAR ups\n\rVAR ups battery.charge \"100\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"1440\"\n\rVAR ups battery.type \"PbAc\"\n\rVAR ups device.mfr \"keai\"\n\rVAR ups device.model \"keai NO.1 \"\n\rVAR ups device.serial \"Blank\"\n\rVAR ups device.type \"ups\"\n\rVAR ups driver.name \"usbhid-ups\"\n\rVAR ups driver.parameter.pollfreq \"30\"\n\rVAR ups driver.parameter.pollinterval \"5\"\n\rVAR ups driver.parameter.port \"auto\"\n\rVAR ups driver.version \"DSM6-2-25364-191230\"\n\rVAR ups driver.version.data \"MGE HID 1.33\"\n\rVAR ups driver.version.internal \"0.38\"\n\rVAR ups input.transfer.high \"264\"\n\rVAR ups input.transfer.low \"184\"\n\rVAR ups outlet.1.desc \"PowerShare Outlet 1\"\n\rVAR ups outlet.1.id \"1\"\n\rVAR ups outlet.1.status \"on\"\n\rVAR ups outlet.1.switchable \"no\"\n\rVAR ups outlet.desc \"Main Outlet\"\n\rVAR ups outlet.id \"0\"\n\rVAR ups outlet.switchable \"yes\"\n\rVAR ups output.frequency.nominal \"50\"\n\rVAR ups output.voltage \"230.0\"\n\rVAR ups output.voltage.nominal \"220\"\n\rVAR ups ups.beeper.status \"enabled\"\n\rVAR ups ups.delay.shutdown \"20\"\n\rVAR ups ups.delay.start \"30\"\n\rVAR ups ups.firmware \"02.08.0010\"\n\rVAR ups ups.load \"20\"\n\rVAR ups ups.mfr \"keai\"\n\rVAR ups ups.model \"keai NO.1 \"\n\rVAR ups ups.power.nominal \"850\"\n\rVAR ups ups.productid \"ffff\"\n\rVAR ups ups.serial \"Blank\"\n\rVAR ups ups.status \"OL CHRG\"\n\rVAR ups ups.timer.shutdown \"0\"\n\rVAR ups ups.timer.start \"0\"\n\rVAR ups ups.type \"offline / line interactive\"\n\rVAR ups ups.vendorid \"0463\"\n\rEND LIST VAR ups\n\r";
    String lsups = "BEGIN LIST VAR ups\n\rVAR ups battery.charge \"" + usp_charge + "\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"" + usp_charge_low + "\"\n\rVAR ups battery.type \"PbAc\"\n\rVAR ups device.mfr \"" + usp_mfr + "\"\n\rVAR ups device.model \"" + usp_model + "\"\n\rVAR ups device.type \"ups\"\n\rVAR ups outlet.id \"0\"\n\rVAR ups outlet.switchable \"yes\"\n\rVAR ups ups.delay.shutdown \"20\"\n\rVAR ups ups.delay.start \"30\"\n\rVAR ups ups.load \"20\"\n\rVAR ups ups.mfr \"" + usp_mfr + "\"\n\rVAR ups ups.model \"" + usp_model + "\"\n\rVAR ups ups.status \"" + usp_status + "\"\n\rEND LIST VAR ups\n\r";
    //char lsups[] = "VAR ups ups.status \"OL CHRG\"\n\rVAR ups battery.charge \"100\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"1440\"\n\r";
    //char lsups[] = "VAR ups ups.status \"OL CHRG\"\n\rVAR ups battery.charge \"100\"\n\rVAR ups battery.charge.low \"20\"\n\rVAR ups battery.runtime \"1440\"\n\r";
    char lsups_buf[lsups.length() + 1] ;

    lsups.toCharArray(lsups_buf, lsups.length() + 1);

    //Serial.println(lsups_buf);

    for (uint8_t k { 0 }; k < max_client; ++k) {
      if (serverClients[k].connected()) {
        serverClients[k].write(lsups_buf, sizeof(lsups_buf));
        ::delay(0);
      }
    }
  }

}
